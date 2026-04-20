#include "planadherencewidget.h"
#include "planadherencestore.h"

#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QInputDialog>
#include <QDate>
#include <QColor>
#include <QFont>
#include <QFrame>

// ── PlanAdherenceModel ───────────────────────────────────────────────────────

PlanAdherenceModel::PlanAdherenceModel(QObject *parent) : QAbstractTableModel(parent) {}

void PlanAdherenceModel::setEntries(const QList<PlanAdherenceEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

int PlanAdherenceModel::rowCount(const QModelIndex &) const    { return m_entries.size(); }
int PlanAdherenceModel::columnCount(const QModelIndex &) const { return 4; }

const PlanAdherenceEntry &PlanAdherenceModel::entryAt(int row) const
{
    return m_entries.at(row);
}

QVariant PlanAdherenceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return tr("Date");
    case 1: return tr("Workout");
    case 2: return tr("Status");
    case 3: return tr("Note");
    }
    return {};
}

QVariant PlanAdherenceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};

    const PlanAdherenceEntry &e = m_entries.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return e.date.toString(QStringLiteral("yyyy-MM-dd"));
        case 1: return e.workoutName;
        case 2: return statusLabel(e.status);
        case 3: return e.note;
        }
    } else if (role == Qt::ForegroundRole && index.column() == 2) {
        return statusColor(e.status);
    } else if (role == Qt::UserRole) {
        // Used for sorting column 0 by date
        if (index.column() == 0) return e.date.toJulianDay();
    }
    return {};
}

QString PlanAdherenceModel::statusLabel(PlanAdherenceEntry::Status s)
{
    switch (s) {
    case PlanAdherenceEntry::Completed:   return QObject::tr("Completed");
    case PlanAdherenceEntry::Skipped:     return QObject::tr("Skipped");
    case PlanAdherenceEntry::Substituted: return QObject::tr("Substituted");
    }
    return {};
}

QColor PlanAdherenceModel::statusColor(PlanAdherenceEntry::Status s)
{
    switch (s) {
    case PlanAdherenceEntry::Completed:   return QColor(0x2e, 0x7d, 0x32); // green
    case PlanAdherenceEntry::Skipped:     return QColor(0x75, 0x75, 0x75); // grey
    case PlanAdherenceEntry::Substituted: return QColor(0xe6, 0x5c, 0x00); // amber
    }
    return {};
}

// ── PlanAdherenceWidget ──────────────────────────────────────────────────────

PlanAdherenceWidget::PlanAdherenceWidget(PlanAdherenceStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    setupUi();
    connect(store, &PlanAdherenceStore::storeChanged, this, &PlanAdherenceWidget::refresh);
    refresh();
}

void PlanAdherenceWidget::setupUi()
{
    m_model = new PlanAdherenceModel(this);

    m_tableView = new QTableView(this);
    m_tableView->setModel(m_model);
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->hide();
    m_tableView->sortByColumn(0, Qt::DescendingOrder);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested,
            this, &PlanAdherenceWidget::showContextMenu);

    // Summary card
    auto *cardFrame = new QFrame(this);
    cardFrame->setFrameShape(QFrame::StyledPanel);
    auto *cardLayout = new QVBoxLayout(cardFrame);
    cardLayout->setContentsMargins(8, 6, 8, 6);
    cardLayout->setSpacing(4);

    QFont boldFont = font();
    boldFont.setBold(true);

    auto *summaryTitle = new QLabel(tr("<b>30-Day Adherence Summary</b>"), this);
    m_summaryLbl = new QLabel(this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(true);
    m_progressBar->setMaximumHeight(16);

    cardLayout->addWidget(summaryTitle);
    cardLayout->addWidget(m_summaryLbl);
    cardLayout->addWidget(m_progressBar);

    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_refreshBtn->setMaximumWidth(120);
    connect(m_refreshBtn, &QPushButton::clicked, this, &PlanAdherenceWidget::refresh);

    auto *toolBar = new QHBoxLayout();
    toolBar->addWidget(m_refreshBtn);
    toolBar->addStretch();

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);
    mainLayout->addWidget(cardFrame);
    mainLayout->addLayout(toolBar);
    mainLayout->addWidget(m_tableView);
}

void PlanAdherenceWidget::refresh()
{
    m_model->setEntries(m_store->entries());
    updateSummary();
}

void PlanAdherenceWidget::updateSummary()
{
    const int total       = m_store->totalCount();
    const int completed   = m_store->completedCount();
    const int skipped     = m_store->skippedCount();
    const int substituted = m_store->substitutedCount();
    const double pct30d   = m_store->adherencePctRecent(30);

    m_summaryLbl->setText(
        tr("Total: %1   ✓ Completed: %2   ✗ Skipped: %3   ~ Substituted: %4")
            .arg(total).arg(completed).arg(skipped).arg(substituted));

    m_progressBar->setValue(static_cast<int>(pct30d + 0.5));
    m_progressBar->setFormat(
        tr("%1% completion (last 30 days)").arg(static_cast<int>(pct30d + 0.5)));
}

void PlanAdherenceWidget::showContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_tableView->indexAt(pos);
    if (!idx.isValid()) return;

    QMenu menu(this);
    QAction *actSkip  = menu.addAction(tr("Mark as Skipped…"));
    QAction *actSub   = menu.addAction(tr("Mark as Substituted…"));
    menu.addSeparator();
    QAction *actRemove = menu.addAction(tr("Remove Entry"));

    connect(actSkip,   &QAction::triggered, this, &PlanAdherenceWidget::markSkipped);
    connect(actSub,    &QAction::triggered, this, &PlanAdherenceWidget::markSubstituted);
    connect(actRemove, &QAction::triggered, this, &PlanAdherenceWidget::removeEntry);

    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

void PlanAdherenceWidget::markSkipped()
{
    const QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) return;

    // Map through sort proxy — model index is direct since no proxy here
    const PlanAdherenceEntry &e = m_model->entryAt(idx.row());

    bool ok = false;
    const QString note = QInputDialog::getText(
        this, tr("Mark as Skipped"),
        tr("Optional note for skipping "%1" on %2:")
            .arg(e.workoutName, e.date.toString(Qt::DefaultLocaleShortDate)),
        QLineEdit::Normal, QString(), &ok);

    if (ok)
        m_store->addSkipped(e.date, e.workoutName, note);
}

void PlanAdherenceWidget::markSubstituted()
{
    const QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) return;

    const PlanAdherenceEntry &e = m_model->entryAt(idx.row());

    bool ok = false;
    const QString note = QInputDialog::getText(
        this, tr("Mark as Substituted"),
        tr("What did you do instead of "%1" on %2?")
            .arg(e.workoutName, e.date.toString(Qt::DefaultLocaleShortDate)),
        QLineEdit::Normal, QString(), &ok);

    if (ok)
        m_store->addSubstituted(e.date, e.workoutName, note);
}

void PlanAdherenceWidget::removeEntry()
{
    const QModelIndex idx = m_tableView->currentIndex();
    if (!idx.isValid()) return;

    const PlanAdherenceEntry &e = m_model->entryAt(idx.row());
    m_store->remove(e.date, e.workoutName);
}
