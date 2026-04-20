#include "historywidget.h"
#include "workouthistorymodel.h"
#include "fitactivityreader.h"
#include "pmccalculator.h"
#include "pmcdialog.h"
#include "util.h"

#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDir>
#include <QStringList>
#include <QApplication>

HistoryWidget::HistoryWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void HistoryWidget::setupUi()
{
    m_model = new WorkoutHistoryModel(this);

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::DisplayRole);

    m_tableView = new QTableView(this);
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

    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_refreshBtn->setMaximumWidth(120);
    connect(m_refreshBtn, &QPushButton::clicked, this, &HistoryWidget::loadHistory);

    m_pmcBtn = new QPushButton(tr("Performance Chart"), this);
    m_pmcBtn->setMaximumWidth(160);
    connect(m_pmcBtn, &QPushButton::clicked, this, &HistoryWidget::openPmcDialog);

    m_statusLabel = new QLabel(this);

    auto *toolBar = new QHBoxLayout();
    toolBar->addWidget(m_refreshBtn);
    toolBar->addWidget(m_pmcBtn);
    toolBar->addWidget(m_statusLabel);
    toolBar->addStretch();

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);
    mainLayout->addLayout(toolBar);
    mainLayout->addWidget(m_tableView);
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

void HistoryWidget::openPmcDialog()
{
    if (!m_loaded)
        loadHistory();

    const QList<PmcPoint> points = PmcCalculator::compute(m_model->history());
    auto *dlg = new PmcDialog(points, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}
