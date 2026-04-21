#ifndef PMCDIALOG_H
#define PMCDIALOG_H

#include <QDialog>
#include <QList>

#include "pmccalculator.h"

class QwtPlot;
class QwtPlotCurve;
class QwtPlotMarker;
class QLabel;

/// Performance Management Chart dialog.
///
/// Displays Chronic Training Load (CTL / fitness), Acute Training Load
/// (ATL / fatigue) and Training Stress Balance (TSB / form) over time,
/// computed from the user's workout history.
class PmcDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PmcDialog(const QList<PmcPoint> &points, QWidget *parent = nullptr);

private:
    void setupUi(const QList<PmcPoint> &points);
    void buildCurves(const QList<PmcPoint> &points);
    void buildInfoLabels(const QList<PmcPoint> &points);

    QwtPlot       *m_plot    = nullptr;
    QwtPlotCurve  *m_ctlCurve = nullptr;
    QwtPlotCurve  *m_atlCurve = nullptr;
    QwtPlotCurve  *m_tsbCurve = nullptr;
    QwtPlotMarker *m_zeroLine = nullptr;

    QLabel *m_ctlLabel = nullptr;
    QLabel *m_atlLabel = nullptr;
    QLabel *m_tsbLabel = nullptr;
};

#endif // PMCDIALOG_H
