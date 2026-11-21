#ifndef CRITICALPOWERDIALOG_H
#define CRITICALPOWERDIALOG_H

#include <QDialog>
#include <QList>

#include "mmpcalculator.h"
#include "powerbests.h"
#include "workouthistorysummary.h"

class QComboBox;
class QLabel;
class QTableWidget;
class QwtPlot;
class QwtPlotCurve;

/// The rider's power curve (best average power per duration) over a chosen
/// period, compared with the all-time curve, plus the fitted Critical Power
/// model and a table of best efforts. Opened from the History tab.
class CriticalPowerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CriticalPowerDialog(const QList<WorkoutHistorySummary> &history,
                                 QWidget *parent = nullptr);

private:
    void setupUi();
    void loadRides(const QList<WorkoutHistorySummary> &history);
    void showPeriod();
    void updatePlot(const QVector<PowerBests::Effort> &period, const QVector<PowerBests::Effort> &allTime,
                    bool comparing);
    void updateTable(const QVector<PowerBests::Effort> &period, const QVector<PowerBests::Effort> &allTime,
                     bool comparing);
    void updateLabels(const CriticalPowerModel &model);

    QList<PowerBests::Ride> m_rides;
    bool m_loaded = false;

    QComboBox    *m_periodCombo  = nullptr;
    QwtPlot      *m_plot         = nullptr;
    QwtPlotCurve *m_periodCurve  = nullptr;
    QwtPlotCurve *m_allTimeCurve = nullptr;
    QwtPlotCurve *m_cpCurve      = nullptr;
    QTableWidget *m_table        = nullptr;
    QLabel       *m_cpLabel      = nullptr;
    QLabel       *m_wPrimeLabel  = nullptr;
    QLabel       *m_statusLabel  = nullptr;
};

#endif // CRITICALPOWERDIALOG_H
