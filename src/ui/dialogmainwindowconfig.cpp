#include "dialogmainwindowconfig.h"
#include "ui_dialogmainwindowconfig.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QShowEvent>

#include "util.h"
#include "environnement.h"
#include "extrequest.h"
#include "strava_oauth_flow.h"
#include "intervalsicuservice.h"
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
#ifndef GC_WASM_BUILD
    if (replyIntervalsTest) {
        replyIntervalsTest->abort();
        replyIntervalsTest->deleteLater();
        replyIntervalsTest = nullptr;
    }
#endif
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

    QListWidgetItem *item1 = new QListWidgetItem(QIcon(":/image/icon/general"), tr("General"), ui->listWidget_settings);
    QListWidgetItem *item3 = new QListWidgetItem(QIcon(":/image/icon/folder"), tr("Folders"), ui->listWidget_settings);
    QListWidgetItem *item4 = new QListWidgetItem(QIcon(":/image/icon/strava_logo"), tr("Strava"), ui->listWidget_settings);
    QListWidgetItem *item5 = new QListWidgetItem(QIcon(":/image/icon/intervals"), tr("Intervals.icu"), ui->listWidget_settings);
    // "Profile" reuses the old main-page profile icon; its page (page_profile)
    // is the last static page in the .ui, so it maps to stacked index 4 and the
    // runtime-added Logging page lands at index 5 — keep this order in sync.
    QListWidgetItem *item6 = new QListWidgetItem(QIcon(":/image/icon/user"), tr("Profile"), ui->listWidget_settings);
    QListWidgetItem *item7 = new QListWidgetItem(QIcon(":/image/icon/gear"), tr("Logging"), ui->listWidget_settings);
    item1->setSizeHint(QSize(35,35));
    item3->setSizeHint(QSize(35,35));
    item4->setSizeHint(QSize(35,35));
    item5->setSizeHint(QSize(35,35));
    item6->setSizeHint(QSize(35,35));
    item7->setSizeHint(QSize(35,35));

    ui->listWidget_settings->addItem(item1);
    ui->listWidget_settings->addItem(item3);
    ui->listWidget_settings->addItem(item4);
    ui->listWidget_settings->addItem(item5);
    ui->listWidget_settings->addItem(item6);
    ui->listWidget_settings->addItem(item7);

    // Add the logging page to the stacked widget (lands at index 5, after the
    // static page_profile at index 4)
    ui->stackedWidget->addWidget(createLoggingPage());


    connect(ui->listWidget_settings, SIGNAL(currentRowChanged(int)), this, SLOT(currentListViewSelectionChanged(int)) );

    ui->listWidget_settings->setCurrentRow(0);

    initUI();

    connect(ui->pushButton_testIntervalsConnection, &QPushButton::clicked,
            this, &DialogMainWindowConfig::onTestIntervalsConnectionClicked);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogMainWindowConfig::initUI() {

    // Strava
    ui->label_stravaUnlink->setText(tr("Unlink"));
    // Light blue that stays readable on both the dark and light Preferences themes.
    ui->label_stravaUnlink->setStyleSheet("background-color : transparent; color : #5a9fd4; text-decoration: underline;");
    connect(ui->label_stravaUnlink, SIGNAL(clicked(bool)), this, SLOT(unlinkStravaClicked()) );

    if (account->strava_access_token != "") {
        stravaLinked(true);
    }
    else {
        stravaLinked(false);
    }

    ui->checkBox_stravaAutoUpload->setChecked(account->strava_auto_upload);
    ui->lineEdit_historyDir->setText(Util::getSystemPathHistory());
    ui->lineEdit_workoutDir->setText(Util::getSystemPathWorkout());
    ui->lineEdit_historyDir->setReadOnly(true);
    ui->lineEdit_workoutDir->setReadOnly(true);

    if (account->distance_in_km)
        ui->comboBox_distance->setCurrentIndex(0);
    else //MPH
        ui->comboBox_distance->setCurrentIndex(1);

    ui->checkBox_forceOnTop->setChecked(account->force_workout_window_on_top);

    ui->comboBox_theme->setCurrentIndex(themeComboIndexFromMode(account->app_theme));

    // Intervals.icu credentials
    ui->lineEdit_intervalsApiKey->setText(account->intervals_icu_api_key);
    ui->lineEdit_intervalsAthleteId->setText(account->intervals_icu_athlete_id);
    ui->checkBox_intervalsAutoUpload->setChecked(account->intervals_icu_auto_upload);
    ui->label_intervalsTestResult->clear();

    // Athlete profile (FTP / LTHR / weight) — edited here, persisted locally.
    ui->spinBox_ftp->setValue(account->FTP);
    ui->spinBox_lthr->setValue(account->LTHR);
    ui->doubleSpinBox_weight->setValue(account->weight_kg);

}



///////////////////////////////////////////////////////////////////////////////////////////////
void DialogMainWindowConfig::stravaLinked(bool linked) {

    qDebug() << "strava linked!";

    if (linked) {
        ui->label_connectStrava->setCursor(Qt::ArrowCursor);
        ui->label_connectStrava->setVisible(false);

        qDebug() << "before disc linked end!";
        disconnect(ui->label_connectStrava, SIGNAL(clicked(bool)), this, SLOT(stravaLabelClicked()) );

        ui->label_stravaUnlink->setVisible(true);
        ui->label_stravaUnlink->setCursor(Qt::PointingHandCursor);
        ui->label_stravaUnlink->fadeIn(1000);

        qDebug() << "after disc linked end!";
    }
    else {
        ui->label_connectStrava->setStyleSheet("image: url(:/image/icon/strava);");
        ui->label_connectStrava->setCursor(Qt::PointingHandCursor);
        ui->label_connectStrava->setVisible(true);
        connect(ui->label_connectStrava, SIGNAL(clicked(bool)), this, SLOT(stravaLabelClicked()) );

        ui->label_stravaUnlink->setVisible(false);
    }
    ui->label_connectStrava->fadeIn(1000);

    qDebug() << "strava linked end!";
}


//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::stravaLabelClicked() {

    // Open the Strava login in the user's system browser and capture the
    // redirect via a localhost loopback listener (Strava blocks embedded views).
    StravaOAuthFlow *flow = new StravaOAuthFlow(this);
    connect(flow, &StravaOAuthFlow::finished, this, [this, flow](bool linked) {
        flow->deleteLater();
        if (linked) {
            stravaLinked(true);
        } else {
            QMessageBox::warning(this, tr("Strava"),
                                 tr("Could not connect to Strava. Please try again."));
        }
    });

    if (!flow->start()) {
        flow->deleteLater();
        QMessageBox::warning(this, tr("Strava"),
                             tr("Could not start the local login listener. "
                                "Please try again."));
    }
}


//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::unlinkStravaClicked() {


    qDebug() << "unlinkStravaClicked";

    replyStravaDeauthorization = ExtRequest::stravaDeauthorization(account->strava_access_token);
    connect(replyStravaDeauthorization, SIGNAL(finished()), this, SLOT(stravaUnlinkFinished()) );
}



//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::stravaUnlinkFinished() {

    qDebug() << "stravaUnlinkFinished";

    //success, process data
    if (replyStravaDeauthorization->error() == QNetworkReply::NoError) {
        qDebug() << "UNlink done success with Strava!";
        account->strava_access_token = "";
        stravaLinked(false);
    }
    else {
        qDebug() << "Error with stravaUnlink" << replyStravaDeauthorization->errorString();
        account->strava_access_token = "";
        stravaLinked(false);
    }
    replyStravaDeauthorization->deleteLater();


}




//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::currentListViewSelectionChanged(int section) {

    qDebug() << "changed!" << section;

    ui->stackedWidget->setCurrentIndex(section);

    //    if (section == 0) {
    //        ui->label_headerSettings->setText(tr("Folders"));
    //    }
    //    if (section == 1) {
    //        ui->label_headerSettings->setText(tr("Units"));
    //    }
    //    else {
    //        ui->label_headerSettings->setText(tr("-"));
    //    }

}


//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::on_pushButton_browseWorkoutDir_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Workout Folder"), Util::getSystemPathWorkout(), QFileDialog::ShowDirsOnly);
    if (path == "")
        return;

    if (Util::checkFolderPathIsValidForWrite(path)) {
        ui->lineEdit_workoutDir->setText(path);
    }
    else {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("The specified folder could not be used to write files in"));
        msgBox.setStandardButtons(QMessageBox::Close);
        msgBox.exec();
    }

}
//----------------------------------------------------------------------------------------
void DialogMainWindowConfig::on_pushButton_browseHistoryDir_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select History Folder"), Util::getSystemPathHistory(), QFileDialog::ShowDirsOnly);
    if (path == "")
        return;

    if (Util::checkFolderPathIsValidForWrite(path)) {
        ui->lineEdit_historyDir->setText(path);
    }
    else {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("The specified folder could not be used to write files in"));
        msgBox.setStandardButtons(QMessageBox::Close);
        msgBox.exec();
    }
}


///////////////////////////////////////////////////////////////////////
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
}

///////////////////////////////////////////////////////////////////////
/// Show where FTP/LTHR came from when the last intervals.icu login synced
/// them (the stamp is cleared when the user edits the values manually).
void DialogMainWindowConfig::updateProfileSyncLabel() {

    const QString syncedAt =
        QSettings().value(QStringLiteral("intervalsIcu/profileSyncedAt")).toString();
    const QDateTime when = QDateTime::fromString(syncedAt, Qt::ISODate);
    ui->label_profileSyncSource->setVisible(when.isValid());
    if (when.isValid())
        ui->label_profileSyncSource->setText(
            tr("FTP and LTHR synced from intervals.icu on %1")
                .arg(when.toString(QStringLiteral("yyyy-MM-dd hh:mm"))));
}


///////////////////////////////////////////////////////////////////////
void DialogMainWindowConfig::reject() {

    qDebug() << "rejected, put back settings value!";

    initUI();

    QDialog::reject();
}


//---------------------------------------------------------------------------
void DialogMainWindowConfig::accept() {
    qDebug() << "ACCEPT, save settings";

    account->strava_auto_upload = ui->checkBox_stravaAutoUpload->isChecked();

    //Folder changed
    if (settings->workoutFolder != ui->lineEdit_workoutDir->text()) {
        settings->workoutFolder = ui->lineEdit_workoutDir->text();
        emit folderWorkoutChanged();
    }

    settings->historyFolder = ui->lineEdit_historyDir->text();
    account->force_workout_window_on_top = ui->checkBox_forceOnTop->isChecked();

    // Athlete profile (FTP / LTHR / weight) — persist locally and notify the
    // main window so dependent metrics (zones, workout targets) recompute.
    const bool profileChangedNow =
        account->FTP       != ui->spinBox_ftp->value() ||
        account->LTHR      != ui->spinBox_lthr->value() ||
        account->weight_kg != ui->doubleSpinBox_weight->value();
    account->saveProfileFields(ui->spinBox_ftp->value(),
                               ui->spinBox_lthr->value(),
                               ui->doubleSpinBox_weight->value());
    // Manually edited values no longer come from intervals.icu — drop the
    // sync stamp so the Preferences label doesn't claim otherwise (the next
    // login sync re-stamps it).
    if (profileChangedNow)
        QSettings().remove(QStringLiteral("intervalsIcu/profileSyncedAt"));

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

    // Intervals.icu credentials — trim whitespace before saving
    const QString newApiKey    = ui->lineEdit_intervalsApiKey->text().trimmed();
    const QString newAthleteId = ui->lineEdit_intervalsAthleteId->text().trimmed();

    const bool intervalsChanged =
        account->intervals_icu_api_key    != newApiKey ||
        account->intervals_icu_athlete_id != newAthleteId;

    account->intervals_icu_api_key    = newApiKey;
    account->intervals_icu_athlete_id = newAthleteId;
    account->intervals_icu_auto_upload = ui->checkBox_intervalsAutoUpload->isChecked();
    account->saveIntervalsIcuCredentials();  // persist to QSettings (fast, no-fail path)
    if (!XmlUtil::saveLocalSaveFile(account)) {
        QMessageBox::warning(this,
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

    if (intervalsChanged)
        emit intervalsIcuCredentialsChanged();

    saveLoggingSettings();

    if (profileChangedNow)
        emit profileChanged();

    QDialog::accept();

    if (themeChanged) {
        QMessageBox::information(
            this,
            tr("Theme Changed"),
            tr("The new theme will be applied the next time you restart MaximumTrainer."));
    }
}



//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::onTestIntervalsConnectionClicked()
{
#ifndef GC_WASM_BUILD
    const QString apiKey    = ui->lineEdit_intervalsApiKey->text().trimmed();
    const QString athleteId = ui->lineEdit_intervalsAthleteId->text().trimmed();

    if (apiKey.isEmpty() || athleteId.isEmpty()) {
        ui->label_intervalsTestResult->setStyleSheet("color: red;");
        ui->label_intervalsTestResult->setText(tr("Please enter both API key and Athlete ID."));
        return;
    }

    // Abort any in-flight test request
    if (replyIntervalsTest) {
        replyIntervalsTest->abort();
        replyIntervalsTest->deleteLater();
        replyIntervalsTest = nullptr;
    }

    // Reuse or create the service object
    if (!m_intervalsService)
        m_intervalsService = new IntervalsIcuService(this);
    m_intervalsService->setCredentials(apiKey, athleteId);

    ui->pushButton_testIntervalsConnection->setEnabled(false);
    // #888 mid-grey stays legible on both the light and dark themes.
    ui->label_intervalsTestResult->setStyleSheet("color: #888;");
    ui->label_intervalsTestResult->setText(tr("Testing…"));

    replyIntervalsTest = m_intervalsService->testConnection();
    connect(replyIntervalsTest, &QNetworkReply::finished,
            this, &DialogMainWindowConfig::onTestIntervalsConnectionFinished);
#else
    ui->label_intervalsTestResult->setStyleSheet("color: #888;");
    ui->label_intervalsTestResult->setText(tr("Not available in the web version."));
#endif
}

//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::onTestIntervalsConnectionFinished()
{
#ifndef GC_WASM_BUILD
    ui->pushButton_testIntervalsConnection->setEnabled(true);

    if (!replyIntervalsTest)
        return;

    if (replyIntervalsTest->error() == QNetworkReply::NoError) {
        const QByteArray data = replyIntervalsTest->readAll();
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "IntervalsIcuService: failed to parse athlete response:"
                       << parseError.errorString();
            ui->label_intervalsTestResult->setStyleSheet("color: red;");
            ui->label_intervalsTestResult->setText(
                tr("✗ Unexpected response from server."));
        } else {
            const QString name = doc.object()["name"].toString();
            if (name.isEmpty())
                qWarning() << "IntervalsIcuService: athlete response missing 'name' field";
            ui->label_intervalsTestResult->setStyleSheet("color: green;");
            ui->label_intervalsTestResult->setText(
                tr("✓ Connected") + (name.isEmpty() ? "" : " — " + name));
        }
    } else {
        ui->label_intervalsTestResult->setStyleSheet("color: red;");
        ui->label_intervalsTestResult->setText(
            tr("✗ Failed: %1").arg(replyIntervalsTest->errorString()));
    }

    replyIntervalsTest->deleteLater();
    replyIntervalsTest = nullptr;
#endif
}

//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::setOnlineMode(bool isOnline)
{
#ifndef GC_WASM_BUILD
    ui->groupBox_intervals->setVisible(isOnline);
#endif
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
    pathRow->addWidget(m_editLogFilePath);
    pathRow->addWidget(m_btnBrowseLog);
    fileLayout->addLayout(pathRow);

    m_btnOpenLog = new QPushButton(tr("Open log file"), fileGroup);
    m_btnOpenLog->setFixedWidth(130);
    fileLayout->addWidget(m_btnOpenLog, 0, Qt::AlignLeft);

    // Platform-specific default path hint
    const QString defaultLogDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString defaultLogPath = defaultLogDir + QStringLiteral("/MaximumTrainer.log");

#if defined(Q_OS_WIN)
    // Double %% is intentional: tr() uses QString::arg() which treats % as
    // a placeholder; %% produces the literal % character in the displayed text,
    // so the user sees the correct Windows environment variable syntax %APPDATA%.
    const QString osHint = tr("Windows default: %%APPDATA%%\\MaximumTrainer\\MaximumTrainer.log\n"
                               "(%1)").arg(defaultLogPath);
#elif defined(Q_OS_MAC)
    const QString osHint = tr("macOS default: ~/Library/Application Support/MaximumTrainer/MaximumTrainer.log\n"
                               "(%1)").arg(defaultLogPath);
#else
    const QString osHint = tr("Linux default: ~/.local/share/MaximumTrainer/MaximumTrainer.log\n"
                               "(%1)").arg(defaultLogPath);
#endif

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
            this, &DialogMainWindowConfig::onOpenLogFileClicked);

    loadLoggingSettings();
    return page;
}

//---------------------------------------------------------------------------------------------
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

//---------------------------------------------------------------------------------------------
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

//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::onLogFileEnabledToggled(bool checked)
{
    if (!m_editLogFilePath) return;
    m_editLogFilePath->setEnabled(checked);
    m_btnBrowseLog->setEnabled(checked);
    m_btnOpenLog->setEnabled(checked);
}

//---------------------------------------------------------------------------------------------
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

//---------------------------------------------------------------------------------------------
void DialogMainWindowConfig::onOpenLogFileClicked()
{
    QString path = m_editLogFilePath->text().trimmed();
    if (path.isEmpty()) {
        // No explicit path set — resolve the same default the Logger would use
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
               + QStringLiteral("/MaximumTrainer.log");
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}
