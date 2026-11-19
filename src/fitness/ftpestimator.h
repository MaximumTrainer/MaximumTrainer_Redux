#ifndef FTPESTIMATOR_H
#define FTPESTIMATOR_H

#include <QDateTime>
#include <QList>
#include <QString>

/// Estimates FTP from recent rides so the rider rarely needs a ramp test:
/// 95% of the best 20-minute power in the last kWindowDays. Only increases are
/// suggested, since ERG workouts hold power at the target and cannot show a
/// decline.
namespace FtpEstimator {

constexpr int kWindowDays = 42;
constexpr int kEffortSeconds = 1200;

struct Estimate {
    int  ftpW    = 0;
    int  best20W = 0;   ///< best 20-minute average power behind the estimate
    bool valid   = false;
};

int ftpFromBest20(double best20W);

/// True when \a estimateW is a real gain over \a currentFtpW (at least 2% and
/// 5 W) and higher than the last estimate the rider dismissed.
bool worthSuggesting(int currentFtpW, int estimateW, int dismissedW);

/// FIT files in \a historyDir written within the window ending \a now.
QList<QString> recentRides(const QString &historyDir, const QDateTime &now);

/// Best 20-minute power across \a fitFiles turned into an FTP estimate.
Estimate fromRides(const QList<QString> &fitFiles);

} // namespace FtpEstimator

#endif // FTPESTIMATOR_H
