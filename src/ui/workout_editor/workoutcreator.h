#ifndef WORKOUTCREATOR_H
#define WORKOUTCREATOR_H

#include <QWidget>
#include <QItemSelection>
#include <QMenu>
#include "intervaltablemodel.h"
#include "repeatwidget.h"
#include "interval.h"
#include "workout.h"
#include "account.h"


namespace Ui {
class WorkoutCreator;
}

class WorkoutCreator : public QWidget
{
    Q_OBJECT

public:
    explicit WorkoutCreator(QWidget *parent = 0);
    ~WorkoutCreator();

    void paintEvent(QPaintEvent *);



public slots:

    void resetWorkout();
    void editWorkout(Workout);
    void createWorkout(const QString& name, const QString& plan, const QString& creator, const QString& description, int type_workout);
    void checkToEnableButtonSave();

    void unselectSelectionNow();



signals :
    void workoutCreated(const Workout&);
    void workoutOverwrited(const Workout&);
    void showStatusBarMessage(const QString& msg, int timeDisplay);



private slots:
    void populateInputsFromWorkout();
    void on_pushButton_save_workout_clicked();



    void on_pushButton_add_clicked();
    void on_pushButton_copy_clicked();
    void on_pushButton_delete_clicked();
    void on_pushButton_repeat_clicked();

    void hightlightSelectedSourceRow(QString sourceRowIdentifier);
    void rightClickedGraph(QPointF);

    void customMenuRequested(QPoint pos);
    void tableViewSelectionChanged(QItemSelection,QItemSelection);

    void deletedRepeatWidget(int id);
    void updatedRepeatWidget(int id);

    void computeWorkout();





private :
    void restoreRepeatWidgetInterface();
    void ajustRepeatWidgetPosition();



private:
    Ui::WorkoutCreator *ui;

    Account *account;
    Workout workout;
    QList<RepeatWidget*> lstRepeatWidget;
    QList<RepeatData> lstRepeatData;
    int idRepeatWidget;


    IntervalTableModel *intervalModel;
    int totalWidthColumms;
    int widthLast2Column;
    QMenu *contextMenu;
    QAction *actionDelete;
    QAction *actionCopy;
    QAction *actionRepeat;




};

#endif // WORKOUTCREATOR_H
