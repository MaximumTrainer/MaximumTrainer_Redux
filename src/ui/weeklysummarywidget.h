#ifndef WEEKLYSUMMARYWIDGET_H
#define WEEKLYSUMMARYWIDGET_H

#include <QWidget>

#include "weeklysummary.h"

class QLabel;

/// Top of the History tab: the weekly riding-time goal, this week's rides,
/// time, TSS and distance (each with the change from last week) and a bar
/// chart of the last 12 weeks' TSS.
class WeeklySummaryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WeeklySummaryWidget(QWidget *parent = nullptr);

    static constexpr int kWeeks = 12;

    void setHistory(const QList<WorkoutHistorySummary> &history);

    /// "3:10 / 5:00 h, 4-week streak" style text for \a progress, with a
    /// "change" (or "Set a weekly goal") link; activating it calls showGoalMenu().
    static QString goalText(const WeeklySummary::GoalProgress &progress);
    /// Pops up the goal choices (hours per week, or none) under \a anchor.
    static void showGoalMenu(QWidget *anchor);

private:
    class TssBars;
    void render();

    QList<WorkoutHistorySummary> m_history;
    QLabel  *m_goal  = nullptr;
    QLabel  *m_rides = nullptr;
    QLabel  *m_time  = nullptr;
    QLabel  *m_tss   = nullptr;
    QLabel  *m_km    = nullptr;
    TssBars *m_bars  = nullptr;
};

#endif // WEEKLYSUMMARYWIDGET_H
