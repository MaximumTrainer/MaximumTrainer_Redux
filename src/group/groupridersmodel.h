#ifndef GROUPRIDERSMODEL_H
#define GROUPRIDERSMODEL_H

#include "groupprotocol.h"

#include <QAbstractListModel>

#include <optional>

/// Rows of the group riders column: you first, then the others in join order.
/// Holds each rider's latest telemetry and derives the status shown on the row.
class GroupRidersModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum class RowState { Connecting, Waiting, OnTarget, OffTarget, Paused, Finished, Stale };

    struct RiderRow {
        QString riderId;
        QString name;
        bool isHost = false;
        bool isYou = false;
        bool isReady = false;
        std::optional<GroupProtocol::RiderTelemetry> telemetry;
        qint64 lastTelemetryLocalMs = 0;
    };

    static constexpr qint64 kStaleAfterMs = 5000;
    static constexpr int kOnTargetTolerancePercent = 5;
    static constexpr int kBehindThresholdSeconds = 5;

    explicit GroupRidersModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void setRoster(const QList<GroupProtocol::RosterEntry> &roster, const QString &yourRiderId);
    void updateTelemetry(const GroupProtocol::RiderTelemetry &telemetry, qint64 nowLocalMs);
    /// Your own numbers come from the workout, not the relay.
    void updateOwnTelemetry(const GroupProtocol::RiderTelemetry &telemetry, qint64 nowLocalMs);
    /// Re-evaluates staleness; call about once a second.
    void tick(qint64 nowLocalMs);

    const RiderRow &rowAt(int row) const { return m_rows.at(row); }
    RowState stateOf(const RiderRow &row, qint64 nowLocalMs) const;
    /// "105% FTP · 92 rpm · 0:24 behind" style second line.
    QString detailLine(const RiderRow &row, qint64 nowLocalMs) const;

private:
    int rowOf(const QString &riderId) const;
    std::optional<double> ownElapsedSeconds() const;

    QList<RiderRow> m_rows;
    QString m_yourRiderId;
    qint64 m_nowLocalMs = 0;
};

#endif // GROUPRIDERSMODEL_H
