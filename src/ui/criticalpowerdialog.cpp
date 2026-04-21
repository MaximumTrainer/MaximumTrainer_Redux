#include "criticalpowerdialog.h"

#include "mmpcalculator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QApplication>
#include <QFrame>

#include "qwt_plot.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_grid.h"
#include "qwt_scale_engine.h"
#include "qwt_scale_draw.h"
#include "qwt_text.h"
#include "qwt_symbol.h"

// ---------------------------------------------------------------------------
// Custom scale draw for human-readable duration labels
// ---------------------------------------------------------------------------
class DurationScaleDraw : public QwtScaleDraw
{
public:
    QwtText label(double value) const override
    {
        int sec = static_cast<int>(value + 0.5);
        if (sec < 60)
            return QwtText(QString::number(sec) + QStringLiteral("s"));
        if (sec < 3600) {
            int m = sec / 60, s = sec % 60;
            if (s == 0)
                return QwtText(QString::number(m) + QStringLiteral("m"));
            return QwtText(QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0')));
        }
        int h = sec / 3600, m = (sec % 3600) / 60;
        if (m == 0)
            return QwtText(QString::number(h) + QStringLiteral("h"));
        return QwtText(QString("%1h%2").arg(h).arg(m, 2, 10, QChar('0')));
    }
};

// ---------------------------------------------------------------------------
// CriticalPowerDialog
// ---------------------------------------------------------------------------

CriticalPowerDialog::CriticalPowerDialog(const QList<WorkoutHistorySummary> &history,
                                         QWidget *parent)
    : QDialog(parent)
    , m_history(history)
{
    setWindowTitle(tr("Critical Power Curve"));
    setMinimumSize(640, 480);
    setupUi();
    calculate();
}

void CriticalPowerDialog::setupUi()
{
    m_plot = new QwtPlot(this);
    m_plot->setTitle(tr("Mean Maximal Power"));
    m_plot->setAxisTitle(QwtPlot::xBottom, tr("Duration"));
    m_plot->setAxisTitle(QwtPlot::yLeft,   tr("Power (W)"));
    m_plot->setAxisScaleEngine(QwtPlot::xBottom, new QwtLogScaleEngine());
    m_plot->setAxisScaleDraw(QwtPlot::xBottom, new DurationScaleDraw());
    m_plot->setCanvasBackground(Qt::white);

    auto *grid = new QwtPlotGrid();
    grid->setMajorPen(Qt::lightGray, 1, Qt::DashLine);
    grid->attach(m_plot);

    m_mmpCurve = new QwtPlotCurve(tr("MMP"));
    m_mmpCurve->setStyle(QwtPlotCurve::Lines);
    m_mmpCurve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
                                         QBrush(QColor(0, 114, 189)),
                                         QPen(QColor(0, 114, 189)), QSize(6, 6)));
    m_mmpCurve->setPen(QColor(0, 114, 189), 2);
    m_mmpCurve->attach(m_plot);

    m_cpCurve = new QwtPlotCurve(tr("CP Model"));
    m_cpCurve->setStyle(QwtPlotCurve::Lines);
    m_cpCurve->setPen(QColor(217, 83, 25), 2, Qt::DashLine);
    m_cpCurve->attach(m_plot);

    // Readout panel
    auto *statsFrame = new QFrame(this);
    statsFrame->setFrameShape(QFrame::StyledPanel);
    auto *statsLayout = new QHBoxLayout(statsFrame);

    m_cpLabel = new QLabel(tr("CP: —"), statsFrame);
    m_cpLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));

    m_wPrimeLabel = new QLabel(tr("W': —"), statsFrame);
    m_wPrimeLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));

    statsLayout->addStretch();
    statsLayout->addWidget(m_cpLabel);
    statsLayout->addSpacing(24);
    statsLayout->addWidget(m_wPrimeLabel);
    statsLayout->addStretch();

    m_statusLabel = new QLabel(this);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_plot, 1);
    mainLayout->addWidget(statsFrame);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(buttonBox);
}

void CriticalPowerDialog::calculate()
{
    if (m_history.isEmpty()) {
        m_statusLabel->setText(tr("No activities to analyse."));
        return;
    }

    m_statusLabel->setText(tr("Calculating…"));
    qApp->processEvents();

    QList<QString> paths;
    for (const WorkoutHistorySummary &s : m_history) {
        if (!s.filePath.isEmpty())
            paths.append(s.filePath);
    }

    m_mmps    = MmpCalculator::computeMmp(paths, MMP_DURATIONS);
    m_cpModel = MmpCalculator::fitCriticalPower(MMP_DURATIONS, m_mmps);

    updatePlot(m_mmps);
    updateLabels(m_cpModel);

    int activitiesWithPower = 0;
    for (double v : m_mmps) { if (v > 0) { ++activitiesWithPower; break; } }
    if (activitiesWithPower == 0)
        m_statusLabel->setText(tr("No power data found in %1 activit(y/ies).").arg(paths.size()));
    else
        m_statusLabel->setText(tr("Based on %n activit(y/ies).", "", paths.size()));
}

void CriticalPowerDialog::updatePlot(const QVector<double> &mmps)
{
    QVector<double> xs, ys;
    for (int i = 0; i < MMP_DURATIONS.size() && i < mmps.size(); ++i) {
        if (mmps[i] > 0) {
            xs.append(static_cast<double>(MMP_DURATIONS[i]));
            ys.append(mmps[i]);
        }
    }

    if (!xs.isEmpty()) {
        m_mmpCurve->setSamples(xs, ys);
        m_plot->setAxisScale(QwtPlot::xBottom, xs.first(), xs.last());
    }

    // CP model curve (smooth, over full range of valid MMP durations)
    if (m_cpModel.valid && !xs.isEmpty()) {
        QVector<double> cxs, cys;
        for (int i = 0; i < MMP_DURATIONS.size() && i < mmps.size(); ++i) {
            if (mmps[i] > 0) {
                double t = static_cast<double>(MMP_DURATIONS[i]);
                cxs.append(t);
                cys.append(m_cpModel.cp + m_cpModel.wPrime / t);
            }
        }
        m_cpCurve->setSamples(cxs, cys);
    }

    m_plot->replot();
}

void CriticalPowerDialog::updateLabels(const CriticalPowerModel &model)
{
    if (model.valid) {
        m_cpLabel->setText(tr("CP: %1 W").arg(qRound(model.cp)));
        double wPrimeKj = model.wPrime / 1000.0;
        m_wPrimeLabel->setText(tr("W': %1 kJ").arg(wPrimeKj, 0, 'f', 1));
    } else {
        m_cpLabel->setText(tr("CP: —"));
        m_wPrimeLabel->setText(tr("W': —  (need ≥3 efforts at 2–30 min)"));
    }
}
