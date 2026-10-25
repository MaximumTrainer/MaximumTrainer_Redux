/*
 * tst_config_bootstrap.cpp
 *
 * Qt Test suite for ConfigBootstrap (#377): first-run defaults, upgrade of
 * unversioned installs, legacy "Max++ inc." migration ordering, unreadable
 * stores, and CredentialStore load failures being logged without the value.
 *
 * Every test works on its own INI files in a temporary directory, with
 * QStandardPaths test mode on, so the real user settings are never touched.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QStandardPaths>
#include <QFile>

#include "config_bootstrap.h"
#include "config_defaults.h"
#include "credential_store.h"
#include "logger.h"

using ConfigBootstrap::Result;

class TstConfigBootstrap : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void testFirstRun_writesDefaults();
    void testSecondRun_noOverwrite();
    void testExistingUserWithoutVersion_onlyAddsVersion();
    void testMigration_beforeDefaults();
    void testMigration_skippedWhenAlreadyMigrated();
    void testCorruptStore_fallsBackToDefaults();
    void testCredentialLoadFailure_loggedWithoutValue();

private:
    QString ini(const QString &name) const { return m_dir->filePath(name); }
    QString logContent() const;

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_logPath;
};

void TstConfigBootstrap::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("MaximumTrainerTest"));
    QCoreApplication::setApplicationName(QStringLiteral("ConfigBootstrapTest"));
}

void TstConfigBootstrap::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_logPath = m_dir->filePath(QStringLiteral("test.log"));
    Logger::instance().setLogLevel(LogLevel::Info);
    Logger::instance().setFileLogging(true, m_logPath);
}

void TstConfigBootstrap::cleanup()
{
    Logger::instance().setFileLogging(false);
    m_dir.reset();
}

QString TstConfigBootstrap::logContent() const
{
    QFile f(m_logPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

void TstConfigBootstrap::testFirstRun_writesDefaults()
{
    QSettings settings(ini(QStringLiteral("new.ini")), QSettings::IniFormat);
    QSettings legacy(ini(QStringLiteral("legacy.ini")), QSettings::IniFormat);
    const QString logFile = QStringLiteral("/some/where/MaximumTrainer.log");

    const auto outcome = ConfigBootstrap::run(settings, &legacy, logFile);

    QCOMPARE(outcome.result, Result::Created);
    QCOMPARE(settings.value("config_version").toInt(), ConfigDefaults::ConfigVersion);
    QCOMPARE(settings.value("logging/level").toInt(), 2);
    QCOMPARE(settings.value("logging/file_enabled").toBool(), true);
    QCOMPARE(settings.value("logging/file_path").toString(), logFile);
    QCOMPARE(settings.value("account/FTP").toInt(), 150);
    QCOMPARE(settings.value("account/LTHR").toInt(), 150);
    QCOMPARE(settings.value("account/weight_kg").toDouble(), 70.0);
    QCOMPARE(settings.value("account/nb_sec_show_interval").toInt(), 5);
    QCOMPARE(settings.value("account/nb_sec_show_interval_before").toInt(), 4);
    QCOMPARE(settings.value("account/erg_smoothing_duration_s").toInt(), 5);
    QCOMPARE(settings.value("account/sensor_dropout_enabled").toBool(), true);
    QCOMPARE(settings.value("account/sensor_dropout_timeout_s").toInt(), 5);
    QCOMPARE(settings.value("account/battery_warning_threshold").toInt(), 20);

    // Persisted, not just held in memory.
    QSettings reread(ini(QStringLiteral("new.ini")), QSettings::IniFormat);
    QCOMPARE(reread.value("account/FTP").toInt(), 150);

    QVERIFY(logContent().contains(QStringLiteral("no configuration found — default configuration created at")));
}

void TstConfigBootstrap::testSecondRun_noOverwrite()
{
    const QString path = ini(QStringLiteral("existing.ini"));
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("config_version", ConfigDefaults::ConfigVersion);
        s.setValue("account/FTP", 250);
        s.setValue(ConfigBootstrap::MigrationMarkerKey, true);
    }
    QSettings settings(path, QSettings::IniFormat);
    QSettings legacy(ini(QStringLiteral("legacy.ini")), QSettings::IniFormat);

    const auto outcome = ConfigBootstrap::run(settings, &legacy, QStringLiteral("x.log"));

    QCOMPARE(outcome.result, Result::Existing);
    QCOMPARE(settings.value("account/FTP").toInt(), 250);
    QVERIFY(!settings.contains("account/LTHR"));
    QVERIFY(!logContent().contains(QStringLiteral("default configuration created")));
}

void TstConfigBootstrap::testExistingUserWithoutVersion_onlyAddsVersion()
{
    const QString path = ini(QStringLiteral("unversioned.ini"));
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue("account/FTP", 275);
        s.setValue("logging/level", 1);
        s.setValue(ConfigBootstrap::MigrationMarkerKey, true);
    }
    QStringList before;
    {
        QSettings s(path, QSettings::IniFormat);
        before = s.allKeys();
    }
    QSettings settings(path, QSettings::IniFormat);

    const auto outcome = ConfigBootstrap::run(settings, nullptr, QStringLiteral("x.log"));

    QCOMPARE(outcome.result, Result::Upgraded);
    QStringList after = settings.allKeys();
    after.removeOne(QString::fromLatin1(ConfigBootstrap::ConfigVersionKey));
    before.sort();
    after.sort();
    QCOMPARE(after, before);
    QCOMPARE(settings.value("config_version").toInt(), ConfigDefaults::ConfigVersion);
    QCOMPARE(settings.value("account/FTP").toInt(), 275);
    QCOMPARE(settings.value("logging/level").toInt(), 1);
    QVERIFY(!logContent().contains(QStringLiteral("default configuration created")));
}

void TstConfigBootstrap::testMigration_beforeDefaults()
{
    {
        QSettings old(ini(QStringLiteral("legacy.ini")), QSettings::IniFormat);
        old.setValue("account/FTP", 310);
        old.setValue("account/weight_kg", 82.5);
        old.setValue("some/other_key", QStringLiteral("kept"));
    }
    QSettings settings(ini(QStringLiteral("new.ini")), QSettings::IniFormat);
    QSettings legacy(ini(QStringLiteral("legacy.ini")), QSettings::IniFormat);

    const auto outcome = ConfigBootstrap::run(settings, &legacy, QStringLiteral("x.log"));

    QCOMPARE(outcome.result, Result::Created);
    QVERIFY(outcome.migrationRan);
    QCOMPARE(outcome.migratedKeys, 3);
    QCOMPARE(settings.value("account/FTP").toInt(), 310);           // migrated wins
    QCOMPARE(settings.value("account/weight_kg").toDouble(), 82.5); // migrated wins
    QCOMPARE(settings.value("account/LTHR").toInt(), 150);          // default fills gap
    QCOMPARE(settings.value("some/other_key").toString(), QStringLiteral("kept"));
    QVERIFY(settings.value(ConfigBootstrap::MigrationMarkerKey).toBool());
    QVERIFY(logContent().contains(QStringLiteral("migration ran: copied 3 key(s)")));
}

void TstConfigBootstrap::testMigration_skippedWhenAlreadyMigrated()
{
    {
        QSettings old(ini(QStringLiteral("legacy.ini")), QSettings::IniFormat);
        old.setValue("account/FTP", 310);
    }
    const QString path = ini(QStringLiteral("new.ini"));
    {
        QSettings s(path, QSettings::IniFormat);
        s.setValue(ConfigBootstrap::MigrationMarkerKey, true);
        s.setValue("account/FTP", 200);
    }
    QSettings settings(path, QSettings::IniFormat);
    QSettings legacy(ini(QStringLiteral("legacy.ini")), QSettings::IniFormat);

    const auto outcome = ConfigBootstrap::run(settings, &legacy, QStringLiteral("x.log"));

    QVERIFY(!outcome.migrationRan);
    QCOMPARE(settings.value("account/FTP").toInt(), 200);
    QVERIFY(logContent().contains(QStringLiteral("migration skipped (already migrated)")));
}

void TstConfigBootstrap::testCorruptStore_fallsBackToDefaults()
{
    const QString path = ini(QStringLiteral("corrupt.ini"));
    const QByteArray corrupt("[account\nFTP=250\n[logging]\nlevel=1\n");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(corrupt);
    }

    {
        QSettings settings(path, QSettings::IniFormat);
        const auto outcome = ConfigBootstrap::run(settings, nullptr, QStringLiteral("x.log"));
        QCOMPARE(settings.status(), QSettings::FormatError);
        QCOMPARE(outcome.result, Result::Unreadable);
        // In-memory defaults: nothing was written into the unreadable store.
        QVERIFY(!settings.contains("config_version"));
        QCOMPARE(settings.value("account/LTHR", ConfigDefaults::LTHR).toInt(), ConfigDefaults::LTHR);
    }

    const QString log = logContent();
    QVERIFY2(log.contains(QStringLiteral("[ERROR] [Config]")), qPrintable(log));
    QVERIFY(log.contains(QStringLiteral("FormatError")));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), corrupt);
}

void TstConfigBootstrap::testCredentialLoadFailure_loggedWithoutValue()
{
#ifdef Q_OS_MACOS
    QSKIP("macOS stores credentials in the Keychain, not in QSettings");
#endif
    const QSettings::Format savedFormat = QSettings::defaultFormat();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir->path());
    auto restore = qScopeGuard([savedFormat] { QSettings::setDefaultFormat(savedFormat); });

    const QString secret = QStringLiteral("c2VjcmV0LXRva2VuLXZhbHVlLTEyMzQ1Njc4OTA=");
    {
        QSettings s;
        s.setValue(QStringLiteral("credentials/strava/access_token"), secret);
    }

    const QString value = CredentialStore::load(QStringLiteral("strava"), QStringLiteral("access_token"));

    QVERIFY(value.isEmpty());
    const QString log = logContent();
    QVERIFY2(log.contains(QStringLiteral("[WARN] [CredentialStore]")), qPrintable(log));
    QVERIFY(log.contains(QStringLiteral("strava/access_token")));
    QVERIFY(!log.contains(secret));
    QVERIFY(!log.contains(QStringLiteral("secret-token-value")));
}

QTEST_GUILESS_MAIN(TstConfigBootstrap)
#include "tst_config_bootstrap.moc"
