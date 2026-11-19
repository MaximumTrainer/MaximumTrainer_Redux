#ifndef WEEKLYSUMMARYWIDGET_H
#define WEEKLYSUMMARYWIDGET_H

#include <QWidget>

#include "weeklysummary.h"

class QLabel;

/// Top of the History tab: this week's rides, time, TSS and distance (each
/// with the change from last week) and a bar chart of the last 12 weeks' TSS.
class WeeklySummaryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WeeklySummaryWidget(QWidget *parent = nullptr);

    static constexpr int kWeeks = 12;

    void setHistory(const QList<WorkoutHistorySummary> &history);

private:
    class TssBars;

    QLabel  *m_rides = nullptr;
    QLabel  *m_time  = nullptr;
    QLabel  *m_tss   = nullptr;
    QLabel  *m_km    = nullptr;
    TssBars *m_bars  = nullptr;
};

#endif // WEEKLYSUMMARYWIDGET_H
