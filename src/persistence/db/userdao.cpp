#include "userdao.h"

#include "environnement.h"
#include "util.h"
#include "logger.h"



//http://localhost/index.php/api/account_rest/account/id/1/session_mt_id/a0b22d52d043b8de968239ba865919fbe721a40401063d2b69f7f6608d40aec8
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//QNetworkReply* UserDAO::getAccount(int id, QString session_mt_id) {

//    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();


//    const QString url =  Environnement::getURLEnvironnementWS() + "api/account_rest/account"
//            "/id/" + id +
//            "/session_mt_id/"  + session_mt_id +
//            "/format/json";


//    qDebug() << "getAccount url: " << url;


//    QNetworkRequest request;
//    request.setUrl(QUrl(url));
//    request.setRawHeader("User-Agent", "MyOwnBrowser 1.0");

//    QNetworkReply *reply = managerWS->get(request);
//    return reply;
//}


// http://localhost/index.php/api/account_rest/account/
// https://www.dropbox.com/s/745odh7k7o2rtx6/putUserInfo.png
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QNetworkReply* UserDAO::putAccount(Account *account) {

    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("UserDAO", QStringLiteral("putAccount: NetworkManagerWS not available"));
        return nullptr;
    }
    const QString url =  Environnement::getURLEnvironnementWS() + "api/account_rest/account/";
    LOG_DEBUG("UserDAO", QStringLiteral("putAccount: PUT ") + url);


    QUrlQuery postData;


    qDebug() << "**** PUT with session_mt_id:" << account->session_mt_id;

    postData.addQueryItem("id", QString::number(account->id) );
    postData.addQueryItem("session_mt_id", account->session_mt_id);


    postData.addQueryItem("last_os", account->os);


    postData.addQueryItem("FTP", QString::number(account->FTP) );
    postData.addQueryItem("LTHR", QString::number(account->LTHR) );
    postData.addQueryItem("minutes_rode", QString::number(account->minutes_rode) );
    postData.addQueryItem("weight_kg", QString::number(account->weight_kg) );
    postData.addQueryItem("weight_in_kg", QString::number(account->weight_in_kg) );
    postData.addQueryItem("height_cm", QString::number(account->height_cm) );
    postData.addQueryItem("trainer_curve_id", QString::number( account->powerCurve.getId()) ) ;


    postData.addQueryItem("wheel_circ", QString::number(account->wheel_circ) );
    postData.addQueryItem("bike_weight_kg", QString::number(account->bike_weight_kg) );
    postData.addQueryItem("bike_type", QString::number(account->bike_type) );

    // -------------- Settings -------------------------------------------------------------------------------------
    postData.addQueryItem("nb_user_studio", QString::number(account->nb_user_studio) );
    postData.addQueryItem("enable_studio_mode", QString::number(account->enable_studio_mode) );
    postData.addQueryItem("use_pm_for_cadence", QString::number(account->use_pm_for_cadence) );
    postData.addQueryItem("use_pm_for_speed", QString::number(account->use_pm_for_speed) );

    postData.addQueryItem("force_workout_window_on_top", QString::number(account->force_workout_window_on_top) );
    postData.addQueryItem("show_included_workout", QString::number(account->show_included_workout) );
    postData.addQueryItem("show_included_course", QString::number(account->show_included_course) );
    postData.addQueryItem("distance_in_km", QString::number(account->distance_in_km) );
    postData.addQueryItem("strava_access_token", account->strava_access_token);
    postData.addQueryItem("strava_private_upload", QString::number(account->strava_private_upload) );

    postData.addQueryItem("training_peaks_access_token", account->training_peaks_access_token);
    postData.addQueryItem("training_peaks_refresh_token", account->training_peaks_refresh_token);
    postData.addQueryItem("training_peaks_public_upload", QString::number(account->training_peaks_public_upload) );

    postData.addQueryItem("selfloops_user", account->selfloops_user);
    postData.addQueryItem("selfloops_pw", account->selfloops_pw);
    postData.addQueryItem("control_trainer_resistance", QString::number(account->control_trainer_resistance) );
    postData.addQueryItem("stop_pairing_on_found", QString::number(account->stop_pairing_on_found) );
    postData.addQueryItem("nb_sec_pairing", QString::number(account->nb_sec_pairing) );

    /* ----- */
    postData.addQueryItem("last_index_selected_config_workout", QString::number(account->last_index_selected_config_workout) );
    postData.addQueryItem("last_tab_sub_config_selected", QString::number(account->last_tab_sub_config_selected) );
    postData.addQueryItem("tab_display0", account->tab_display[0] );
    postData.addQueryItem("tab_display1", account->tab_display[1] );
    postData.addQueryItem("tab_display2", account->tab_display[2] );
    postData.addQueryItem("tab_display3", account->tab_display[3] );
    postData.addQueryItem("tab_display4", account->tab_display[4] );
    postData.addQueryItem("tab_display5", account->tab_display[5] );
    postData.addQueryItem("tab_display6", account->tab_display[6] );
    postData.addQueryItem("tab_display7", account->tab_display[7] );


    postData.addQueryItem("start_trigger", QString::number(account->start_trigger) );
    postData.addQueryItem("value_cadence_start", QString::number(account->value_cadence_start) );
    postData.addQueryItem("value_power_start", QString::number(account->value_power_start) );
    postData.addQueryItem("value_speed_start", QString::number(account->value_speed_start) );
    postData.addQueryItem("show_hr_widget", QString::number(account->show_hr_widget) );
    postData.addQueryItem("show_power_widget", QString::number(account->show_power_widget) );
    postData.addQueryItem("show_power_balance_widget", QString::number(account->show_power_balance_widget) );
    postData.addQueryItem("show_cadence_widget", QString::number(account->show_cadence_widget) );
    postData.addQueryItem("show_speed_widget", QString::number(account->show_speed_widget) );
    postData.addQueryItem("show_calories_widget", QString::number(account->show_calories_widget) );
    postData.addQueryItem("show_oxygen_widget", QString::number(account->show_oxygen_widget) );
    postData.addQueryItem("use_virtual_speed", QString::number(account->use_virtual_speed) );
    postData.addQueryItem("show_trainer_speed", QString::number(account->show_trainer_speed) );


    postData.addQueryItem("display_hr", QString::number(account->display_hr) );
    postData.addQueryItem("display_power", QString::number(account->display_power) );
    postData.addQueryItem("display_power_balance", QString::number(account->display_power_balance) );
    postData.addQueryItem("display_cadence", QString::number(account->display_cadence) );

    postData.addQueryItem("show_timer_on_top", QString::number(account->show_timer_on_top) );
    postData.addQueryItem("show_interval_remaining", QString::number(account->show_interval_remaining) );
    postData.addQueryItem("show_workout_remaining", QString::number(account->show_workout_remaining) );
    postData.addQueryItem("show_elapsed", QString::number(account->show_elapsed) );
    postData.addQueryItem("font_size_timer", QString::number(account->font_size_timer) );

    postData.addQueryItem("averaging_power", QString::number(account->averaging_power) );
    postData.addQueryItem("offset_power", QString::number(account->offset_power) );

    postData.addQueryItem("show_seperator_interval", QString::number(account->show_seperator_interval) );
    postData.addQueryItem("show_grid", QString::number(account->show_grid) );
    postData.addQueryItem("show_hr_target", QString::number(account->show_hr_target) );
    postData.addQueryItem("show_power_target", QString::number(account->show_power_target) );
    postData.addQueryItem("show_cadence_target", QString::number(account->show_cadence_target) );
    postData.addQueryItem("show_speed_target", QString::number(account->show_speed_target) );
    postData.addQueryItem("show_hr_curve", QString::number(account->show_hr_curve) );
    postData.addQueryItem("show_power_curve", QString::number(account->show_power_curve) );
    postData.addQueryItem("show_cadence_curve", QString::number(account->show_cadence_curve) );
    postData.addQueryItem("show_speed_curve", QString::number(account->show_speed_curve) );
    /* ----- */
    postData.addQueryItem("sound_player_vol", QString::number(account->sound_player_vol) );
    postData.addQueryItem("enable_sound", QString::number(account->enable_sound) );
    postData.addQueryItem("sound_interval", QString::number(account->sound_interval) );
    postData.addQueryItem("sound_pause_resume_workout", QString::number(account->sound_pause_resume_workout) );
    postData.addQueryItem("sound_achievement", QString::number(account->sound_achievement) );
    postData.addQueryItem("sound_end_workout", QString::number(account->sound_end_workout) );
    postData.addQueryItem("sound_alert_power_under_target", QString::number(account->sound_alert_power_under_target) );
    postData.addQueryItem("sound_alert_power_above_target", QString::number(account->sound_alert_power_above_target) );
    postData.addQueryItem("sound_alert_cadence_under_target", QString::number(account->sound_alert_cadence_under_target) );
    postData.addQueryItem("sound_alert_cadence_above_target", QString::number(account->sound_alert_cadence_above_target) );



    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");



    QNetworkReply *replyPutUser = managerWS->put(request, postData.toString(QUrl::FullyEncoded).toUtf8() );

    return replyPutUser;
}







