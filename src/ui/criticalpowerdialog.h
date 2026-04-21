#ifndef CRITICALPOWERDIALOG_H
#define CRITICALPOWERDIALOG_H

#include <QDialog>
#include <QList>

#include "workouthistorysummary.h"
#include "mmpcalculator.h"

class QLabel;
class QwtPlot;
class QwtPlotCurve;

/**
 * @brief Dialog displaying the Mean Maximal Power (MMP) curve and fitted
 *        Critical Power (CP) model for a rider's history of FIT activities.
 *
 * Open from the Workout History tab via the "Critical Power" button.
 */
class CriticalPowerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CriticalPowerDialog(const QList<WorkoutHistorySummary> &history,
                                 QWidget *parent = nullptr);

private:
    void setupUi();
    void calculate();
    void updatePlot(const QVector<double> &mmps);
    void updateLabels(const CriticalPowerModel &model);

    QList<WorkoutHistorySummary> m_history;
    QVector<double>              m_mmps;
    CriticalPowerModel           m_cpModel;

    QwtPlot      *m_plot       = nullptr;
    QwtPlotCurve *m_mmpCurve   = nullptr;
    QwtPlotCurve *m_cpCurve    = nullptr;
    QLabel       *m_cpLabel    = nullptr;
    QLabel       *m_wPrimeLabel= nullptr;
    QLabel       *m_statusLabel= nullptr;
};

#endif // CRITICALPOWERDIALOG_H
