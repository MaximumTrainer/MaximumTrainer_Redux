#ifndef HISTORYWIDGET_H
#define HISTORYWIDGET_H

#include <QWidget>
#include <QList>

#include "workouthistorysummary.h"

class QTableView;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class QTabWidget;
class WorkoutHistoryModel;
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

private:
    void setupUi();

    QTabWidget           *m_tabs          = nullptr;

    // Workout history tab
    QTableView           *m_tableView     = nullptr;
    WorkoutHistoryModel  *m_model         = nullptr;
    QSortFilterProxyModel*m_proxy         = nullptr;
    QLabel               *m_statusLabel   = nullptr;
    QPushButton          *m_refreshBtn    = nullptr;
    QPushButton          *m_cpBtn         = nullptr;
    QPushButton          *m_pmcBtn        = nullptr;

    // Plan adherence tab (added when store is injected)
    PlanAdherenceWidget  *m_adherenceWidget = nullptr;

    bool                  m_loaded        = false;
};

#endif // HISTORYWIDGET_H
