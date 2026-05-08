#include "globalvars.h"

#include <QNetworkAccessManager>

#include "account.h"
#include "environnement.h"
#include "settings.h"
#include "soundplayer.h"
#include "sensor.h"
#include "calibration_types.h"
#include "trackpoint.h"
#include "userstudio.h"
#include "networkmonitor.h"

#include <QDir>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#ifdef GC_HAVE_VLCQT
#include <VLCQtCore/Common.h>
#endif






GlobalVars::GlobalVars(QObject *parent) :
    QObject(parent)
{

    qDebug() << "SSL version"
#ifndef GC_WASM_BUILD
             << QSslSocket::sslLibraryBuildVersionString()
#endif
    ;

    QCoreApplication::setOrganizationName("MaximumTrainer");
    QCoreApplication::setOrganizationDomain("maximumtrainer.com");
    QCoreApplication::setApplicationName("MaximumTrainer");
    QCoreApplication::setApplicationVersion(Environnement::getVersion());

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

#ifndef Q_OS_WASM
    QWebEngineProfile::defaultProfile()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
#endif // !Q_OS_WASM


    // Used in Signal/Slots connection
    qRegisterMetaType<PowerCurve>("PowerCurve");
    qRegisterMetaType<Sensor>("Sensor");
    qRegisterMetaType<QList<Sensor> >( "QList<Sensor>" );
    qRegisterMetaType<QList<int> >( "QList<int>" );
    qRegisterMetaType<CalibrationType>("CalibrationType");
    qRegisterMetaType<QVector<UserStudio> >( "QVector<UserStudio>" );
    qRegisterMetaType<FEC_Controller::CALIBRATION_TYPE>("FEC_Controller::CALIBRATION_TYPE");
    qRegisterMetaType<FEC_Controller::TEMPERATURE_CONDITION>("FEC_Controller::TEMPERATURE_CONDITION");
    qRegisterMetaType<FEC_Controller::SPEED_CONDITION>("FEC_Controller::SPEED_CONDITION");
    qRegisterMetaType<QList<Trackpoint> >( "QList<Trackpoint>" );




    //plugin path using by libvlc
#ifdef GC_HAVE_VLCQT
    // Only override VLC_PLUGIN_PATH when the bundled plugins directory
    // actually exists next to the executable.  If it doesn't (e.g. in CI or
    // on a developer machine without a local VLC build), leave the variable
    // unset so libvlc falls back to the system VLC plugin path, which is
    // available when vlc-plugin-base (Linux) or VLC.app (macOS/Windows) is
    // installed.  Overriding with a non-existent path causes libvlc_new() to
    // return NULL, which subsequently triggers a null-pointer dereference.
    {
        const QString vlcPluginPath = QDir(qApp->applicationDirPath()).filePath(QStringLiteral("plugins"));
        if (QDir(vlcPluginPath).exists())
            VlcCommon::setPluginPath(vlcPluginPath);
    }
#endif


    ///-------- INIT GLOBAL VAR ---------------------------
    Account *account = new Account(this);
    Settings *settings = new Settings(this);
    SoundPlayer *soundPlayer = new SoundPlayer(this);

    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkAccessManager *managerWS = new QNetworkAccessManager(this);


#ifndef GC_WASM_BUILD
    QSslConfiguration sslCfg = QSslConfiguration::defaultConfiguration();
    QList<QSslCertificate> ca_list = sslCfg.caCertificates();
    QList<QSslCertificate> ca_new = QSslCertificate::fromData("CaCertificates");
    ca_list += ca_new;
    sslCfg.setCaCertificates(ca_list);
    sslCfg.setProtocol(QSsl::AnyProtocol);
    QSslConfiguration::setDefaultConfiguration(sslCfg);


    connect(manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(sslErrorHandler(QNetworkReply*,QList<QSslError>)));
    connect(managerWS, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(sslErrorHandler(QNetworkReply*,QList<QSslError>)));
#endif


    qApp->setProperty("Account", QVariant::fromValue<Account*>(account));
    qApp->setProperty("User_Settings", QVariant::fromValue<Settings*>(settings));
    qApp->setProperty("SoundPlayer", QVariant::fromValue<SoundPlayer*>(soundPlayer));
    qApp->setProperty("NetworkManager", QVariant::fromValue<QNetworkAccessManager*>(manager));
    qApp->setProperty("NetworkManagerWS", QVariant::fromValue<QNetworkAccessManager*>(managerWS));

    // Initialise the network connectivity monitor now that the network managers
    // are registered.  The first probe fires after a 3-second startup delay.
    NetworkMonitor::instance();
}

//----------------------------------------------------------------------------------
#ifndef GC_WASM_BUILD
void GlobalVars::sslErrorHandler(QNetworkReply* qnr, const QList<QSslError> & errlist) {

    Q_UNUSED(errlist);

    qnr->ignoreSslErrors();
}
#endif
