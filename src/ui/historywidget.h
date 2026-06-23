#ifndef HISTORYWIDGET_H
#define HISTORYWIDGET_H

#include <QWidget>
#include <QList>

#include "workouthistorysummary.h"

class QTableView;
class QLabel;
class QPushButton;
class QLineEdit;
class QDateEdit;
class QTabWidget;
class QModelIndex;
class WorkoutHistoryModel;
class HistoryFilterProxyModel;
class PlanAdherenceStore;
class PlanAdherenceWidget;

class HistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWidget(QWidget *parent = nullptr);

    /// Inject the adherence store (called from MainWindow after store is created).
    void setAdherenceStore(PlanAdherenceStore *store);

public slots:
    void loadHistory();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void openCriticalPowerDialog();
    void openPmcDialog();
    void applyFilters();
    void clearFilters();
    void deleteSelected();
    void openDetail(const QModelIndex &proxyIndex);

private:
    void setupUi();

    /// Map a proxy-model index to its WorkoutHistorySummary; returns nullptr if invalid.
    const WorkoutHistorySummary *summaryForProxyRow(const QModelIndex &proxyIndex) const;

    QTabWidget              *m_tabs          = nullptr;

    // Workout history tab
    QTableView              *m_tableView     = nullptr;
    WorkoutHistoryModel     *m_model         = nullptr;
    HistoryFilterProxyModel *m_proxy         = nullptr;
    QLabel                  *m_statusLabel   = nullptr;
    QPushButton             *m_refreshBtn    = nullptr;
    QPushButton             *m_cpBtn         = nullptr;
    QPushButton             *m_pmcBtn        = nullptr;
    QPushButton             *m_deleteBtn     = nullptr;
    QLineEdit               *m_searchEdit    = nullptr;
    QDateEdit               *m_fromDate      = nullptr;
    QDateEdit               *m_toDate        = nullptr;

    // Plan adherence tab (added when store is injected)
    PlanAdherenceWidget  *m_adherenceWidget = nullptr;

    bool                  m_loaded        = false;
};

#endif // HISTORYWIDGET_H
