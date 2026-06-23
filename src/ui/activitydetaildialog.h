#ifndef ACTIVITYDETAILDIALOG_H
#define ACTIVITYDETAILDIALOG_H

#include <QDialog>

#include "workouthistorysummary.h"
#include "workouthistorydetail.h"

class QwtPlot;
class QTableWidget;
class QLabel;

/// Drill-down view for a single stored activity: a power/HR/cadence graph
/// re-rendered from the FIT record stream plus a per-lap stats table.
class ActivityDetailDialog : public QDialog
{
    Q_OBJECT

public:
    ActivityDetailDialog(const WorkoutHistorySummary &summary,
                         const WorkoutHistoryDetail &detail,
                         QWidget *parent = nullptr);

private:
    void buildGraph(const WorkoutHistoryDetail &detail);
    void buildLapTable(const WorkoutHistoryDetail &detail);

    QwtPlot      *m_plot     = nullptr;
    QTableWidget *m_lapTable = nullptr;
};

#endif // ACTIVITYDETAILDIALOG_H
