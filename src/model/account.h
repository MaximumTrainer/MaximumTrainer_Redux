#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QtCore>


//We use same syntax as mySql, so no camel-case here



class Account : public QObject
{
    Q_OBJECT

public:
    ~Account();
    Account(QObject *parent = 0);

    void saveNbSecShowInterval(int nbSec);
    void saveNbSecShowIntervalBefore(int nbSec);
    void saveErgSmoothingDuration(int seconds);
    /// Persist athlete profile (FTP / LTHR / weight) locally so it survives a
    /// restart and offline use. Edited from the Preferences dialog.
    void saveProfileFields(int ftp, int lthr, double weightKg);
    void saveIntervalsIcuCredentials();
    /// Sign out: clear the Intervals.icu identity (OAuth tokens, manual API key
    /// and athlete id) from memory, the encrypted credential store and QSettings.
    /// The next launch shows the login screen again.
    void logout();
    void saveSensorDropoutSettings();
    void saveBatteryWarningThreshold();
    void saveIntervalSummarySettings();
    void saveAppTheme();

    /// Display & sound preferences (video player, widget displays, target/curve
    /// toggles, sound alerts). These were historically stored only server-side
    /// via the now-defunct putAccount REST endpoint, so they were lost on every
    /// restart (especially offline). These persist them to local QSettings.
    void loadDisplayPrefs();
    void saveDisplayPrefs();

    /// Encrypt and persist Strava OAuth2 tokens to QSettings.
    /// Call after a successful token exchange or refresh.
    void saveStravaCredentials();


    int id;
    int subscription_type_id;  //1 = Free, 2= Regular, 3=Studio

    QString email;
    QString password;
    QString session_mt_id;
    QDateTime session_mt_expire;

    QString first_name;
    QString last_name;
    QString display_name;


    int FTP;
    int LTHR;
    int minutes_rode;
    double weight_kg;
    bool weight_in_kg;
    int height_cm;

    int wheel_circ;
    double bike_weight_kg;
    int bike_type;


    //-------------------------- not in DB ----------------------
    bool isOffline;  ///< true when the user logged in via offline (local) mode
    double userCda;
    QString os;

    QString email_clean; //"blais.maxime@gmail.com --> blaismaxime"
    QSet<QString> hashWorkoutDone;

    int nb_sec_show_interval;
    int nb_sec_show_interval_before;



    // -----------------------------------  Settings ----------------------------------------------------------------------
    int nb_user_studio;
    bool enable_studio_mode;
    bool use_pm_for_cadence;
    bool use_pm_for_speed;

    bool force_workout_window_on_top;
    bool show_included_workout;
    bool distance_in_km;
    QString strava_access_token;
    QString strava_refresh_token;    ///< OAuth2 refresh token for Strava.
    bool strava_auto_upload;         ///< Auto-upload completed activities to Strava.
    qint64 strava_token_expires_at;  ///< Epoch seconds when the access token expires.

    // Intervals.icu integration
    QString intervals_icu_athlete_id;  ///< Intervals.icu athlete ID (e.g. "i12345")
    QString intervals_icu_api_key;     ///< Intervals.icu API key (legacy / manual entry)
    bool    intervals_icu_auto_upload; ///< Auto-upload completed activities to Intervals.icu
    QString intervals_icu_access_token;  ///< OAuth2 bearer token (from OAuth login flow)
    QString intervals_icu_refresh_token; ///< OAuth2 refresh token (from OAuth login flow)
    QList<int> hr_zones;               ///< HR zone upper-bounds retrieved from Intervals.icu
    QList<int> power_zones;            ///< Power zone upper-bounds retrieved from Intervals.icu

    bool control_trainer_resistance;
    bool virtual_shifting;         ///< Opt-in (default off): drive resistance from a virtual gear (▲/▼) for single-cog / Zwift Cog setups. Off = real gears / free ride unchanged.
    int erg_smoothing_duration_s;  ///< Ramp duration (seconds) for ERG setpoint transitions; 0 = disabled.

    /// Battery warning threshold (issue #156): warn when a sensor's battery
    /// drops at or below this percentage (default 20, range 5–50).
    int battery_warning_threshold;

    /* ----- */

    // Sensor dropout auto-pause
    bool sensor_dropout_enabled;   ///< Master toggle for auto-pause on sensor dropout
    int  sensor_dropout_timeout_s; ///< Seconds before dropout triggers pause (default 5, range 2–30)

    int last_index_selected_config_workout;
    int last_tab_sub_config_selected;
    QString tab_display[8];

    //-------------- Util functions -------------
    // Must Matches the DB identifier
    QString getTimerStr() const {
        return "Timer";
    }
    QString getHrStr() const {
        return "Hr";
    }
    QString getPowerStr() const {
        return "Power";
    }
    QString getCadenceStr() const {
        return "Cadence";
    }
    QString getPowerBalanceStr() const {
        return "PowerBal";
    }
    QString getSpeedStr() const {
        return "Speed";
    }
    QString getOxygenStr() const {
        return "Oxygen";
    }
    QString getInfoWorkoutStr() const{
        return "InfoWorkout";
    }
    int getNumberWidget() const {
        return 8;
    }
    //-------------- /Util functions -------------




    int start_trigger;
    int value_cadence_start;
    int value_power_start;
    int value_speed_start;

    bool show_hr_widget;
    bool show_power_widget;
    bool show_power_balance_widget;
    bool show_cadence_widget;
    bool show_speed_widget;
    bool show_calories_widget;
    bool show_oxygen_widget;
    bool use_virtual_speed;
    bool show_trainer_speed;

    int display_hr;
    int display_power;
    int display_power_balance;
    int display_cadence;
    int display_video;

    bool show_timer_on_top;
    bool show_interval_remaining;
    bool show_workout_remaining;
    bool show_elapsed;
    bool show_current_target;

    int offset_power;

    bool show_seperator_interval;
    bool show_grid;
    bool show_hr_target;
    bool show_power_target;
    bool show_cadence_target;
    bool show_speed_target;
    bool show_hr_curve;
    bool show_power_curve;
    bool show_cadence_curve;
    bool show_speed_curve;

    /* ----- */
    int sound_player_vol;
    bool enable_sound;
    bool sound_interval;
    bool sound_pause_resume_workout;
    bool sound_achievement;
    bool sound_end_workout;

    bool sound_alert_power_under_target;
    bool sound_alert_power_above_target;
    bool sound_alert_cadence_under_target;
    bool sound_alert_cadence_above_target;

    bool interval_summary_enabled;
    int  interval_summary_duration_s; ///< seconds to display overlay (2–15)

    int app_theme; ///< 0=Light, 1=Dark, 2=System (default)
    //----










};
Q_DECLARE_METATYPE(Account*)

#endif // ACCOUNT_H
