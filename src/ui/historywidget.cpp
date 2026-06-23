#include "historywidget.h"
#include "workouthistorymodel.h"
#include "historyfilterproxymodel.h"
#include "activitydetaildialog.h"
#include "fitactivityreader.h"
#include "fitactivitydetailreader.h"
#include "planadherencewidget.h"
#include "planadherencestore.h"
#include "criticalpowerdialog.h"
#include "pmccalculator.h"
#include "pmcdialog.h"
#include "util.h"

#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QDateEdit>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
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

    m_proxy = new HistoryFilterProxyModel(this);
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
    connect(m_tableView, &QTableView::doubleClicked, this, &HistoryWidget::openDetail);

    m_refreshBtn = new QPushButton(tr("Refresh"), histTab);
    m_refreshBtn->setMaximumWidth(120);
    connect(m_refreshBtn, &QPushButton::clicked, this, &HistoryWidget::loadHistory);

    m_cpBtn = new QPushButton(tr("Critical Power Curve"), histTab);
    connect(m_cpBtn, &QPushButton::clicked, this, &HistoryWidget::openCriticalPowerDialog);

    m_pmcBtn = new QPushButton(tr("Performance Chart"), histTab);
    m_pmcBtn->setMaximumWidth(160);
    connect(m_pmcBtn, &QPushButton::clicked, this, &HistoryWidget::openPmcDialog);

    m_deleteBtn = new QPushButton(tr("Delete"), histTab);
    m_deleteBtn->setMaximumWidth(120);
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &HistoryWidget::deleteSelected);

    m_statusLabel = new QLabel(histTab);

    auto *toolBar = new QHBoxLayout();
    toolBar->addWidget(m_refreshBtn);
    toolBar->addWidget(m_cpBtn);
    toolBar->addWidget(m_pmcBtn);
    toolBar->addWidget(m_deleteBtn);
    toolBar->addWidget(m_statusLabel);
    toolBar->addStretch();

    // ── Search + date-range filter row ───────────────────────────────────────
    m_searchEdit = new QLineEdit(histTab);
    m_searchEdit->setPlaceholderText(tr("Search by workout name…"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &HistoryWidget::applyFilters);

    // Defaults span everything (year 2000 .. today) so the date filter is a
    // no-op until the user narrows it. clearFilters() restores these.
    m_fromDate = new QDateEdit(histTab);
    m_fromDate->setCalendarPopup(true);
    m_fromDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_fromDate->setDate(QDate(2000, 1, 1));
    m_toDate = new QDateEdit(histTab);
    m_toDate->setCalendarPopup(true);
    m_toDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_toDate->setDate(QDate::currentDate());
    connect(m_fromDate, &QDateEdit::dateChanged, this, &HistoryWidget::applyFilters);
    connect(m_toDate,   &QDateEdit::dateChanged, this, &HistoryWidget::applyFilters);

    auto *clearBtn = new QPushButton(tr("Clear filters"), histTab);
    connect(clearBtn, &QPushButton::clicked, this, &HistoryWidget::clearFilters);

    auto *filterBar = new QHBoxLayout();
    filterBar->addWidget(m_searchEdit, 1);
    filterBar->addWidget(new QLabel(tr("From:"), histTab));
    filterBar->addWidget(m_fromDate);
    filterBar->addWidget(new QLabel(tr("To:"), histTab));
    filterBar->addWidget(m_toDate);
    filterBar->addWidget(clearBtn);

    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
                m_deleteBtn->setEnabled(m_tableView->selectionModel()->hasSelection());
            });

    auto *histLayout = new QVBoxLayout(histTab);
    histLayout->setContentsMargins(8, 8, 8, 8);
    histLayout->setSpacing(6);
    histLayout->addLayout(toolBar);
    histLayout->addLayout(filterBar);
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

void HistoryWidget::applyFilters()
{
    if (!m_proxy)
        return;
    m_proxy->setNameFilter(m_searchEdit->text());
    m_proxy->setDateRange(m_fromDate->date(), m_toDate->date());
}

void HistoryWidget::clearFilters()
{
    m_searchEdit->clear();
    m_fromDate->setDate(QDate(2000, 1, 1));
    m_toDate->setDate(QDate::currentDate());
    applyFilters();
}

const WorkoutHistorySummary *HistoryWidget::summaryForProxyRow(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid())
        return nullptr;
    const QModelIndex src = m_proxy->mapToSource(proxyIndex);
    const int row = src.row();
    const QList<WorkoutHistorySummary> &history = m_model->history();
    if (row < 0 || row >= history.size())
        return nullptr;
    return &history.at(row);
}

void HistoryWidget::openDetail(const QModelIndex &proxyIndex)
{
    const WorkoutHistorySummary *summary = summaryForProxyRow(proxyIndex);
    if (!summary)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const WorkoutHistoryDetail detail = FitActivityDetailReader::readFile(summary->filePath);
    QApplication::restoreOverrideCursor();

    if (!detail.valid) {
        QMessageBox::information(this, tr("Activity Detail"),
                                 tr("No detailed record data could be read from this activity."));
        return;
    }

    auto *dlg = new ActivityDetailDialog(*summary, detail, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

void HistoryWidget::deleteSelected()
{
    const QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    const WorkoutHistorySummary *summary = summaryForProxyRow(selected.first());
    if (!summary)
        return;

    const QString filePath = summary->filePath;
    const QString name = summary->workoutName;

    const auto answer = QMessageBox::question(
        this, tr("Delete Activity"),
        tr("Permanently delete this activity from disk?\n\n%1\n%2")
            .arg(name, QFileInfo(filePath).fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes)
        return;

    if (!QFile::remove(filePath)) {
        QMessageBox::warning(this, tr("Delete Activity"),
                             tr("Could not delete the file:\n%1").arg(filePath));
        return;
    }

    loadHistory();
}
