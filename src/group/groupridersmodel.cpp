#include "groupridersmodel.h"

#include <QtMath>

using namespace GroupProtocol;

namespace {

QString formatGap(int seconds) {
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

GroupRidersModel::GroupRidersModel(QObject *parent) : QAbstractListModel(parent) {}

int GroupRidersModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant GroupRidersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) return {};
    const RiderRow &row = m_rows.at(index.row());
    if (role == Qt::DisplayRole) return row.name;
    if (role == Qt::ToolTipRole) return detailLine(row, m_nowLocalMs);
    return {};
}

void GroupRidersModel::setRoster(const QList<RosterEntry> &roster, const QString &yourRiderId) {
    m_yourRiderId = yourRiderId;
    QList<RiderRow> rows;
    for (const RosterEntry &entry : roster) {
        RiderRow row;
        const int existing = rowOf(entry.riderId);
        if (existing >= 0) row = m_rows.at(existing);
        row.riderId = entry.riderId;
        row.name = entry.name;
        row.isHost = entry.isHost;
        row.isReady = entry.isReady;
        row.isYou = entry.riderId == yourRiderId;
        if (row.isYou) rows.prepend(row);
        else rows.append(row);
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

void GroupRidersModel::updateTelemetry(const RiderTelemetry &telemetry, qint64 nowLocalMs) {
    m_nowLocalMs = nowLocalMs;
    const int row = rowOf(telemetry.riderId);
    if (row < 0) return;
    m_rows[row].telemetry = telemetry;
    m_rows[row].lastTelemetryLocalMs = nowLocalMs;
    emit dataChanged(index(row), index(row));
}

void GroupRidersModel::updateOwnTelemetry(const RiderTelemetry &telemetry, qint64 nowLocalMs) {
    RiderTelemetry own = telemetry;
    own.riderId = m_yourRiderId;
    updateTelemetry(own, nowLocalMs);
    // Everyone's "behind" text is relative to you, so it moves with your clock.
    if (m_rows.size() > 1) emit dataChanged(index(1), index(int(m_rows.size()) - 1));
}

void GroupRidersModel::tick(qint64 nowLocalMs) {
    m_nowLocalMs = nowLocalMs;
    if (!m_rows.isEmpty()) emit dataChanged(index(0), index(int(m_rows.size()) - 1));
}

int GroupRidersModel::rowOf(const QString &riderId) const {
    for (int row = 0; row < m_rows.size(); ++row)
        if (m_rows.at(row).riderId == riderId) return row;
    return -1;
}

std::optional<double> GroupRidersModel::ownElapsedSeconds() const {
    const int row = rowOf(m_yourRiderId);
    if (row < 0 || !m_rows.at(row).telemetry) return std::nullopt;
    return m_rows.at(row).telemetry->elapsedWorkoutSeconds;
}

GroupRidersModel::RowState GroupRidersModel::stateOf(const RiderRow &row, qint64 nowLocalMs) const {
    if (!row.isReady) return RowState::Connecting;
    if (!row.telemetry) return RowState::Waiting;
    if (!row.isYou && nowLocalMs - row.lastTelemetryLocalMs > kStaleAfterMs) return RowState::Stale;
    const RiderTelemetry &telemetry = *row.telemetry;
    if (telemetry.status == RiderStatus::Done) return RowState::Finished;
    if (telemetry.status == RiderStatus::Paused) return RowState::Paused;
    if (!telemetry.powerWatts || !telemetry.targetPowerWatts || *telemetry.targetPowerWatts <= 0)
        return RowState::OnTarget;
    const int deviationPercent = qRound(100.0 * (*telemetry.powerWatts - *telemetry.targetPowerWatts)
                                        / *telemetry.targetPowerWatts);
    return qAbs(deviationPercent) <= kOnTargetTolerancePercent ? RowState::OnTarget : RowState::OffTarget;
}

QString GroupRidersModel::detailLine(const RiderRow &row, qint64 nowLocalMs) const {
    switch (stateOf(row, nowLocalMs)) {
    case RowState::Connecting: return tr("connecting sensors…");
    case RowState::Waiting:    return QString();
    case RowState::Stale:      return tr("connection lost");
    case RowState::Finished:   return tr("finished");
    default: break;
    }

    const RiderTelemetry &telemetry = *row.telemetry;
    QStringList parts;
    if (telemetry.status == RiderStatus::Paused) parts << tr("paused");
    else if (telemetry.percentFtp) parts << tr("%1% FTP").arg(*telemetry.percentFtp);
    if (telemetry.status != RiderStatus::Paused && telemetry.cadenceRpm)
        parts << tr("%1 rpm").arg(*telemetry.cadenceRpm);

    const std::optional<double> ownElapsed = ownElapsedSeconds();
    if (!row.isYou && ownElapsed) {
        const int gapSeconds = qRound(*ownElapsed - telemetry.elapsedWorkoutSeconds);
        if (gapSeconds >= kBehindThresholdSeconds) parts << tr("%1 behind").arg(formatGap(gapSeconds));
        else if (gapSeconds <= -kBehindThresholdSeconds) parts << tr("%1 ahead").arg(formatGap(-gapSeconds));
    }
    return parts.join(QStringLiteral(" · "));
}
