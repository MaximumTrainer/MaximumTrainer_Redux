#ifndef HISTORYWIDGET_H
#define HISTORYWIDGET_H

#include <QWidget>
#include <QList>

#include "workouthistorysummary.h"

class QTableView;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class WorkoutHistoryModel;

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

private:
    void setupUi();

    QTableView           *m_tableView       = nullptr;
    WorkoutHistoryModel  *m_model           = nullptr;
    QSortFilterProxyModel*m_proxy           = nullptr;
    QLabel               *m_statusLabel     = nullptr;
    QPushButton          *m_refreshBtn      = nullptr;
    QPushButton          *m_cpBtn           = nullptr;
    bool                  m_loaded          = false;
};

#endif // HISTORYWIDGET_H
