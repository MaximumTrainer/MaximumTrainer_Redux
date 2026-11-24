#include "dialogmainwindowconfig.h"
#include <QAbstractSpinBox>
#include <QLocale>
#include "setting_keys.h"
#include "ui_dialogmainwindowconfig.h"
#include "asyncdialogs.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFrame>
#include <QDesktopServices>
#include <QFileInfo>
#include <QStandardPaths>
#include <QShowEvent>

#include "util.h"
#include "environnement.h"
#include "extrequest.h"
#include "strava_oauth_flow.h"
#include "xmlutil.h"
#include "logger.h"
#include "apptheme.h"


namespace {
// The Theme combobox is displayed as OS Default / Light / Dark, but
// account->app_theme uses the AppTheme::Mode enum (Light=0, Dark=1, System=2).
// These map between the visible combobox index and the stored enum value.
int themeComboIndexFromMode(int appTheme)
{
    switch (static_cast<AppTheme::Mode>(appTheme)) {
    case AppTheme::Light:  return 1;
    case AppTheme::Dark:   return 2;
    case AppTheme::System: return 0;
    }
    return 0; // default to OS Default
}

int themeModeFromComboIndex(int comboIndex)
{
    switch (comboIndex) {
    case 1:  return AppTheme::Light;
    case 2:  return AppTheme::Dark;
    default: return AppTheme::System; // index 0 == OS Default
    }
}
} // namespace


DialogMainWindowConfig::~DialogMainWindowConfig()
{
    delete ui;
}


DialogMainWindowConfig::DialogMainWindowConfig(QWidget *parent) : QDialog(parent), ui(new Ui::DialogMainWindowConfig)
{
    // Strava login runs in the system browser (StravaOAuthFlow) — Strava blocks
    // embedded webviews. Set up in stravaLabelClicked().

    ui->setupUi(this);


    this->settings = qApp->property("User_Settings").value<Settings*>();
    this->account = qApp->property("Account").value<Account*>();


    /// List widgets
    ui->listWidget_settings->setIconSize(QSize(24, 24));

    // Add the logging page to the stacked widget first (lands at index 4, after
    // the static page_profile at index 3) so the UserRole indices below are valid.
    ui->stackedWidget->addWidget(createLoggingPage());

    // Two groups: app settings on top, third-party integrations under a divider
    // at the bottom (so it reads as "these are external services"). The nav order
    // no longer matches the stacked-page order, so each entry carries its target
    // page index in UserRole — read back in currentListViewSelectionChanged().
    auto addNav = [this](const QString &iconPath, const QString &label, int pageIndex) {
        QListWidgetItem *it = new QListWidgetItem(QIcon(iconPath), label);
        it->setSizeHint(QSize(35, 35));
        it->setData(Qt::UserRole, pageIndex);
        ui->listWidget_settings->addItem(it);
    };

    // App settings
    addNav(":/image/icon/general", tr("General"), 0);
    addNav(":/image/icon/user",    tr("Profile"), 3);
    addNav(":/image/icon/gear",    tr("Logging"), 4);

    // Divider — a non-selectable spacer row mapping to no page.
    QListWidgetItem *sep = new QListWidgetItem(ui->listWidget_settings);
    sep->setFlags(Qt::NoItemFlags);
    sep->setSizeHint(QSize(35, 12));
    QFrame *sepLine = new QFrame();
    sepLine->setFrameShape(QFrame::HLine);
    sepLine->setFrameShadow(QFrame::Sunken);
    ui->listWidget_settings->setItemWidget(sep, sepLine);

    // Integrations (third-party services)
    addNav(":/image/icon/strava_logo", tr("Strava"),        1);
    addNav(":/image/icon/intervals",   tr("Intervals.icu"), 2);


    connect(ui->listWidget_settings, &QListWidget::currentRowChanged, this, &DialogMainWindowConfig::currentListViewSelectionChanged);

    ui->listWidget_settings->setCurrentRow(0);

    initUI();
}


void DialogMainWindowConfig::initUI() {

    // Strava
    ui->label_stravaUnlink->setText(tr("Unlink"));
    // Light blue that stays readable on both the dark and light Preferences themes.
    ui->label_stravaUnlink->setStyleSheet("background-color : transparent; color : #5a9fd4; text-decoration: underline;");
    connect(ui->label_stravaUnlink, &FaderLabel::clicked, this, &DialogMainWindowConfig::unlinkStravaClicked);

    if (account->strava_access_token != "") {
        stravaLinked(true);
    }
    else {
        stravaLinked(false);
    }

    ui->checkBox_stravaAutoUpload->setChecked(account->strava_auto_upload);

    if (account->distance_in_km)
        ui->comboBox_distance->setCurrentIndex(0);
    else //MPH
        ui->comboBox_distance->setCurrentIndex(1);

    ui->checkBox_forceOnTop->setChecked(account->force_workout_window_on_top);
    ui->checkBox_rowingMode->setChecked(account->rowing_mode);

    ui->comboBox_theme->setCurrentIndex(themeComboIndexFromMode(account->app_theme));

    // Intervals.icu credentials
    ui->checkBox_intervalsAutoUpload->setChecked(account->intervals_icu_auto_upload);
    updateIntervalsAvailability();

    // Athlete profile (FTP / LTHR / weight) — edited here, persisted locally.
    ui->spinBox_ftp->setValue(account->FTP);
    ui->spinBox_lthr->setValue(account->LTHR);
    ui->doubleSpinBox_weight->setValue(account->weight_kg);

}


void DialogMainWindowConfig::stravaLinked(bool linked) {

    ui->checkBox_stravaAutoUpload->setEnabled(linked);
    ui->label_stravaConnected->setVisible(linked);

    if (linked) {
        ui->label_connectStrava->setCursor(Qt::ArrowCursor);
        ui->label_connectStrava->setVisible(false);

        disconnect(ui->label_connectStrava, &FaderLabel::clicked, this, &DialogMainWindowConfig::stravaLabelClicked);

        ui->label_stravaUnlink->setVisible(true);
        ui->label_stravaUnlink->setCursor(Qt::PointingHandCursor);
        ui->label_stravaUnlink->fadeIn(1000);

    }
    else {
        ui->label_connectStrava->setStyleSheet("image: url(:/image/icon/strava);");
        ui->label_connectStrava->setCursor(Qt::PointingHandCursor);
        ui->label_connectStrava->setVisible(true);
        connect(ui->label_connectStrava, &FaderLabel::clicked, this, &DialogMainWindowConfig::stravaLabelClicked);

        ui->label_stravaUnlink->setVisible(false);
    }
    ui->label_connectStrava->fadeIn(1000);

}


void DialogMainWindowConfig::stravaLabelClicked() {

    // Open the Strava login in the user's system browser and capture the
    // redirect via a localhost loopback listener (Strava blocks embedded views).
    StravaOAuthFlow *flow = new StravaOAuthFlow(this);
    connect(flow, &StravaOAuthFlow::finished, this, [this, flow](bool linked) {
        flow->deleteLater();
        if (linked) {
            stravaLinked(true);
        } else {
            AsyncDialogs::warning(this, tr("Strava"),
                                  tr("Could not connect to Strava. Please try again."));
        }
    });

    if (!flow->start()) {
        flow->deleteLater();
        AsyncDialogs::warning(this, tr("Strava"),
                              tr("Could not start the local login listener. "
                                 "Please try again."));
    }
}


void DialogMainWindowConfig::unlinkStravaClicked() {


    replyStravaDeauthorization = ExtRequest::stravaDeauthorization(account->strava_access_token);
    connect(replyStravaDeauthorization, &QNetworkReply::finished, this, &DialogMainWindowConfig::stravaUnlinkFinished);
}


void DialogMainWindowConfig::stravaUnlinkFinished() {


    // Unlink locally whether or not Strava acknowledged the deauthorization,
    // and persist now: Cancel on this dialog must not resurrect the link.
    account->strava_access_token.clear();
    account->strava_refresh_token.clear();
    account->strava_token_expires_at = 0;
    account->saveStravaCredentials();
    stravaLinked(false);
    replyStravaDeauthorization->deleteLater();


}


void DialogMainWindowConfig::currentListViewSelectionChanged(int section) {

    // The nav order no longer matches the stacked-page order; each row stores its
    // target page in UserRole. Skip rows without one (e.g. the group divider).
    QListWidgetItem *item = ui->listWidget_settings->item(section);
    if (!item)
        return;
    const QVariant page = item->data(Qt::UserRole);
    if (!page.isValid())
        return;
    ui->stackedWidget->setCurrentIndex(page.toInt());
}


void DialogMainWindowConfig::showEvent(QShowEvent *event) {

    QDialog::showEvent(event);

    // The athlete profile can change outside this dialog — an FTP test or the
    // intervals.icu login sync writes a new FTP/LTHR straight into the account.
    // Re-read those fields on every open so the spin boxes reflect the current
    // values and OK does not clobber a fresh result with the stale value loaded
    // at construction time.
    ui->spinBox_ftp->setValue(account->FTP);
    ui->spinBox_lthr->setValue(account->LTHR);
    ui->doubleSpinBox_weight->setValue(account->weight_kg);
    updateProfileSyncLabel();
    updateIntervalsAvailability();
}

/// Signed in, FTP / LTHR / weight are copied from Intervals.icu at every
/// login, so a local edit would not last: show them read-only and say where to
/// change them. Offline they stay editable.
void DialogMainWindowConfig::updateProfileSyncLabel() {

    const bool fromIntervals = !account->intervals_icu_access_token.isEmpty();
    for (QAbstractSpinBox *box : std::initializer_list<QAbstractSpinBox *>{
             ui->spinBox_ftp, ui->spinBox_lthr, ui->doubleSpinBox_weight}) {
        box->setReadOnly(fromIntervals);
        box->setButtonSymbols(fromIntervals ? QAbstractSpinBox::NoButtons : QAbstractSpinBox::UpDownArrows);
        box->setToolTip(fromIntervals ? tr("From your Intervals.icu profile") : QString());
        box->setStyleSheet(fromIntervals ? QStringLiteral("color: #9a9aa4;") : QString());
    }
    ui->label_profileSyncSource->setVisible(fromIntervals);
    if (!fromIntervals)
        return;

    const QDateTime when = QDateTime::fromString(
        AppSettings::read(SettingKeys::Login::IntervalsIcuProfileSyncedAt), Qt::ISODate);
    QString text = tr("From your Intervals.icu profile");
    if (when.isValid())
        text += tr(", synced %1").arg(QLocale().toString(when, QStringLiteral("d MMM yyyy hh:mm")));
    text += tr(". Change them in <a href=\"https://intervals.icu/settings\">Intervals.icu settings</a>; "
               "they update here at your next sign-in.");
    ui->label_profileSyncSource->setText(text);
}


void DialogMainWindowConfig::reject() {


    initUI();

    QDialog::reject();
}


void DialogMainWindowConfig::accept() {

    account->strava_auto_upload = ui->checkBox_stravaAutoUpload->isChecked();
    account->force_workout_window_on_top = ui->checkBox_forceOnTop->isChecked();
    const bool rowingWas = account->rowing_mode;
    account->rowing_mode = ui->checkBox_rowingMode->isChecked();
    if (rowingWas != account->rowing_mode)
        emit rowingModeChanged();

    // Athlete profile (FTP / LTHR / weight) — persist locally and notify the
    // main window so dependent metrics (zones, workout targets) recompute.
    const bool profileChangedNow = !ui->spinBox_ftp->isReadOnly() && (
        account->FTP       != ui->spinBox_ftp->value() ||
        account->LTHR      != ui->spinBox_lthr->value() ||
        account->weight_kg != ui->doubleSpinBox_weight->value());
    if (profileChangedNow)
        account->saveProfileFields(ui->spinBox_ftp->value(),
                                   ui->spinBox_lthr->value(),
                                   ui->doubleSpinBox_weight->value());
    // Manually edited values no longer come from intervals.icu — drop the
    // sync stamp so the Preferences label doesn't claim otherwise (the next
    // login sync re-stamps it).
    if (profileChangedNow)
        AppSettings::remove(SettingKeys::Login::IntervalsIcuProfileSyncedAt);

    // Persist the theme choice but apply it on next launch only. Live
    // re-styling via qApp->setStyleSheet() does not reliably repolish widgets
    // that carry their own stylesheets (the workout table, message boxes,
    // etc.), so they keep stale colours until recreated. A restart guarantees
    // a clean, consistent result.
    const int newTheme = themeModeFromComboIndex(ui->comboBox_theme->currentIndex());
    bool themeChanged = false;
    if (newTheme != account->app_theme) {
        account->app_theme = newTheme;
        account->saveAppTheme();
        themeChanged = true;
    }

    if (ui->comboBox_distance->currentIndex() == 0)
        account->distance_in_km = true;
    else //MPH
        account->distance_in_km = false;

    account->intervals_icu_auto_upload = ui->checkBox_intervalsAutoUpload->isChecked();
    account->saveIntervalsIcuCredentials();  // persist to QSettings (fast, no-fail path)
    if (!XmlUtil::saveLocalSaveFile(account)) {
        AsyncDialogs::warning(this,
                              tr("Save Failed"),
                              tr("Could not save Intervals.icu credentials to the local file.\n"
                                 "Your settings may not be remembered after the next restart."));
        // Do not close the dialog — let the user correct the situation (e.g.
        // free disk space) or explicitly dismiss.
        return;
    }


    settings->saveGeneralSettings();

    // Persist the preferences edited here locally (the maximumtrainer.com
    // account endpoint is defunct): general/trainer/pairing/upload toggles via
    // saveDisplayPrefs(), and the Strava credentials via the encrypted
    // credential-store saver (covers Strava connect/disconnect).
    account->saveDisplayPrefs();
    account->saveStravaCredentials();

    saveLoggingSettings();

    if (profileChangedNow)
        emit profileChanged();

    QDialog::accept();

    if (themeChanged) {
        // Parented to the main window: this dialog just accept()ed (hidden),
        // and the box outlives it on screen.
        AsyncDialogs::information(
            parentWidget(),
            tr("Theme Changed"),
            tr("The new theme will be applied the next time you restart MaximumTrainer."));
    }
}


void DialogMainWindowConfig::setOnlineMode(bool isOnline)
{
    m_networkOnline = isOnline;
    updateIntervalsAvailability();
}

void DialogMainWindowConfig::updateIntervalsAvailability()
{
    const bool signedIn = !account->intervals_icu_access_token.isEmpty();
    QString reason;
    if (!signedIn)
        reason = tr("Intervals.icu features need you to sign in with Intervals.icu. "
                    "Choose MaximumTrainer → Log Out to return to the sign-in screen.");
    else if (!m_networkOnline)
        reason = tr("Intervals.icu is not available while offline. "
                    "Please check your internet connection.");
    ui->checkBox_intervalsAutoUpload->setEnabled(reason.isEmpty());
    ui->label_intervalsSignIn->setText(reason);
    ui->label_intervalsSignIn->setVisible(!reason.isEmpty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Logging settings page
// ─────────────────────────────────────────────────────────────────────────────

QWidget *DialogMainWindowConfig::createLoggingPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // ── Log level ────────────────────────────────────────────────────────────
    QGroupBox *levelGroup = new QGroupBox(tr("Log Level"), page);
    QFormLayout *levelForm = new QFormLayout(levelGroup);

    m_comboLogLevel = new QComboBox(levelGroup);
    m_comboLogLevel->addItem(tr("Verbose"), static_cast<int>(LogLevel::Verbose));
    m_comboLogLevel->addItem(tr("Debug"),   static_cast<int>(LogLevel::Debug));
    m_comboLogLevel->addItem(tr("Info"),    static_cast<int>(LogLevel::Info));
    m_comboLogLevel->addItem(tr("Warning"), static_cast<int>(LogLevel::Warn));
    m_comboLogLevel->addItem(tr("Error"),   static_cast<int>(LogLevel::Error));
    levelForm->addRow(tr("Minimum level:"), m_comboLogLevel);
    mainLayout->addWidget(levelGroup);

    // ── File logging ─────────────────────────────────────────────────────────
    QGroupBox *fileGroup = new QGroupBox(tr("File Logging"), page);
    QVBoxLayout *fileLayout = new QVBoxLayout(fileGroup);

    m_checkFileLogging = new QCheckBox(tr("Write log to file"), fileGroup);
    fileLayout->addWidget(m_checkFileLogging);

    QHBoxLayout *pathRow = new QHBoxLayout();
    m_editLogFilePath = new QLineEdit(fileGroup);
    m_editLogFilePath->setReadOnly(false);
    m_editLogFilePath->setPlaceholderText(tr("(default path)"));
    m_btnBrowseLog = new QPushButton(tr("Browse…"), fileGroup);
    m_btnBrowseLog->setFixedWidth(90);
#ifdef GC_WASM_BUILD
    // QFileDialog::getSaveFileName spins a fatal nested event loop on WASM,
    // and a browsable log path is meaningless in the in-memory browser FS.
    m_btnBrowseLog->setVisible(false);
#endif
    pathRow->addWidget(m_editLogFilePath);
    pathRow->addWidget(m_btnBrowseLog);
    fileLayout->addLayout(pathRow);

    m_btnOpenLog = new QPushButton(tr("Open log folder"), fileGroup);
    m_btnOpenLog->setFixedWidth(130);
    fileLayout->addWidget(m_btnOpenLog, 0, Qt::AlignLeft);

    // Platform-specific default path hint
    const QString defaultLogDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString defaultLogPath = defaultLogDir + QStringLiteral("/MaximumTrainer.log");

    const QString osHint = tr("Default: %1").arg(QDir::toNativeSeparators(defaultLogPath));

    m_labelLogPathHint = new QLabel(osHint, fileGroup);
    m_labelLogPathHint->setWordWrap(true);
    m_labelLogPathHint->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    fileLayout->addWidget(m_labelLogPathHint);

    mainLayout->addWidget(fileGroup);
    mainLayout->addStretch();

    connect(m_checkFileLogging, &QCheckBox::toggled,
            this, &DialogMainWindowConfig::onLogFileEnabledToggled);
    connect(m_btnBrowseLog, &QPushButton::clicked,
            this, &DialogMainWindowConfig::onBrowseLogFileClicked);
    connect(m_btnOpenLog, &QPushButton::clicked,
            this, &DialogMainWindowConfig::onOpenLogFolderClicked);

    loadLoggingSettings();
    return page;
}

void DialogMainWindowConfig::loadLoggingSettings()
{
    if (!m_comboLogLevel) return;

    // Log level combo
    const int currentLevel = static_cast<int>(Logger::instance().logLevel());
    for (int i = 0; i < m_comboLogLevel->count(); ++i) {
        if (m_comboLogLevel->itemData(i).toInt() == currentLevel) {
            m_comboLogLevel->setCurrentIndex(i);
            break;
        }
    }

    // File logging
    m_checkFileLogging->setChecked(Logger::instance().isFileLoggingEnabled());
    m_editLogFilePath->setText(Logger::instance().logFilePath());

    const bool enabled = m_checkFileLogging->isChecked();
    m_editLogFilePath->setEnabled(enabled);
    m_btnBrowseLog->setEnabled(enabled);
    m_btnOpenLog->setEnabled(enabled);
}

void DialogMainWindowConfig::saveLoggingSettings()
{
    if (!m_comboLogLevel) return;

    const auto newLevel = static_cast<LogLevel>(
        m_comboLogLevel->currentData().toInt());
    Logger::instance().setLogLevel(newLevel);

    const bool fileEnabled = m_checkFileLogging->isChecked();
    const QString filePath = m_editLogFilePath->text().trimmed();
    Logger::instance().setFileLogging(fileEnabled, filePath);
    Logger::instance().saveConfig();
}

void DialogMainWindowConfig::onLogFileEnabledToggled(bool checked)
{
    if (!m_editLogFilePath) return;
    m_editLogFilePath->setEnabled(checked);
    m_btnBrowseLog->setEnabled(checked);
    m_btnOpenLog->setEnabled(checked);
}

void DialogMainWindowConfig::onBrowseLogFileClicked()
{
    const QString current = m_editLogFilePath->text().trimmed();
    const QString suggested = current.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
              + QStringLiteral("/MaximumTrainer.log")
        : current;

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Choose log file location"), suggested,
        tr("Log files (*.log);;All files (*)"));
    if (!path.isEmpty()) {
        m_editLogFilePath->setText(path);
        m_btnOpenLog->setEnabled(true);
    }
}

void DialogMainWindowConfig::onOpenLogFolderClicked()
{
    QString path = m_editLogFilePath->text().trimmed();
    if (path.isEmpty()) {
        // No explicit path set — resolve the same default the Logger would use
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
               + QStringLiteral("/MaximumTrainer.log");
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}
