#include "globalvars.h"

#include <QNetworkAccessManager>

#include "account.h"
#include "environnement.h"
#include "settings.h"
#include "soundplayer.h"
#include "sensor.h"
#include "trackpoint.h"
#include "userstudio.h"
#include "networkmonitor.h"
#include "config_bootstrap.h"
#include "logger.h"
#include "startup_trace.h"

#include <QWebEngineSettings>
#include <QWebEngineProfile>


GlobalVars::GlobalVars(QObject *parent) :
    QObject(parent)
{

#ifndef GC_WASM_BUILD
    LOG_INFO("Startup", QStringLiteral("SSL library: %1").arg(QSslSocket::sslLibraryBuildVersionString()));
#endif

    ConfigBootstrap::applyAppIdentity(Environnement::getVersion());

#ifndef Q_OS_WASM
    {
        StartupStep step("GlobalVars: QWebEngineProfile init");
        QWebEngineProfile::defaultProfile()->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    }
#endif // !Q_OS_WASM

    {
        // Used in Signal/Slots connection
        StartupStep step("GlobalVars: metatype registration");
        qRegisterMetaType<Sensor>("Sensor");
        qRegisterMetaType<QList<Sensor> >( "QList<Sensor>" );
        qRegisterMetaType<QList<int> >( "QList<int>" );
        qRegisterMetaType<QVector<UserStudio> >( "QVector<UserStudio>" );
        qRegisterMetaType<QList<Trackpoint> >( "QList<Trackpoint>" );
    }

    ///-------- INIT GLOBAL VAR ---------------------------
    Account *account = nullptr;
    {
        StartupStep step("GlobalVars: Account");
        account = new Account(this);
    }
    Settings *settings = nullptr;
    {
        StartupStep step("GlobalVars: Settings");
        settings = new Settings(this);
    }
    SoundPlayer *soundPlayer = nullptr;
    {
        StartupStep step("GlobalVars: SoundPlayer");
        soundPlayer = new SoundPlayer(this);
    }

    QNetworkAccessManager *manager = nullptr;
    QNetworkAccessManager *managerWS = nullptr;
    {
        StartupStep step("GlobalVars: network managers / SSL config");
        manager = new QNetworkAccessManager(this);
        managerWS = new QNetworkAccessManager(this);

#ifndef GC_WASM_BUILD
        QSslConfiguration sslCfg = QSslConfiguration::defaultConfiguration();
        QList<QSslCertificate> ca_list = sslCfg.caCertificates();
        QList<QSslCertificate> ca_new = QSslCertificate::fromData("CaCertificates");
        ca_list += ca_new;
        sslCfg.setCaCertificates(ca_list);
        sslCfg.setProtocol(QSsl::AnyProtocol);
        QSslConfiguration::setDefaultConfiguration(sslCfg);


        connect(manager, &QNetworkAccessManager::sslErrors, this, &GlobalVars::sslErrorHandler);
        connect(managerWS, &QNetworkAccessManager::sslErrors, this, &GlobalVars::sslErrorHandler);
#endif
    }


    qApp->setProperty("Account", QVariant::fromValue<Account*>(account));
    qApp->setProperty("User_Settings", QVariant::fromValue<Settings*>(settings));
    qApp->setProperty("SoundPlayer", QVariant::fromValue<SoundPlayer*>(soundPlayer));
    qApp->setProperty("NetworkManager", QVariant::fromValue<QNetworkAccessManager*>(manager));
    qApp->setProperty("NetworkManagerWS", QVariant::fromValue<QNetworkAccessManager*>(managerWS));

    // Initialise the network connectivity monitor now that the network managers
    // are registered.  The first probe fires after a 3-second startup delay.
    StartupStep step("GlobalVars: NetworkMonitor");
    NetworkMonitor::instance();
}

#ifndef GC_WASM_BUILD
void GlobalVars::sslErrorHandler(QNetworkReply* qnr, const QList<QSslError> & errlist) {

    Q_UNUSED(errlist);

    qnr->ignoreSslErrors();
}
#endif
