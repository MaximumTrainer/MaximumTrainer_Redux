#include "ftpestimator.h"

#include <QDir>
#include <QFileInfo>
#include <QtMath>

#include "mmpcalculator.h"

namespace FtpEstimator {

int ftpFromBest20(double best20W)
{
    return qRound(best20W * 0.95);
}

bool worthSuggesting(int currentFtpW, int estimateW, int dismissedW)
{
    const int minimumGain = qMax(5, qRound(currentFtpW * 0.02));
    return estimateW >= currentFtpW + minimumGain && estimateW > dismissedW;
}

QList<QString> recentRides(const QString &historyDir, const QDateTime &now)
{
    QList<QString> rides;
    const QDateTime oldest = now.addDays(-kWindowDays);
    const QFileInfoList files = QDir(historyDir).entryInfoList({QStringLiteral("*.fit")}, QDir::Files);
    for (const QFileInfo &file : files)
        if (file.lastModified() >= oldest)
            rides.append(file.absoluteFilePath());
    return rides;
}

Estimate fromRides(const QList<QString> &fitFiles)
{
    Estimate estimate;
    if (fitFiles.isEmpty())
        return estimate;
    const QVector<double> best = MmpCalculator::computeMmp(fitFiles, {kEffortSeconds});
    if (best.isEmpty() || best.first() <= 0)
        return estimate;
    estimate.best20W = qRound(best.first());
    estimate.ftpW    = ftpFromBest20(best.first());
    estimate.valid   = estimate.ftpW > 0;
    return estimate;
}

} // namespace FtpEstimator
