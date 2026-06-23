#include "activitydetaildialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QPen>
#include <QColor>
#include <QSettings>
#include <QCloseEvent>

#include "qwt_plot.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_grid.h"
#include "qwt_legend.h"

#include "util.h"

namespace {

const char *kGeometryKey = "activityDetailDialog/geometry";

QString fmtDuration(int sec)
{
    const int h = sec / 3600;
    const int m = (sec % 3600) / 60;
    const int s = sec % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

// Attach a single channel as a curve, skipping samples where the value is the
// "absent" sentinel (negative). Returns true if any point was plotted.
bool attachChannel(QwtPlot *plot, const QVector<ActivityRecordPoint> &records,
                   int ActivityRecordPoint::*field, const QString &title, const QColor &color)
{
    QVector<double> xs;
    QVector<double> ys;
    xs.reserve(records.size());
    ys.reserve(records.size());
    for (const ActivityRecordPoint &r : records) {
        const int v = r.*field;
        if (v < 0)
            continue;
        xs.append(r.elapsedSec / 60.0);  // minutes
        ys.append(v);
    }
    if (xs.isEmpty())
        return false;

    auto *curve = new QwtPlotCurve(title);
    curve->setPen(QPen(color, 1.5));
    curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    curve->setSamples(xs, ys);
    curve->attach(plot);
    return true;
}

} // namespace

ActivityDetailDialog::ActivityDetailDialog(const WorkoutHistorySummary &summary,
                                           const WorkoutHistoryDetail &detail,
                                           QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Activity - %1").arg(summary.workoutName));
    setMinimumSize(860, 600);
    // Use the plain top-level window type rather than the default Qt::Dialog
    // type: window managers treat dialogs as fixed helper windows and won't give
    // them a working maximise/restore toggle. This keeps the app-modal behaviour
    // (set by the caller) while exposing the normal window controls.
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint |
                   Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);

    auto *header = new QLabel(this);
    header->setTextFormat(Qt::RichText);
    header->setText(tr("<b>%1</b><br>%2 &nbsp;·&nbsp; %3 &nbsp;·&nbsp; %4 W avg &nbsp;·&nbsp; "
                       "%5 W NP &nbsp;·&nbsp; %6 bpm &nbsp;·&nbsp; TSS %7")
                        .arg(summary.workoutName,
                             summary.startTime.toString(QStringLiteral("ddd d MMM yyyy, HH:mm")),
                             fmtDuration(summary.durationSec))
                        .arg(summary.avgPowerW)
                        .arg(summary.normalizedPower)
                        .arg(summary.avgHrBpm)
                        .arg(summary.tss, 0, 'f', 0));

    buildGraph(detail);
    buildLapTable(detail);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(header);
    layout->addWidget(m_plot, 3);
    if (m_lapTable)
        layout->addWidget(m_lapTable, 2);
    layout->addWidget(buttons);

    // Reopen in the same state the user last left (maximised or a given size).
    // First ever open defaults to maximised.
    const QByteArray geom = QSettings().value(kGeometryKey).toByteArray();
    if (geom.isEmpty())
        setWindowState(windowState() | Qt::WindowMaximized);
    else
        restoreGeometry(geom);
}

void ActivityDetailDialog::saveDialogGeometry()
{
    // saveGeometry() captures both the window state (maximised/normal) and the
    // restored size, so restoreGeometry() round-trips the user's last choice.
    QSettings().setValue(kGeometryKey, saveGeometry());
}

void ActivityDetailDialog::done(int result)
{
    saveDialogGeometry();
    QDialog::done(result);
}

void ActivityDetailDialog::closeEvent(QCloseEvent *event)
{
    saveDialogGeometry();
    QDialog::closeEvent(event);
}

void ActivityDetailDialog::buildGraph(const WorkoutHistoryDetail &detail)
{
    m_plot = new QwtPlot(this);
    m_plot->setAxisTitle(QwtPlot::xBottom, tr("Elapsed (min)"));
    m_plot->setAxisTitle(QwtPlot::yLeft, tr("Power (W) / HR (bpm) / Cadence (rpm)"));
    m_plot->insertLegend(new QwtLegend(), QwtPlot::BottomLegend);

    auto *grid = new QwtPlotGrid();
    grid->setMajorPen(QPen(Qt::gray, 0, Qt::DashLine));
    grid->attach(m_plot);

    // Match the in-workout graph colours: power = yellow, HR = red, cadence = blue.
    attachChannel(m_plot, detail.records, &ActivityRecordPoint::power,     tr("Power"),      Util::getColor(Util::LINE_POWER));
    attachChannel(m_plot, detail.records, &ActivityRecordPoint::heartRate, tr("Heart Rate"), Util::getColor(Util::LINE_HEARTRATE));
    attachChannel(m_plot, detail.records, &ActivityRecordPoint::cadence,   tr("Cadence"),    Util::getColor(Util::LINE_CADENCE));

    m_plot->replot();
}

void ActivityDetailDialog::buildLapTable(const WorkoutHistoryDetail &detail)
{
    if (detail.laps.isEmpty())
        return;

    const QStringList headers = {tr("Lap"), tr("Duration"), tr("Avg W"), tr("NP"),
                                 tr("Max W"), tr("Avg HR"), tr("Avg Cad"), tr("Distance")};

    m_lapTable = new QTableWidget(detail.laps.size(), headers.size(), this);
    m_lapTable->setHorizontalHeaderLabels(headers);
    m_lapTable->verticalHeader()->hide();
    m_lapTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lapTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_lapTable->setAlternatingRowColors(true);
    m_lapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    auto cell = [](const QString &text) {
        auto *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        return item;
    };

    for (int i = 0; i < detail.laps.size(); ++i) {
        const ActivityLap &lap = detail.laps.at(i);
        m_lapTable->setItem(i, 0, cell(QString::number(i + 1)));
        m_lapTable->setItem(i, 1, cell(fmtDuration(lap.durationSec)));
        m_lapTable->setItem(i, 2, cell(QString::number(lap.avgPowerW)));
        m_lapTable->setItem(i, 3, cell(lap.normalizedPower > 0 ? QString::number(lap.normalizedPower) : QStringLiteral("-")));
        m_lapTable->setItem(i, 4, cell(QString::number(lap.maxPowerW)));
        m_lapTable->setItem(i, 5, cell(lap.avgHrBpm > 0 ? QString::number(lap.avgHrBpm) : QStringLiteral("-")));
        m_lapTable->setItem(i, 6, cell(lap.avgCadence > 0 ? QString::number(lap.avgCadence) : QStringLiteral("-")));
        m_lapTable->setItem(i, 7, cell(QStringLiteral("%1 km").arg(lap.distanceKm, 0, 'f', 2)));
    }
}
