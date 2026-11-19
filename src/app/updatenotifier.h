#ifndef UPDATENOTIFIER_H
#define UPDATENOTIFIER_H

#include <QObject>
#include <QString>

/// Checks GitHub for a newer release in the background and reports it through
/// updateAvailable(); never blocks or shows UI itself.
class UpdateNotifier : public QObject
{
    Q_OBJECT

public:
    explicit UpdateNotifier(QObject *parent = nullptr);

    /// Starts the check. No-op for WASM and source builds (version 0.0.0).
    void check();

    /// The release tag in \a releaseJson when it is newer than \a currentVersion,
    /// otherwise an empty string.
    static QString newerVersion(const QString &currentVersion, const QString &releaseJson);

signals:
    void updateAvailable(const QString &latestVersion);
};

#endif // UPDATENOTIFIER_H
