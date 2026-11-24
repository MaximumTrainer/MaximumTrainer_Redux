#include "settingspage.h"
#include <QAbstractSpinBox>
#include <QLocale>
#include "setting_keys.h"
#include "ui_settingspage.h"
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
#include "dialogconfig.h"
#ifdef GC_HAVE_QTMULTIMEDIA
#include "qtmediaplayer.h"
#endif
#include <QScrollArea>


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

// The pages came from fixed-size dialogs; their filler would leave gaps here.
void collapseSpacers(QLayout *layout)
{
    if (!layout)
        return;
    for (int index = 0; index < layout->count(); ++index) {
        QLayoutItem *item = layout->itemAt(index);
        if (QSpacerItem *spacer = item->spacerItem()) {
            if (spacer->expandingDirections() & Qt::Vertical)
                spacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
        } else if (item->layout()) {
            collapseSpacers(item->layout());
        } else if (item->widget()) {
            collapseSpacers(item->widget()->layout());
        }
    }
    layout->invalidate();
}

void collapseSpacers(QWidget *widget)
{
    collapseSpacers(widget->layout());
}

QGroupBox *titled(const QString &title, QWidget *content)
{
    auto *box = new QGroupBox(title);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(content);
    content->show();
    return box;
}
} // namespace


SettingsPage::~SettingsPage()
{
    delete ui;
}


SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent), ui(new Ui::SettingsPage)
{
    // Strava login runs in the system browser (StravaOAuthFlow) — Strava blocks
    // embedded webviews. Set up in stravaLabelClicked().

    ui->setupUi(this);


    this->settings = qApp->property("User_Settings").value<Settings*>();
    this->account = qApp->property("Account").value<Account*>();


    setAutoFillBackground(true);
    ui->listWidget_settings->setIconSize(QSize(20, 20));
    ui->listWidget_settings->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    ui->listWidget_settings->setFixedWidth(190);

    // The .ui and ride pages are moved into sections below; drop the empty stack first.
    QWidget *generalPage = ui->page_general;
    QWidget *profilePage = ui->page_profile;
    QWidget *stravaPage = ui->page_upload;
    QWidget *intervalsPage = ui->page_cloud_sync;
    while (ui->stackedWidget->count() > 0)
        ui->stackedWidget->removeWidget(ui->stackedWidget->widget(0));

    m_rideConfig = new DialogConfig(Util::loadLocalRadioList(), this, nullptr);
    const QList<QWidget *> ride = m_rideConfig->takePagesForSettings();
#ifdef GC_HAVE_QTMULTIMEDIA
    m_radioPlayer = new QtMediaPlayer(this);
    m_radioPlayer->setVisible(false);
    m_radioPlayer->setRadio(true);
    connect(m_rideConfig, &DialogConfig::signal_connectToRadioUrl, m_radioPlayer, &QtMediaPlayer::openUrlRadio);
    connect(m_rideConfig, &DialogConfig::signal_volumeRadioChanged, m_radioPlayer, &QtMediaPlayer::changeVolume);
    connect(m_rideConfig, &DialogConfig::signal_stopPlayingRadio, m_radioPlayer, &QtMediaPlayer::stop);
    connect(m_radioPlayer, &QtMediaPlayer::playing, m_rideConfig, &DialogConfig::radioStartedPlaying);
    connect(m_radioPlayer, &QtMediaPlayer::paused, m_rideConfig, &DialogConfig::radioStoppedPlaying);
    connect(m_radioPlayer, &QtMediaPlayer::stopped, m_rideConfig, &DialogConfig::radioStoppedPlaying);
#endif

    addGroupTitle(tr("YOU"));
    addSection(Section::Profile, QStringLiteral(":/image/icon/user"), tr("Profile"), {profilePage});
    addSection(Section::Accounts, QStringLiteral(":/image/icon/intervals"), tr("Accounts"),
               {intervalsPage, stravaPage, createAccountPage()});
    addGroupTitle(tr("RIDE"));
    addSection(Section::Ride, QStringLiteral(":/image/icon/workoutMan"), tr("Start && alerts"),
               {ride.at(DialogConfig::GeneralPage), createSensorAlertsGroup()});
    addSection(Section::RideScreen, QStringLiteral(":/image/icon/display"), tr("Ride screen"),
               {ride.at(DialogConfig::WidgetsPage), titled(tr("Graph"), ride.at(DialogConfig::GraphPage))});
    addSection(Section::Sounds, QStringLiteral(":/image/icon/sound"), tr("Sounds"), {ride.at(DialogConfig::SoundsPage)});
    addSection(Section::Media, QStringLiteral(":/image/icon/movie"), tr("Video && radio"),
               {titled(tr("Video"), ride.at(DialogConfig::VideoPage)),
                titled(tr("Radio stations"), ride.at(DialogConfig::RadioPage))}, true);
    addGroupTitle(tr("APP"));
    addSection(Section::General, QStringLiteral(":/image/icon/general"), tr("General"), {generalPage});
    addSection(Section::Logs, QStringLiteral(":/image/icon/gear"), tr("Files && logs"), {createFoldersGroup(), createLoggingPage()});
    addSection(Section::Help, QStringLiteral(":/image/icon/main_icon"), tr("Help && about"), {createHelpPage()});

    connect(ui->listWidget_settings, &QListWidget::currentRowChanged, this, &SettingsPage::currentListViewSelectionChanged);
    showSection(Section::Profile);

    // Everything applies as it changes. clicked / activated / editingFinished
    // only fire on user input, so loading values never saves them back.
    for (QCheckBox *box : {ui->checkBox_stravaAutoUpload, ui->checkBox_forceOnTop, ui->checkBox_rowingMode,
                           ui->checkBox_intervalsAutoUpload, m_checkFileLogging})
        connect(box, &QCheckBox::clicked, this, &SettingsPage::apply);
    for (QComboBox *box : {ui->comboBox_distance, ui->comboBox_theme, m_comboLogLevel})
        connect(box, &QComboBox::activated, this, &SettingsPage::apply);
    for (QAbstractSpinBox *box : std::initializer_list<QAbstractSpinBox *>{
             ui->spinBox_ftp, ui->spinBox_lthr, ui->doubleSpinBox_weight})
        connect(box, &QAbstractSpinBox::editingFinished, this, &SettingsPage::apply);
    connect(m_editLogFilePath, &QLineEdit::editingFinished, this, &SettingsPage::apply);

    initUI();
}


void SettingsPage::initUI() {

    // Strava
    ui->label_stravaUnlink->setText(tr("Unlink"));
    // Light blue that stays readable on both the dark and light Preferences themes.
    ui->label_stravaUnlink->setStyleSheet("background-color : transparent; color : #5a9fd4; text-decoration: underline;");
    connect(ui->label_stravaUnlink, &FaderLabel::clicked, this, &SettingsPage::unlinkStravaClicked, Qt::UniqueConnection);

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

    m_dropoutEnabled->setChecked(account->sensor_dropout_enabled);
    m_dropoutTimeout->setValue(account->sensor_dropout_timeout_s);
    m_batteryThreshold->setValue(account->battery_warning_threshold);
}


void SettingsPage::stravaLinked(bool linked) {

    ui->checkBox_stravaAutoUpload->setEnabled(linked);
    ui->label_stravaConnected->setVisible(linked);

    if (linked) {
        ui->label_connectStrava->setCursor(Qt::ArrowCursor);
        ui->label_connectStrava->setVisible(false);

        disconnect(ui->label_connectStrava, &FaderLabel::clicked, this, &SettingsPage::stravaLabelClicked);

        ui->label_stravaUnlink->setVisible(true);
        ui->label_stravaUnlink->setCursor(Qt::PointingHandCursor);
        ui->label_stravaUnlink->fadeIn(1000);

    }
    else {
        ui->label_connectStrava->setStyleSheet("image: url(:/image/icon/strava);");
        ui->label_connectStrava->setCursor(Qt::PointingHandCursor);
        ui->label_connectStrava->setVisible(true);
        connect(ui->label_connectStrava, &FaderLabel::clicked, this, &SettingsPage::stravaLabelClicked, Qt::UniqueConnection);

        ui->label_stravaUnlink->setVisible(false);
    }
    ui->label_connectStrava->fadeIn(1000);

}


void SettingsPage::stravaLabelClicked() {

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


void SettingsPage::unlinkStravaClicked() {


    replyStravaDeauthorization = ExtRequest::stravaDeauthorization(account->strava_access_token);
    connect(replyStravaDeauthorization, &QNetworkReply::finished, this, &SettingsPage::stravaUnlinkFinished);
}


void SettingsPage::stravaUnlinkFinished() {


    // Unlink locally whether or not Strava acknowledged the deauthorization,
    // and persist now.
    account->strava_access_token.clear();
    account->strava_refresh_token.clear();
    account->strava_token_expires_at = 0;
    account->saveStravaCredentials();
    stravaLinked(false);
    replyStravaDeauthorization->deleteLater();


}


void SettingsPage::currentListViewSelectionChanged(int section) {

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

void SettingsPage::addGroupTitle(const QString &title)
{
    auto *item = new QListWidgetItem(title, ui->listWidget_settings);
    item->setFlags(Qt::NoItemFlags);
    QFont small = font();
    small.setPointSizeF(font().pointSizeF() * 0.8);
    small.setBold(true);
    item->setFont(small);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignBottom);
    item->setSizeHint(QSize(35, ui->listWidget_settings->count() == 1 ? 24 : 38));
}

void SettingsPage::addSection(Section section, const QString &iconPath, const QString &label,
                              const QList<QWidget *> &widgets, bool lastFillsPage)
{
    constexpr int kContentWidth = 720;
    auto *content = new QWidget();
    if (!lastFillsPage)
        content->setMaximumWidth(kContentWidth);
    auto *layout = new QVBoxLayout(content);
    auto *title = new QLabel(QString(label).replace(QStringLiteral("&&"), QStringLiteral("&")), content);
    title->setTextFormat(Qt::PlainText);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);
    for (QWidget *widget : widgets) {
        layout->addWidget(widget);
        collapseSpacers(widget);
        if (widget->layout() && !qobject_cast<QGroupBox *>(widget))
            widget->layout()->setContentsMargins(0, 4, 0, 4);
        widget->show();   // a page taken from a stack is hidden
    }
    if (lastFillsPage)
        layout->setStretchFactor(widgets.constLast(), 1);
    else
        layout->addStretch(1);

    auto *holder = new QWidget();
    auto *row = new QHBoxLayout(holder);
    row->setContentsMargins(8, 0, 8, 0);
    row->addWidget(content, 1);
    row->addStretch(0);
    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(holder);
    scroll->viewport()->setAutoFillBackground(false);
    holder->setAutoFillBackground(false);

    auto *item = new QListWidgetItem(QIcon(iconPath), title->text(), ui->listWidget_settings);
    item->setSizeHint(QSize(35, 34));
    item->setData(Qt::UserRole, ui->stackedWidget->addWidget(scroll));
    item->setData(Qt::UserRole + 1, static_cast<int>(section));
}

void SettingsPage::stopRadio()
{
#ifdef GC_HAVE_QTMULTIMEDIA
    m_radioPlayer->stop();
#endif
}

void SettingsPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (!event->spontaneous())   // minimizing keeps it playing
        stopRadio();
}

void SettingsPage::showSection(Section section)
{
    for (int row = 0; row < ui->listWidget_settings->count(); ++row) {
        const QVariant value = ui->listWidget_settings->item(row)->data(Qt::UserRole + 1);
        if (value.isValid() && value.toInt() == static_cast<int>(section))
            ui->listWidget_settings->setCurrentRow(row);
    }
}

QWidget *SettingsPage::createAccountPage()
{
    auto *page = new QWidget();
    auto *layout = new QHBoxLayout(page);
    m_logoutButton = new QPushButton(tr("Log Out"), page);
    m_logoutButton->setObjectName(QStringLiteral("settingsLogout"));
    connect(m_logoutButton, &QPushButton::clicked, this, &SettingsPage::logoutRequested);
    layout->addWidget(m_logoutButton);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsPage::createSensorAlertsGroup()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *dropout = new QGroupBox(tr("Sensor dropout"), page);
    auto *dropoutLayout = new QVBoxLayout(dropout);
    m_dropoutEnabled = new QCheckBox(tr("Auto-pause workout when sensor signal is lost"), dropout);
    m_dropoutEnabled->setToolTip(tr("Workout resumes automatically 3 seconds after the signal is restored."));
    dropoutLayout->addWidget(m_dropoutEnabled);
    auto *dropoutRow = new QHBoxLayout();
    dropoutRow->addWidget(new QLabel(tr("Dropout timeout:"), dropout));
    m_dropoutTimeout = new QSpinBox(dropout);
    m_dropoutTimeout->setRange(2, 30);
    m_dropoutTimeout->setSuffix(tr(" s"));
    m_dropoutTimeout->setMinimumWidth(80);
    dropoutRow->addWidget(m_dropoutTimeout);
    dropoutRow->addStretch();
    dropoutLayout->addLayout(dropoutRow);
    layout->addWidget(dropout);

    auto *battery = new QGroupBox(tr("Battery warning"), page);
    auto *batteryRow = new QHBoxLayout(battery);
    batteryRow->addWidget(new QLabel(tr("Warn when battery drops below:"), battery));
    m_batteryThreshold = new QSpinBox(battery);
    m_batteryThreshold->setRange(5, 50);
    m_batteryThreshold->setSuffix(tr(" %"));
    m_batteryThreshold->setMinimumWidth(80);
    m_batteryThreshold->setToolTip(tr("A notification is shown when a sensor reports a battery level at or "
                                      "below this threshold."));
    batteryRow->addWidget(m_batteryThreshold);
    batteryRow->addStretch();
    layout->addWidget(battery);

    const auto saveDropout = [this]() {
        account->sensor_dropout_enabled = m_dropoutEnabled->isChecked();
        account->sensor_dropout_timeout_s = m_dropoutTimeout->value();
        account->saveSensorDropoutSettings();
    };
    connect(m_dropoutEnabled, &QCheckBox::clicked, this, saveDropout);
    connect(m_dropoutTimeout, &QSpinBox::editingFinished, this, saveDropout);
    connect(m_batteryThreshold, &QSpinBox::editingFinished, this, [this]() {
        account->battery_warning_threshold = m_batteryThreshold->value();
        account->saveBatteryWarningThreshold();
    });
    return page;
}

QWidget *SettingsPage::createFoldersGroup()
{
    auto *box = new QGroupBox(tr("Folders"));
    auto *row = new QHBoxLayout(box);
    const auto addButton = [&](const QString &label, const QString &toolTip, void (SettingsPage::*signal)()) {
        auto *button = new QPushButton(label, box);
        button->setToolTip(toolTip);
        connect(button, &QPushButton::clicked, this, signal);
        row->addWidget(button);
    };
    addButton(tr("Rides"), tr("Open the folder that holds your FIT files"), &SettingsPage::ridesFolderRequested);
    addButton(tr("Workouts"), tr("Open the workout folder"), &SettingsPage::workoutsFolderRequested);
    row->addStretch(1);
#ifdef GC_WASM_BUILD
    box->setEnabled(false);   // no local folders in the browser
#endif
    return box;
}

QWidget *SettingsPage::createHelpPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    const auto addButton = [&](const QString &label, void (SettingsPage::*signal)()) {
        auto *button = new QPushButton(label, page);
        button->setFixedWidth(220);
        connect(button, &QPushButton::clicked, this, signal);
        layout->addWidget(button);
    };
    addButton(tr("User guide"), &SettingsPage::userGuideRequested);
    addButton(tr("Keyboard shortcuts"), &SettingsPage::keyboardShortcutsRequested);
    addButton(tr("About MaximumTrainer"), &SettingsPage::aboutRequested);
    return page;
}


void SettingsPage::showEvent(QShowEvent *event) {

    QWidget::showEvent(event);
    initUI();
    loadLoggingSettings();
    m_rideConfig->reloadFromAccount();

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
void SettingsPage::updateProfileSyncLabel() {

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


void SettingsPage::apply() {

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

    if (themeChanged) {
        AsyncDialogs::information(
            this,
            tr("Theme Changed"),
            tr("The new theme will be applied the next time you restart MaximumTrainer."));
    }
}


void SettingsPage::setOnlineMode(bool isOnline)
{
    m_networkOnline = isOnline;
    updateIntervalsAvailability();
}

void SettingsPage::updateIntervalsAvailability()
{
    const bool signedIn = !account->intervals_icu_access_token.isEmpty();
    QString reason;
    if (!signedIn)
        reason = tr("Intervals.icu features need you to sign in with Intervals.icu. "
                    "Choose Log Out below to return to the sign-in screen.");
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

QWidget *SettingsPage::createLoggingPage()
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
            this, &SettingsPage::onLogFileEnabledToggled);
    connect(m_btnBrowseLog, &QPushButton::clicked,
            this, &SettingsPage::onBrowseLogFileClicked);
    connect(m_btnOpenLog, &QPushButton::clicked,
            this, &SettingsPage::onOpenLogFolderClicked);

    loadLoggingSettings();
    return page;
}

void SettingsPage::loadLoggingSettings()
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

void SettingsPage::saveLoggingSettings()
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

void SettingsPage::onLogFileEnabledToggled(bool checked)
{
    if (!m_editLogFilePath) return;
    m_editLogFilePath->setEnabled(checked);
    m_btnBrowseLog->setEnabled(checked);
    m_btnOpenLog->setEnabled(checked);
}

void SettingsPage::onBrowseLogFileClicked()
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
        apply();
    }
}

void SettingsPage::onOpenLogFolderClicked()
{
    QString path = m_editLogFilePath->text().trimmed();
    if (path.isEmpty()) {
        // No explicit path set — resolve the same default the Logger would use
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
               + QStringLiteral("/MaximumTrainer.log");
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}
