#include "pmcdialog.h"

#include <QPen>
#include <QColor>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QDate>

#include "qwt_plot.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_marker.h"
#include "qwt_plot_grid.h"
#include "qwt_scale_draw.h"
#include "qwt_text.h"

// ── X-axis scale: Julian day → "dd MMM yy" ──────────────────────────────────

class DateScaleDraw : public QwtScaleDraw
{
public:
    QwtText label(double v) const override
    {
        const QDate d = QDate::fromJulianDay(static_cast<qint64>(v));
        return d.toString(QStringLiteral("d MMM yy"));
    }
};

// ── PmcDialog ────────────────────────────────────────────────────────────────

PmcDialog::PmcDialog(const QList<PmcPoint> &points, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Performance Management Chart"));
    setMinimumSize(800, 480);
    setupUi(points);
}

void PmcDialog::setupUi(const QList<PmcPoint> &points)
{
    // ── plot ──────────────────────────────────────────────────────────────────
    m_plot = new QwtPlot(this);
    m_plot->setAxisTitle(QwtPlot::xBottom, tr("Date"));
    m_plot->setAxisTitle(QwtPlot::yLeft,   tr("Load / Form"));
    m_plot->setAxisScaleDraw(QwtPlot::xBottom, new DateScaleDraw());

    // Show x-axis labels at reasonable intervals
    m_plot->setAxisMaxMajor(QwtPlot::xBottom, 10);

    auto *grid = new QwtPlotGrid();
    grid->setMajorPen(QPen(Qt::gray, 0, Qt::DashLine));
    grid->attach(m_plot);

    buildCurves(points);

    // ── info labels ───────────────────────────────────────────────────────────
    m_ctlLabel = new QLabel(this);
    m_atlLabel = new QLabel(this);
    m_tsbLabel = new QLabel(this);

    auto *infoBox = new QGroupBox(tr("Today"), this);
    auto *infoLayout = new QHBoxLayout(infoBox);
    infoLayout->addWidget(m_ctlLabel);
    infoLayout->addWidget(m_atlLabel);
    infoLayout->addWidget(m_tsbLabel);
    infoLayout->addStretch();

    buildInfoLabels(points);

    // ── simple color legend ───────────────────────────────────────────────────
    auto *legendLabel = new QLabel(
        QStringLiteral("<font color='#4a90d9'>&#9644; CTL (Fitness)</font>&nbsp;&nbsp;"
                       "<font color='#e05050'>&#9644; ATL (Fatigue)</font>&nbsp;&nbsp;"
                       "<font color='#2ea055'>&#9146; TSB (Form)</font>"), this);
    legendLabel->setAlignment(Qt::AlignCenter);

    // ── close button ──────────────────────────────────────────────────────────
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ── layout ────────────────────────────────────────────────────────────────
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_plot, 1);
    mainLayout->addWidget(legendLabel);
    mainLayout->addWidget(infoBox);
    mainLayout->addWidget(buttons);
}

void PmcDialog::buildCurves(const QList<PmcPoint> &points)
{
    if (points.isEmpty())
        return;

    QVector<double> xs, ctls, atls, tsbs;
    xs.reserve(points.size());
    ctls.reserve(points.size());
    atls.reserve(points.size());
    tsbs.reserve(points.size());

    for (const PmcPoint &p : points) {
        const double x = static_cast<double>(p.date.toJulianDay());
        xs   << x;
        ctls << p.ctl;
        atls << p.atl;
        tsbs << p.tsb;
    }

    // CTL — blue
    m_ctlCurve = new QwtPlotCurve(tr("CTL (Fitness)"));
    m_ctlCurve->setSamples(xs, ctls);
    m_ctlCurve->setPen(QPen(QColor(0x4A, 0x90, 0xD9), 2));
    m_ctlCurve->setRenderHint(QwtPlotItem::RenderAntialiased);
    m_ctlCurve->attach(m_plot);

    // ATL — red
    m_atlCurve = new QwtPlotCurve(tr("ATL (Fatigue)"));
    m_atlCurve->setSamples(xs, atls);
    m_atlCurve->setPen(QPen(QColor(0xE0, 0x50, 0x50), 2));
    m_atlCurve->setRenderHint(QwtPlotItem::RenderAntialiased);
    m_atlCurve->attach(m_plot);

    // TSB — green, dashed
    m_tsbCurve = new QwtPlotCurve(tr("TSB (Form)"));
    m_tsbCurve->setSamples(xs, tsbs);
    QPen tsbPen(QColor(0x2E, 0xA0, 0x55), 1.5, Qt::DashLine);
    m_tsbCurve->setPen(tsbPen);
    m_tsbCurve->setRenderHint(QwtPlotItem::RenderAntialiased);
    m_tsbCurve->attach(m_plot);

    // Zero reference line for TSB
    m_zeroLine = new QwtPlotMarker();
    m_zeroLine->setLineStyle(QwtPlotMarker::HLine);
    m_zeroLine->setYValue(0.0);
    m_zeroLine->setLinePen(QPen(Qt::darkGray, 0, Qt::SolidLine));
    m_zeroLine->attach(m_plot);

    m_plot->replot();
}

void PmcDialog::buildInfoLabels(const QList<PmcPoint> &points)
{
    if (points.isEmpty()) {
        m_ctlLabel->setText(tr("CTL: —"));
        m_atlLabel->setText(tr("ATL: —"));
        m_tsbLabel->setText(tr("TSB: —"));
        return;
    }

    const PmcPoint &last = points.last();
    m_ctlLabel->setText(tr("CTL: <b>%1</b>").arg(last.ctl, 0, 'f', 1));
    m_atlLabel->setText(tr("ATL: <b>%1</b>").arg(last.atl, 0, 'f', 1));

    const QString tsbColor = last.tsb >= 0 ? QStringLiteral("#2ea055") : QStringLiteral("#e05050");
    m_tsbLabel->setText(
        tr("TSB: <b><span style='color:%1'>%2</span></b>")
            .arg(tsbColor)
            .arg(last.tsb, 0, 'f', 1));
}
