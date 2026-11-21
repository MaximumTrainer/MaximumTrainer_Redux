#include "criticalpowerdialog.h"

#include "mmpcalculator.h"
#include "personalbesttracker.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QTableWidget>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#ifndef GC_WASM_BUILD
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#endif

#include "qwt_plot.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_grid.h"
#include "qwt_scale_engine.h"
#include "qwt_scale_div.h"
#include "qwt_scale_draw.h"
#include "qwt_text.h"
#include "qwt_symbol.h"

// Custom scale draw for human-readable duration labels
class DurationScaleDraw : public QwtScaleDraw
{
public:
    QwtText label(double value) const override
    {
        int sec = qRound(value);
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

// CriticalPowerDialog

namespace {

// Durations listed in the best-efforts table.
const QVector<int> kTableDurations{5, 60, 300, 1200, 3600};

// Period choices: days back, 0 = all time.
struct Period { const char *label; int days; };
const Period kPeriods[] = {
    {QT_TRANSLATE_NOOP("CriticalPowerDialog", "Last 30 days"), 30},
    {QT_TRANSLATE_NOOP("CriticalPowerDialog", "Last 90 days"), 90},
    {QT_TRANSLATE_NOOP("CriticalPowerDialog", "Last year"), 365},
    {QT_TRANSLATE_NOOP("CriticalPowerDialog", "All time"), 0},
};
constexpr int kDefaultPeriodIndex = 1;

} // namespace

CriticalPowerDialog::CriticalPowerDialog(const QList<WorkoutHistorySummary> &history,
                                         QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Power Curve"));
    setMinimumSize(720, 620);
    setupUi();
    loadRides(history);
}

void CriticalPowerDialog::setupUi()
{
    m_periodCombo = new QComboBox(this);
    for (const Period &period : kPeriods)
        m_periodCombo->addItem(tr(period.label), period.days);
    m_periodCombo->setCurrentIndex(kDefaultPeriodIndex);
    connect(m_periodCombo, &QComboBox::currentIndexChanged, this, &CriticalPowerDialog::showPeriod);

    auto *periodRow = new QHBoxLayout;
    periodRow->addWidget(new QLabel(tr("Period:"), this));
    periodRow->addWidget(m_periodCombo);
    periodRow->addStretch();

    m_plot = new QwtPlot(this);
    m_plot->setAxisTitle(QwtPlot::xBottom, tr("Duration"));
    m_plot->setAxisTitle(QwtPlot::yLeft,   tr("Power (W)"));
    m_plot->setAxisScaleEngine(QwtPlot::xBottom, new QwtLogScaleEngine());
    m_plot->setAxisScaleDraw(QwtPlot::xBottom, new DurationScaleDraw());
    const QPalette appPalette = QApplication::palette();
    m_plot->setCanvasBackground(appPalette.color(QPalette::Base));

    auto *grid = new QwtPlotGrid();
    grid->setMajorPen(appPalette.color(QPalette::Mid), 1, Qt::DashLine);
    grid->attach(m_plot);

    m_allTimeCurve = new QwtPlotCurve(tr("All time"));
    m_allTimeCurve->setStyle(QwtPlotCurve::Lines);
    m_allTimeCurve->setPen(appPalette.color(QPalette::Mid), 2, Qt::DashLine);
    m_allTimeCurve->attach(m_plot);

    m_periodCurve = new QwtPlotCurve(tr("Period"));
    m_periodCurve->setStyle(QwtPlotCurve::Lines);
    m_periodCurve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
                                           QBrush(QColor(0, 114, 189)),
                                           QPen(QColor(0, 114, 189)), QSize(6, 6)));
    m_periodCurve->setPen(QColor(0, 114, 189), 2);
    m_periodCurve->attach(m_plot);

    m_cpCurve = new QwtPlotCurve(tr("CP Model"));
    m_cpCurve->setStyle(QwtPlotCurve::Lines);
    m_cpCurve->setPen(QColor(217, 83, 25), 2, Qt::DotLine);
    m_cpCurve->attach(m_plot);

    auto *statsFrame = new QFrame(this);
    statsFrame->setFrameShape(QFrame::StyledPanel);
    auto *statsLayout = new QHBoxLayout(statsFrame);
    m_cpLabel = new QLabel(statsFrame);
    m_cpLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));
    m_wPrimeLabel = new QLabel(statsFrame);
    m_wPrimeLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));
    statsLayout->addStretch();
    statsLayout->addWidget(m_cpLabel);
    statsLayout->addSpacing(24);
    statsLayout->addWidget(m_wPrimeLabel);
    statsLayout->addStretch();

    m_table = new QTableWidget(kTableDurations.size(), 4, this);
    m_table->setHorizontalHeaderLabels({tr("Best"), tr("Date"), tr("Workout"), tr("All-time best")});
    QStringList rowLabels;
    for (int seconds : kTableDurations)
        rowLabels << PersonalBestTracker::durationLabel(seconds);
    m_table->setVerticalHeaderLabels(rowLabels);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_statusLabel = new QLabel(this);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(periodRow);
    mainLayout->addWidget(m_plot, 1);
    mainLayout->addWidget(statsFrame);
    mainLayout->addWidget(m_table);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(buttonBox);
    updateLabels({});
}

void CriticalPowerDialog::loadRides(const QList<WorkoutHistorySummary> &history)
{
    if (history.isEmpty()) {
        m_statusLabel->setText(tr("No activities to analyse."));
        return;
    }
    m_statusLabel->setText(tr("Reading %n ride(s)...", "", history.size()));
    m_periodCombo->setEnabled(false);
    const auto done = [this](const QList<PowerBests::Ride> &rides) {
        m_rides = rides;
        m_loaded = true;
        m_periodCombo->setEnabled(true);
        // Open on the shortest period, from the default up, that has a ride.
        const QDateTime now = QDateTime::currentDateTime();
        for (int index = kDefaultPeriodIndex; index < m_periodCombo->count(); ++index) {
            const int days = m_periodCombo->itemData(index).toInt();
            const bool hasRide = std::any_of(m_rides.cbegin(), m_rides.cend(), [&](const PowerBests::Ride &ride) {
                return days == 0 || (ride.summary.startTime.isValid() && ride.summary.startTime >= now.addDays(-days));
            });
            if (hasRide) {
                const QSignalBlocker blocker(m_periodCombo);
                m_periodCombo->setCurrentIndex(index);
                break;
            }
        }
        showPeriod();
    };
#ifdef GC_WASM_BUILD
    done(PowerBests::loadRides(history));
#else
    // New rides are decoded once, then cached; keep the dialog responsive meanwhile.
    auto *watcher = new QFutureWatcher<QList<PowerBests::Ride>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [watcher, done] {
        done(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&PowerBests::loadRides, history));
#endif
}

void CriticalPowerDialog::showPeriod()
{
    if (!m_loaded)
        return;
    const int days = m_periodCombo->currentData().toInt();
    const QDateTime since = days > 0 ? QDateTime::currentDateTime().addDays(-days) : QDateTime();
    const QVector<PowerBests::Effort> period = PowerBests::bestEfforts(m_rides, since);
    const QVector<PowerBests::Effort> allTime = PowerBests::bestEfforts(m_rides);

    QVector<double> watts;
    for (const PowerBests::Effort &effort : period)
        watts.append(effort.watts);
    const CriticalPowerModel model = MmpCalculator::fitCriticalPower(MMP_DURATIONS, watts);

    m_cpCurve->setSamples(QVector<double>{}, QVector<double>{});
    if (model.valid) {
        QVector<double> xs, ys;
        for (qsizetype index = 0; index < MMP_DURATIONS.size(); ++index) {
            // The model is fitted on 2 to 30 min efforts; shorter, W'/t explodes.
            if (period[index].watts > 0 && MMP_DURATIONS[index] >= 120) {
                const double seconds = MMP_DURATIONS[index];
                xs.append(seconds);
                ys.append(model.cp + model.wPrime / seconds);
            }
        }
        m_cpCurve->setSamples(xs, ys);
    }
    updatePlot(period, allTime, days > 0);
    updateTable(period, allTime, days > 0);
    updateLabels(model);

    int ridesInPeriod = 0;
    for (const PowerBests::Ride &ride : m_rides)
        if (!since.isValid() || (ride.summary.startTime.isValid() && ride.summary.startTime >= since))
            ++ridesInPeriod;
    m_statusLabel->setText(ridesInPeriod > 0
        ? tr("Based on %n ride(s). ★ = also your all-time best.", "", ridesInPeriod)
        : tr("No rides in this period."));
}

void CriticalPowerDialog::updatePlot(const QVector<PowerBests::Effort> &period,
                                     const QVector<PowerBests::Effort> &allTime, bool comparing)
{
    const auto samples = [](const QVector<PowerBests::Effort> &efforts, QVector<double> &xs, QVector<double> &ys) {
        for (qsizetype index = 0; index < MMP_DURATIONS.size() && index < efforts.size(); ++index) {
            if (efforts[index].watts > 0) {
                xs.append(MMP_DURATIONS[index]);
                ys.append(efforts[index].watts);
            }
        }
    };
    QVector<double> periodX, periodY, allX, allY;
    samples(period, periodX, periodY);
    samples(allTime, allX, allY);
    m_periodCurve->setSamples(periodX, periodY);
    m_allTimeCurve->setSamples(comparing ? allX : QVector<double>{}, comparing ? allY : QVector<double>{});
    if (!allX.isEmpty()) {
        // Ticks at the durations riders think in, instead of powers of ten.
        QList<double> ticks;
        for (double seconds : {1.0, 5.0, 15.0, 60.0, 300.0, 1200.0, 3600.0, 7200.0})
            if (seconds >= allX.first() && seconds <= allX.last())
                ticks << seconds;
        QwtScaleDiv division(allX.first(), allX.last());
        division.setTicks(QwtScaleDiv::MajorTick, ticks);
        m_plot->setAxisScaleDiv(QwtPlot::xBottom, division);
    }
    m_plot->setTitle(comparing ? tr("%1 (solid) vs all time (dashed)").arg(m_periodCombo->currentText())
                               : tr("All time"));
    m_plot->replot();
}

void CriticalPowerDialog::updateTable(const QVector<PowerBests::Effort> &period,
                                      const QVector<PowerBests::Effort> &allTime, bool comparing)
{
    const auto cell = [this](int row, int column, const QString &text, bool bold = false) {
        auto *item = new QTableWidgetItem(text);
        if (bold) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
        m_table->setItem(row, column, item);
    };
    const QString none = QStringLiteral("-");
    for (qsizetype row = 0; row < kTableDurations.size(); ++row) {
        const qsizetype index = MMP_DURATIONS.indexOf(kTableDurations[row]);
        const PowerBests::Effort &effort = period[index];
        const PowerBests::Effort &best = allTime[index];
        const int r = static_cast<int>(row);
        if (effort.watts <= 0) {
            cell(r, 0, none);
            cell(r, 1, none);
            cell(r, 2, none);
        } else {
            // A period best that is also the all-time best gets a star.
            const bool isAllTime = comparing && qRound(effort.watts) >= qRound(best.watts);
            cell(r, 0, QStringLiteral("%1 W%2").arg(qRound(effort.watts)).arg(isAllTime ? QStringLiteral("  ★") : QString()), true);
            cell(r, 1, QLocale().toString(effort.date.date(), QLocale::ShortFormat));
            cell(r, 2, effort.workoutName);
        }
        cell(r, 3, best.watts > 0 ? QStringLiteral("%1 W").arg(qRound(best.watts)) : none);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    int height = m_table->horizontalHeader()->height() + 2 * m_table->frameWidth();
    for (int row = 0; row < m_table->rowCount(); ++row)
        height += m_table->rowHeight(row);
    m_table->setFixedHeight(height);
}

void CriticalPowerDialog::updateLabels(const CriticalPowerModel &model)
{
    if (model.valid) {
        m_cpLabel->setText(tr("CP: %1 W").arg(qRound(model.cp)));
        m_wPrimeLabel->setText(tr("W': %1 kJ").arg(model.wPrime / 1000.0, 0, 'f', 1));
    } else {
        m_cpLabel->setText(tr("CP: -"));
        m_wPrimeLabel->setText(tr("W': -  (needs 3+ efforts between 2 and 30 min)"));
    }
}
