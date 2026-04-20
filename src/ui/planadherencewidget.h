#ifndef PLANADHERENCEWIDGET_H
#define PLANADHERENCEWIDGET_H

#include <QWidget>
#include <QAbstractTableModel>
#include <QList>
#include "planadherence.h"

class QTableView;
class QLabel;
class QPushButton;
class QProgressBar;
class PlanAdherenceStore;

// ── Table model ──────────────────────────────────────────────────────────────

class PlanAdherenceModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit PlanAdherenceModel(QObject *parent = nullptr);

    void setEntries(const QList<PlanAdherenceEntry> &entries);

    int rowCount(const QModelIndex & = {}) const override;
    int columnCount(const QModelIndex & = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    const PlanAdherenceEntry &entryAt(int row) const;

private:
    QList<PlanAdherenceEntry> m_entries;

    static QString statusLabel(PlanAdherenceEntry::Status s);
    static QColor  statusColor(PlanAdherenceEntry::Status s);
};

// ── Widget ───────────────────────────────────────────────────────────────────

/// Plan Adherence tab widget.
///
/// Shows all recorded plan adherence entries in a sortable table, with a
/// summary card (completion %, 30-day rolling) at the top.
/// Right-click context menu lets the user add/edit Skipped or Substituted entries.
class PlanAdherenceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlanAdherenceWidget(PlanAdherenceStore *store, QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void showContextMenu(const QPoint &pos);
    void markSkipped();
    void markSubstituted();
    void removeEntry();

private:
    void setupUi();
    void updateSummary();

    PlanAdherenceStore  *m_store       = nullptr;
    PlanAdherenceModel  *m_model       = nullptr;
    QTableView          *m_tableView   = nullptr;
    QLabel              *m_summaryLbl  = nullptr;
    QProgressBar        *m_progressBar = nullptr;
    QPushButton         *m_refreshBtn  = nullptr;
};

#endif // PLANADHERENCEWIDGET_H
