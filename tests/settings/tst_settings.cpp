// Persistence of Account's displayPrefs/ group through the typed setting keys.
// The expected defaults and key names are the values Account used before the
// keys moved into SettingKeys::DisplayPrefs; they are on-disk/user-visible
// behaviour, so they are spelled out here rather than read from the registry.
#include <QtTest>
#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "account.h"
#include "setting_keys.h"

class TstSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void testDisplayPrefs_freshStoreGivesDefaults();
    void testDisplayPrefs_saveUsesOnDiskKeyNames();
    void testDisplayPrefs_roundTripsEveryField();
    void testUiStateKeys_onDiskNamesAndDefaults();
    void testUiStateKeys_roundTripValueTypes();
    void testWorkoutBrowserHomePage_resetsOnceThenKeepsUserChoice();

private:
    QTemporaryDir m_dir;
};

void TstSettings::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("MaximumTrainerSettingsTest"));
    QCoreApplication::setApplicationName(QStringLiteral("SettingsTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
}

void TstSettings::init()
{
    QSettings settings;
    settings.clear();
    settings.sync();
}

void TstSettings::testDisplayPrefs_freshStoreGivesDefaults()
{
    const Account account;
        QCOMPARE(account.show_hr_widget, true);
        QCOMPARE(account.show_power_widget, true);
        QCOMPARE(account.show_power_balance_widget, true);
        QCOMPARE(account.show_cadence_widget, true);
        QCOMPARE(account.show_speed_widget, false);
        QCOMPARE(account.show_calories_widget, false);
        QCOMPARE(account.show_oxygen_widget, false);
        QCOMPARE(account.show_trainer_speed, false);
        QCOMPARE(account.display_hr, 1);
        QCOMPARE(account.display_power, 2);
        QCOMPARE(account.display_power_balance, 1);
        QCOMPARE(account.display_cadence, 1);
        QCOMPARE(account.display_video, 0);
        QCOMPARE(account.start_trigger, 0);
        QCOMPARE(account.value_cadence_start, 40);
        QCOMPARE(account.value_power_start, 120);
        QCOMPARE(account.value_speed_start, 20);
        QCOMPARE(account.last_index_selected_config_workout, 0);
        QCOMPARE(account.control_trainer_resistance, true);
        QCOMPARE(account.virtual_shifting, false);
        QCOMPARE(account.use_zwift_click, false);
        QCOMPARE(account.enable_studio_mode, false);
        QCOMPARE(account.distance_in_km, true);
        QCOMPARE(account.force_workout_window_on_top, false);
        QCOMPARE(account.strava_auto_upload, false);
        QCOMPARE(account.nb_user_studio, 3);
        QCOMPARE(account.use_pm_for_cadence, false);
        QCOMPARE(account.use_pm_for_speed, false);
        QCOMPARE(account.show_included_workout, true);
        QCOMPARE(account.rowing_mode, false);
        QCOMPARE(account.show_interval_remaining, true);
        QCOMPARE(account.show_workout_remaining, false);
        QCOMPARE(account.show_elapsed, true);
        QCOMPARE(account.show_current_target, false);
        QCOMPARE(account.show_seperator_interval, true);
        QCOMPARE(account.show_grid, false);
        QCOMPARE(account.show_hr_target, true);
        QCOMPARE(account.show_power_target, true);
        QCOMPARE(account.show_cadence_target, true);
        QCOMPARE(account.show_speed_target, true);
        QCOMPARE(account.show_hr_curve, true);
        QCOMPARE(account.show_power_curve, true);
        QCOMPARE(account.show_cadence_curve, true);
        QCOMPARE(account.show_speed_curve, true);
        QCOMPARE(account.sound_player_vol, 100);
        QCOMPARE(account.enable_sound, true);
        QCOMPARE(account.sound_interval, true);
        QCOMPARE(account.sound_pause_resume_workout, true);
        QCOMPARE(account.sound_achievement, true);
        QCOMPARE(account.sound_end_workout, true);
        QCOMPARE(account.sound_alert_power_under_target, false);
        QCOMPARE(account.sound_alert_power_above_target, false);
        QCOMPARE(account.sound_alert_cadence_under_target, false);
        QCOMPARE(account.sound_alert_cadence_above_target, false);
}

void TstSettings::testDisplayPrefs_saveUsesOnDiskKeyNames()
{
    Account account;
    account.saveDisplayPrefs();

    const QSettings settings;
    const char *const names[] = {
        "show_hr_widget",
        "show_power_widget",
        "show_power_balance_widget",
        "show_cadence_widget",
        "show_speed_widget",
        "show_calories_widget",
        "show_oxygen_widget",
        "show_trainer_speed",
        "display_hr",
        "display_power",
        "display_power_balance",
        "display_cadence",
        "display_video",
        "start_trigger",
        "value_cadence_start",
        "value_power_start",
        "value_speed_start",
        "last_index_selected_config_workout",
        "control_trainer_resistance",
        "virtual_shifting",
        "use_zwift_click",
        "enable_studio_mode",
        "distance_in_km",
        "force_workout_window_on_top",
        "strava_auto_upload",
        "nb_user_studio",
        "use_pm_for_cadence",
        "use_pm_for_speed",
        "show_included_workout",
        "rowing_mode",
        "show_interval_remaining",
        "show_workout_remaining",
        "show_elapsed",
        "show_current_target",
        "show_seperator_interval",
        "show_grid",
        "show_hr_target",
        "show_power_target",
        "show_cadence_target",
        "show_speed_target",
        "show_hr_curve",
        "show_power_curve",
        "show_cadence_curve",
        "show_speed_curve",
        "sound_player_vol",
        "enable_sound",
        "sound_interval",
        "sound_pause_resume_workout",
        "sound_achievement",
        "sound_end_workout",
        "sound_alert_power_under_target",
        "sound_alert_power_above_target",
        "sound_alert_cadence_under_target",
        "sound_alert_cadence_above_target"
    };
    for (const char *name : names)
        QVERIFY2(settings.contains(QStringLiteral("displayPrefs/") + QLatin1String(name)), name);
}

void TstSettings::testDisplayPrefs_roundTripsEveryField()
{
    Account writer;
        writer.show_hr_widget = false;
        writer.show_power_widget = false;
        writer.show_power_balance_widget = false;
        writer.show_cadence_widget = false;
        writer.show_speed_widget = false;
        writer.show_calories_widget = false;
        writer.show_oxygen_widget = false;
        writer.show_trainer_speed = false;
        writer.display_hr = 8;
        writer.display_power = 9;
        writer.display_power_balance = 8;
        writer.display_cadence = 8;
        writer.display_video = 7;
        writer.start_trigger = 7;
        writer.value_cadence_start = 47;
        writer.value_power_start = 127;
        writer.value_speed_start = 27;
        writer.last_index_selected_config_workout = 7;
        writer.control_trainer_resistance = false;
        writer.virtual_shifting = true;
        writer.use_zwift_click = true;
        writer.enable_studio_mode = true;
        writer.distance_in_km = false;
        writer.force_workout_window_on_top = true;
        writer.strava_auto_upload = true;
        writer.nb_user_studio = 10;
        writer.use_pm_for_cadence = true;
        writer.use_pm_for_speed = true;
        writer.show_included_workout = false;
        writer.rowing_mode = true;
        writer.show_interval_remaining = false;
        writer.show_workout_remaining = true;
        writer.show_elapsed = false;
        writer.show_current_target = false;
        writer.show_seperator_interval = false;
        writer.show_grid = true;
        writer.show_hr_target = false;
        writer.show_power_target = false;
        writer.show_cadence_target = false;
        writer.show_speed_target = false;
        writer.show_hr_curve = false;
        writer.show_power_curve = false;
        writer.show_cadence_curve = false;
        writer.show_speed_curve = false;
        writer.sound_player_vol = 107;
        writer.enable_sound = false;
        writer.sound_interval = false;
        writer.sound_pause_resume_workout = false;
        writer.sound_achievement = false;
        writer.sound_end_workout = false;
        writer.sound_alert_power_under_target = true;
        writer.sound_alert_power_above_target = true;
        writer.sound_alert_cadence_under_target = true;
        writer.sound_alert_cadence_above_target = true;
    writer.saveDisplayPrefs();

    const Account reader;
        QCOMPARE(reader.show_hr_widget, false);
        QCOMPARE(reader.show_power_widget, false);
        QCOMPARE(reader.show_power_balance_widget, false);
        QCOMPARE(reader.show_cadence_widget, false);
        QCOMPARE(reader.show_speed_widget, false);
        QCOMPARE(reader.show_calories_widget, false);
        QCOMPARE(reader.show_oxygen_widget, false);
        QCOMPARE(reader.show_trainer_speed, false);
        QCOMPARE(reader.display_hr, 8);
        QCOMPARE(reader.display_power, 9);
        QCOMPARE(reader.display_power_balance, 8);
        QCOMPARE(reader.display_cadence, 8);
        QCOMPARE(reader.display_video, 7);
        QCOMPARE(reader.start_trigger, 7);
        QCOMPARE(reader.value_cadence_start, 47);
        QCOMPARE(reader.value_power_start, 127);
        QCOMPARE(reader.value_speed_start, 27);
        QCOMPARE(reader.last_index_selected_config_workout, 7);
        QCOMPARE(reader.control_trainer_resistance, false);
        QCOMPARE(reader.virtual_shifting, true);
        QCOMPARE(reader.use_zwift_click, true);
        QCOMPARE(reader.enable_studio_mode, true);
        QCOMPARE(reader.distance_in_km, false);
        QCOMPARE(reader.force_workout_window_on_top, true);
        QCOMPARE(reader.strava_auto_upload, true);
        QCOMPARE(reader.nb_user_studio, 10);
        QCOMPARE(reader.use_pm_for_cadence, true);
        QCOMPARE(reader.use_pm_for_speed, true);
        QCOMPARE(reader.show_included_workout, false);
        QCOMPARE(reader.rowing_mode, true);
        QCOMPARE(reader.show_interval_remaining, false);
        QCOMPARE(reader.show_workout_remaining, true);
        QCOMPARE(reader.show_elapsed, false);
        QCOMPARE(reader.show_current_target, false);
        QCOMPARE(reader.show_seperator_interval, false);
        QCOMPARE(reader.show_grid, true);
        QCOMPARE(reader.show_hr_target, false);
        QCOMPARE(reader.show_power_target, false);
        QCOMPARE(reader.show_cadence_target, false);
        QCOMPARE(reader.show_speed_target, false);
        QCOMPARE(reader.show_hr_curve, false);
        QCOMPARE(reader.show_power_curve, false);
        QCOMPARE(reader.show_cadence_curve, false);
        QCOMPARE(reader.show_speed_curve, false);
        QCOMPARE(reader.sound_player_vol, 107);
        QCOMPARE(reader.enable_sound, false);
        QCOMPARE(reader.sound_interval, false);
        QCOMPARE(reader.sound_pause_resume_workout, false);
        QCOMPARE(reader.sound_achievement, false);
        QCOMPARE(reader.sound_end_workout, false);
        QCOMPARE(reader.sound_alert_power_under_target, true);
        QCOMPARE(reader.sound_alert_power_above_target, true);
        QCOMPARE(reader.sound_alert_cadence_under_target, true);
        QCOMPARE(reader.sound_alert_cadence_above_target, true);
}

template <typename T>
static void checkKey(const SettingKey<T> &key, const char *expectedPath, const T &expectedDefault)
{
    QCOMPARE(QString::fromLatin1(key.path), QString::fromLatin1(expectedPath));
    QCOMPARE(key.defaultValue, expectedDefault);
}

void TstSettings::testUiStateKeys_onDiskNamesAndDefaults()
{
    using namespace SettingKeys;
    checkKey(MainWindow::Size,      "MainWindow/size",      QSize(1150, 700));
    checkKey(MainWindow::Pos,       "MainWindow/pos",       QPoint(200, 200));
    checkKey(MainWindow::Maximized, "MainWindow/maximized", false);
    checkKey(WorkoutDialog::Size,       "WorkoutDialog/size",       QSize(1350, 800));
    checkKey(WorkoutDialog::Pos,        "WorkoutDialog/pos",        QPoint(100, 100));
    checkKey(WorkoutDialog::Splitter0,  "WorkoutDialog/splitter0",  2);
    checkKey(WorkoutDialog::Splitter1,  "WorkoutDialog/splitter1",  35);
    checkKey(WorkoutDialog::Splitter2,  "WorkoutDialog/splitter2",  39);
    checkKey(WorkoutDialog::Splitter3,  "WorkoutDialog/splitter3",  24);
    checkKey(WorkoutDialog::Fullscreen, "WorkoutDialog/fullscreen", false);
    checkKey(ActivityDetailDialog::Geometry, "activityDetailDialog/geometry", QByteArray());
    checkKey(WorkoutFilter::Name,    "Filter/nameFilter",    QString());
    checkKey(WorkoutFilter::Plan,    "Filter/planFilter",    QString());
    checkKey(WorkoutFilter::Creator, "Filter/creatorFilter", QString());
    checkKey(WorkoutFilter::Type,    "Filter/typeFilter",    -1);
    checkKey(WorkoutQueue::FilePaths, "workoutQueue/filePaths", QStringList());
    checkKey(WorkoutQueue::Names,     "workoutQueue/names",     QStringList());
    checkKey(LastPath::WorkoutImport, "Importer/loadPath",      QString());
    checkKey(LastPath::Video,         "videoPlayer/loadPath",   QString());
    checkKey(LastPath::ReportExport,  "reports/savePathExport", QString());
    checkKey(Media::VideoVolume,      "videoPlayer/soundVolume", 100);
    checkKey(Media::RadioVolume,      "radioPlayer/volume",      100);
    checkKey(Media::LastRadioStation, "radioPlayer/lastStation", QString());
    checkKey(Media::RadioDefaultsHash, "radios/defaultsHash",    QString());
    checkKey(Media::WorkoutBrowserHomePage, "webBrowserWorkout/defaultUrl", QStringLiteral("https://www.youtube.com"));
    checkKey(Media::WorkoutBrowserYoutubeResetDone, "webBrowserWorkout/youtubeResetDone", false);
    checkKey(Login::WorkOffline,        "login/workOffline", false);
    checkKey(Login::LastLoggedUsername, "SettingsGeneral/lastLoggedUsername", QString());
    checkKey(Login::LastLoggedKey,      "SettingsGeneral/lastLoggedKey",      QString());
    checkKey(Login::IntervalsIcuProfileSyncedAt, "intervalsIcu/profileSyncedAt", QString());
    checkKey(Group::RelayUrl, "group_relay_url", QString());
}

void TstSettings::testUiStateKeys_roundTripValueTypes()
{
    {
        QSettings settings;
        AppSettings::write(settings, SettingKeys::MainWindow::Size, QSize(1600, 900));
        AppSettings::write(settings, SettingKeys::MainWindow::Pos, QPoint(12, 34));
        AppSettings::write(settings, SettingKeys::WorkoutQueue::FilePaths, QStringList{QStringLiteral("/a.workout"), QStringLiteral("/b.workout")});
        AppSettings::write(settings, SettingKeys::ActivityDetailDialog::Geometry, QByteArray("\x01\x02geometry", 10));
    }
    const QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("MainWindow/size")).toSize(), QSize(1600, 900));
    QCOMPARE(AppSettings::read(settings, SettingKeys::MainWindow::Size), QSize(1600, 900));
    QCOMPARE(AppSettings::read(settings, SettingKeys::MainWindow::Pos), QPoint(12, 34));
    QCOMPARE(settings.value(QStringLiteral("workoutQueue/filePaths")).toStringList(),
             (QStringList{QStringLiteral("/a.workout"), QStringLiteral("/b.workout")}));
    QCOMPARE(AppSettings::read(settings, SettingKeys::WorkoutQueue::FilePaths).size(), 2);
    QCOMPARE(AppSettings::read(settings, SettingKeys::ActivityDetailDialog::Geometry), QByteArray("\x01\x02geometry", 10));
    QCOMPARE(AppSettings::read(settings, SettingKeys::LastPath::WorkoutImport, QStringLiteral("/home/x")), QStringLiteral("/home/x"));
}

void TstSettings::testWorkoutBrowserHomePage_resetsOnceThenKeepsUserChoice()
{
    {
        QSettings settings;
        settings.setValue(QStringLiteral("webBrowserWorkout/defaultUrl"), QStringLiteral("https://www.netflix.com"));
    }
    QCOMPARE(AppSettings::readWorkoutBrowserHomePage(), QStringLiteral("https://www.youtube.com"));
    QVERIFY(QSettings().value(QStringLiteral("webBrowserWorkout/youtubeResetDone")).toBool());

    AppSettings::write(SettingKeys::Media::WorkoutBrowserHomePage, QStringLiteral("https://example.org"));
    QCOMPARE(AppSettings::readWorkoutBrowserHomePage(), QStringLiteral("https://example.org"));
}

QTEST_GUILESS_MAIN(TstSettings)
#include "tst_settings.moc"
