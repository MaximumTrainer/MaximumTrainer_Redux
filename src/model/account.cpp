#include "account.h"
#include "credential_store.h"
#include <QDebug>


Account::~Account() {
    qDebug() << "Destructor account";
}



Account::Account(QObject *parent) : QObject(parent)  {

    // -------------- Set Default values
    id = -1;
    subscription_type_id = 1;

    email = "email";
    password = "pw";
    session_mt_id = "session_mt_id";
    session_mt_expire = QDateTime::currentDateTime().addDays(1);

    first_name = "first_name";
    last_name = "last_name";
    display_name = "display_name";

    FTP = 150;
    LTHR = 150;
    minutes_rode = 0;
    weight_kg = 70;
    weight_in_kg = true;
    height_cm = 170;

    powerCurve = PowerCurve();
    wheel_circ = 2100;
    bike_weight_kg = 9;
    bike_type = 2;


    userCda = 0.35;

    //-- no more DB settings, move them here later
    QSettings settings;
    settings.beginGroup("account");
    nb_sec_show_interval = settings.value("nb_sec_show_interval", 5 ).toInt();
    nb_sec_show_interval_before = settings.value("nb_sec_show_interval_before", 4 ).toInt();
    erg_smoothing_duration_s = settings.value("erg_smoothing_duration_s", 5).toInt();
    intervals_icu_api_key     = settings.value("intervals_icu_api_key", "").toString();
    intervals_icu_athlete_id  = settings.value("intervals_icu_athlete_id", "").toString();
    intervals_icu_auto_upload = settings.value("intervals_icu_auto_upload", false).toBool();
    // OAuth2 tokens — loaded to restore an existing OAuth session across restarts.
    intervals_icu_access_token  = settings.value("intervals_icu_access_token",  "").toString();
    intervals_icu_refresh_token = settings.value("intervals_icu_refresh_token", "").toString();
    settings.endGroup();

    // Load encrypted third-party service credentials from the platform credential store.
    strava_access_token         = CredentialStore::load("strava",        "access_token");
    strava_refresh_token        = CredentialStore::load("strava",        "refresh_token");
    training_peaks_access_token  = CredentialStore::load("trainingpeaks", "access_token");
    training_peaks_refresh_token = CredentialStore::load("trainingpeaks", "refresh_token");
    selfloops_user              = CredentialStore::load("selfloops",     "email");
    selfloops_pw                = CredentialStore::load("selfloops",     "password");




    // -----------------------------------  Settings ----------------------------------------------------------------------
    nb_ant_stick = 1;
    nb_user_studio = 3;
    enable_studio_mode = false;
    use_pm_for_cadence = false;
    use_pm_for_speed = false;


    force_workout_window_on_top = false;
    show_included_workout = true;
    show_included_course = true;
    distance_in_km = true;
    strava_private_upload = false;
    training_peaks_public_upload = false;
    intervals_icu_auto_upload = false;
    control_trainer_resistance = true;
    stop_pairing_on_found = true;
    nb_sec_pairing = 2;
    erg_smoothing_duration_s = 5;
    /* ----- */

    last_index_selected_config_workout = 0;
    last_tab_sub_config_selected = 0;
    tab_display[0] = "Timer";
    tab_display[1] = "Power";
    tab_display[2] = "Cadence";
    tab_display[3] = "PowerBal";
    tab_display[4] = "Hr";
    tab_display[5] = "Speed";
    tab_display[6] = "InfoWorkout";
    tab_display[7] = "Oxygen";


    start_trigger = 0;
    value_cadence_start = 40;
    value_power_start = 120;
    value_speed_start = 20;


    show_hr_widget = true;
    show_power_widget = true;
    show_power_balance_widget = true;
    show_cadence_widget = true;
    show_speed_widget = true;
    show_calories_widget = true;
    show_oxygen_widget = true;
    use_virtual_speed = true;
    show_trainer_speed = true;

    display_hr = 1;
    display_power = 2;
    display_power_balance = 1;
    display_cadence = 1;

    show_timer_on_top = false;
    show_interval_remaining = true;
    show_workout_remaining = false;
    show_elapsed = true;
    font_size_timer = 26;

    averaging_power = 2;
    offset_power = 0;



    show_seperator_interval = true;
    show_grid = false;
    show_hr_target = true;
    show_power_target = true;
    show_cadence_target = true;
    show_speed_target = true;
    show_hr_curve = true;
    show_power_curve = true;
    show_cadence_curve = true;
    show_speed_curve = true;
    display_video = 0;

    /* ----- */
    sound_player_vol = 100;
    enable_sound = true;
    sound_interval = true;
    sound_pause_resume_workout = true;
    sound_achievement = true;
    sound_end_workout = true;

    sound_alert_power_under_target = false;
    sound_alert_power_above_target = false;
    sound_alert_cadence_under_target = false;
    sound_alert_cadence_above_target = false;


    //-------------------------- not in DB ----------------------
    isOffline = false;
#ifdef Q_OS_WIN32
    os = "win";
#endif
#ifdef Q_OS_MAC
    os = "mac";
#endif

    email_clean = "user1";
    hashWorkoutDone = QSet<QString>();
    hashCourseDone = QSet<QString>();
    //------------------------------

}


void Account::saveNbSecShowInterval(int nbSec) {

    nb_sec_show_interval = nbSec;

    QSettings settings;

    settings.beginGroup("account");
    settings.setValue("nb_sec_show_interval", nb_sec_show_interval);

    settings.endGroup();
}

void Account::saveNbSecShowIntervalBefore(int nbSec) {

    nb_sec_show_interval_before = nbSec;

    QSettings settings;

    settings.beginGroup("account");
    settings.setValue("nb_sec_show_interval_before", nb_sec_show_interval_before);

    settings.endGroup();
}

void Account::saveErgSmoothingDuration(int seconds)
{
    erg_smoothing_duration_s = qBound(0, seconds, 30);

    QSettings settings;
    settings.beginGroup("account");
    settings.setValue("erg_smoothing_duration_s", erg_smoothing_duration_s);
    settings.endGroup();
}

void Account::saveIntervalsIcuCredentials() {

    QSettings settings;

    settings.beginGroup("account");
    settings.setValue("intervals_icu_api_key",    intervals_icu_api_key);
    settings.setValue("intervals_icu_athlete_id", intervals_icu_athlete_id);
    settings.setValue("intervals_icu_auto_upload", intervals_icu_auto_upload);
    // OAuth2 tokens — persisted so they survive an app restart.
    settings.setValue("intervals_icu_access_token",  intervals_icu_access_token);
    settings.setValue("intervals_icu_refresh_token", intervals_icu_refresh_token);
    settings.endGroup();
}

void Account::saveStravaCredentials()
{
    CredentialStore::store("strava", "access_token",  strava_access_token);
    CredentialStore::store("strava", "refresh_token", strava_refresh_token);
}

void Account::saveTrainingPeaksCredentials()
{
    CredentialStore::store("trainingpeaks", "access_token",  training_peaks_access_token);
    CredentialStore::store("trainingpeaks", "refresh_token", training_peaks_refresh_token);
}

void Account::saveSelfloopsCredentials()
{
    CredentialStore::store("selfloops", "email",    selfloops_user);
    CredentialStore::store("selfloops", "password", selfloops_pw);
}




