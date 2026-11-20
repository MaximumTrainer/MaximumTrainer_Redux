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
class QModelIndex;
class QMenu;
class QAction;
class QPoint;
class WorkoutHistoryModel;
class HistoryFilterProxyModel;

class HistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWidget(QWidget *parent = nullptr);


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
    void showHistoryContextMenu(const QPoint &pos);

private:
    void setupUi();

    /// Map a proxy-model index to its WorkoutHistorySummary; returns nullptr if invalid.
    const WorkoutHistorySummary *summaryForProxyRow(const QModelIndex &proxyIndex) const;

    /// Refresh the status label with the visible-vs-total activity count.
    void updateStatus();


    // Workout history tab
    QTableView              *m_tableView     = nullptr;
    WorkoutHistoryModel     *m_model         = nullptr;
    HistoryFilterProxyModel *m_proxy         = nullptr;
    QLabel                  *m_statusLabel   = nullptr;
    class WeeklySummaryWidget *m_weekly      = nullptr;
    QPushButton             *m_cpBtn         = nullptr;
    QPushButton             *m_pmcBtn        = nullptr;
    QLineEdit               *m_searchEdit    = nullptr;
    QDateEdit               *m_fromDate      = nullptr;
    QDateEdit               *m_toDate        = nullptr;
    QMenu                   *m_contextMenu   = nullptr;
    QAction                 *m_deleteAction  = nullptr;


    bool                  m_loaded        = false;
    bool                  m_dateRangeInitialized = false;

    void resetDateRange();
};

#endif // HISTORYWIDGET_H
