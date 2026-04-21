#include "historywidget.h"
#include "workouthistorymodel.h"
#include "fitactivityreader.h"
#include "planadherencewidget.h"
#include "planadherencestore.h"
#include "criticalpowerdialog.h"
#include "pmccalculator.h"
#include "pmcdialog.h"
#include "util.h"

#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDir>
#include <QStringList>
#include <QApplication>
#include <QWidget>

HistoryWidget::HistoryWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void HistoryWidget::setupUi()
{
    m_tabs = new QTabWidget(this);

    // ── Workout History tab ──────────────────────────────────────────────────
    auto *histTab = new QWidget(this);
    m_model = new WorkoutHistoryModel(this);

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::UserRole);

    m_tableView = new QTableView(histTab);
    m_tableView->setModel(m_proxy);
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->hide();
    m_tableView->sortByColumn(0, Qt::DescendingOrder);

    m_refreshBtn = new QPushButton(tr("Refresh"), histTab);
    m_refreshBtn->setMaximumWidth(120);
    connect(m_refreshBtn, &QPushButton::clicked, this, &HistoryWidget::loadHistory);

    m_cpBtn = new QPushButton(tr("Critical Power Curve"), histTab);
    connect(m_cpBtn, &QPushButton::clicked, this, &HistoryWidget::openCriticalPowerDialog);

    m_pmcBtn = new QPushButton(tr("Performance Chart"), histTab);
    m_pmcBtn->setMaximumWidth(160);
    connect(m_pmcBtn, &QPushButton::clicked, this, &HistoryWidget::openPmcDialog);

    m_statusLabel = new QLabel(histTab);

    auto *toolBar = new QHBoxLayout();
    toolBar->addWidget(m_refreshBtn);
    toolBar->addWidget(m_cpBtn);
    toolBar->addWidget(m_pmcBtn);
    toolBar->addWidget(m_statusLabel);
    toolBar->addStretch();

    auto *histLayout = new QVBoxLayout(histTab);
    histLayout->setContentsMargins(8, 8, 8, 8);
    histLayout->setSpacing(6);
    histLayout->addLayout(toolBar);
    histLayout->addWidget(m_tableView);

    m_tabs->addTab(histTab, tr("Activity History"));

    // Plan Adherence tab placeholder — added in setAdherenceStore()

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_tabs);
}

void HistoryWidget::setAdherenceStore(PlanAdherenceStore *store)
{
    if (!store || m_adherenceWidget) return;
    m_adherenceWidget = new PlanAdherenceWidget(store, this);
    m_tabs->addTab(m_adherenceWidget, tr("Plan Adherence"));
}

void HistoryWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_loaded)
        loadHistory();
}

void HistoryWidget::loadHistory()
{
    m_loaded = true;
    m_statusLabel->setText(tr("Loading…"));
    qApp->processEvents();

    const QString historyPath = Util::getSystemPathHistory();
    QDir dir(historyPath);
    if (!dir.exists()) {
        m_statusLabel->setText(tr("History folder not found: %1").arg(historyPath));
        return;
    }

    const QStringList fitFiles = dir.entryList({QStringLiteral("*.fit")}, QDir::Files);

    QList<WorkoutHistorySummary> summaries;
    summaries.reserve(fitFiles.size());

    for (const QString &fileName : fitFiles) {
        WorkoutHistorySummary s = FitActivityReader::readFile(dir.filePath(fileName));
        if (s.valid || !s.workoutName.isEmpty())
            summaries.append(s);
    }

    m_model->setHistory(summaries);

    if (summaries.isEmpty())
        m_statusLabel->setText(tr("No activities found in %1").arg(historyPath));
    else
        m_statusLabel->setText(tr("%n activity(ies)", "", summaries.size()));
}

void HistoryWidget::openCriticalPowerDialog()
{
    if (!m_loaded)
        loadHistory();

    const QList<WorkoutHistorySummary> &history = m_model->history();
    CriticalPowerDialog dlg(history, this);
    dlg.exec();
}

void HistoryWidget::openPmcDialog()
{
    if (!m_loaded)
        loadHistory();

    const QList<PmcPoint> points = PmcCalculator::compute(m_model->history());
    auto *dlg = new PmcDialog(points, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}
