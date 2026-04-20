#ifndef MAIN_WORKOUTPAGE_H
#define MAIN_WORKOUTPAGE_H

#include <QWidget>
#include <QLabel>
#include <QMenu>
#include <QNetworkReply>

#include "workouttablemodel.h"
#include "sortfilterproxymodel.h"
#include "delegaterowhover.h"
#include "xmlutil.h"
#include "settings.h"
#include "radio.h"



namespace Ui {
class Main_WorkoutPage;
}

class Main_WorkoutPage : public QWidget
{
    Q_OBJECT

public:
    explicit Main_WorkoutPage(QWidget *parent = 0);
    ~Main_WorkoutPage();


    void parseIncludedWorkouts();
    void parseMapWorkout(int userFTP);
    void parseUserWorkouts();

    void paintEvent(QPaintEvent *);




signals :
    void signal_exportWorkoutToPdf(const Workout& workout);
    void editWorkout(Workout);

    void executeWorkout(Workout);
    void addWorkoutToQueue(const Workout &workout);





public slots:
    void filterChanged(const QString& field, const QString& value);
    void filterChangedList(int id);
    void filterChangedWorkoutType(bool includedWorkout);

    void setFilterPlanName(const QString& name);
    void setFilterWorkoutName(const QString& name);

    void refreshUserWorkout();
    void refreshMapWorkout();

    void saveFilterFields();
    void loadFilterFields();



private slots:

    void connectWebChannelWorkout();
    void fillWorkoutPage();


    void on_tableView_workout_doubleClicked(const QModelIndex &index);

    void customMenuRequested(const QPoint& p);
    void tableViewSelectionChanged(QItemSelection,QItemSelection);
    void editWorkout();
    void deleteWorkout();
    void setAsDone();
    void openFolderWorkout();
    void exportToPdf();
    void addToQueue();
    void addWorkout(const Workout&);
    void overwriteWorkout(const Workout&);

    void updateTableViewMetrics();


    void on_checkBox_clicked(bool checked);

    void on_pushButton_refresh_clicked();




private:
    QString nameFilter;
    QString planFilter;
    QString creatorFilter;
    int typeFilter;


    Ui::Main_WorkoutPage *ui;

    XmlUtil *xmlUtil;
    Account *account;
    Settings *settings;

    WorkoutTableModel *tableModel;
    SortFilterProxyModel *proxyModel;
    delegateRowHover *delegateRow;

    QMenu *contextMenu;
    QAction *actionEdit;
    QAction *actionDelete;
    QAction *actionSetAsDone;
    QAction *actionOpenFolder;
    QAction *actionExportPDF;
    QAction *actionAddToQueue;
    QModelIndex indexSourceSelected;
};

#endif // MAIN_WORKOUTPAGE_H
