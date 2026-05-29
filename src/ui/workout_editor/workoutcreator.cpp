
#include "workoutcreator.h"
#include "ui_workoutcreator.h"


#include <QMessageBox>
#include <QTableWidgetItem>
#include <QDebug>
#include <QPainter>
#include <algorithm>



#include "intervaldelegate.h"
#include "util.h"
#include "account.h"
#include "xmlutil.h"
#include "environnement.h"



WorkoutCreator::~WorkoutCreator()
{
    delete ui;
    qDeleteAll(lstRepeatWidget);
    lstRepeatWidget.clear();
}



WorkoutCreator::WorkoutCreator(QWidget *parent) : QWidget(parent), ui(new Ui::WorkoutCreator)
{
    ui->setupUi(this);

    this->account = qApp->property("Account").value<Account*>();


    /// Populate the type combobox in the same order as Workout::Type so
    /// QComboBox::currentIndex() maps directly onto the enum value.
    ui->comboBox_type_workout->addItem(tr("Endurance"), Workout::T_ENDURANCE);
    ui->comboBox_type_workout->addItem(tr("Interval"),  Workout::T_INTERVAL);
    ui->comboBox_type_workout->addItem(tr("Tempo"),     Workout::T_TEMPO);
    ui->comboBox_type_workout->addItem(tr("Test"),      Workout::T_TEST);
    ui->comboBox_type_workout->addItem(tr("Other"),     Workout::T_OTHERS);
    ui->comboBox_type_workout->addItem(tr("Threshold"), Workout::T_THRESHOLD);

    connect(ui->lineEdit_name_workout,    SIGNAL(textChanged(QString)), this, SLOT(checkToEnableButtonSave()));
    connect(ui->lineEdit_plan_workout,    SIGNAL(textChanged(QString)), this, SLOT(checkToEnableButtonSave()));
    connect(ui->lineEdit_creator_workout, SIGNAL(textChanged(QString)), this, SLOT(checkToEnableButtonSave()));
    connect(ui->pushButton_save_workout,  SIGNAL(clicked()),            this, SLOT(on_pushButton_save_workout_clicked()));



    workout = Workout();
    workout.setCreator(account->display_name);
    //    workout.setWorkout_name_enum(Workout::USER_MADE);



    QList<Interval> lstInterval;
    Interval interval1(QTime(0,10,0), tr("Warm up"),
                       Interval::PROGRESSIVE,  0.45, 0.60, 20, -1,
                       Interval::NONE,  90, 90, 5,
                       Interval::NONE, 0.5, 0.5, 15,
                       false, 0, 0, 0);
    lstInterval.append(interval1);




    idRepeatWidget = 0;

    intervalModel = new IntervalTableModel(this);
    intervalModel->setListInterval(lstInterval);
    connect(intervalModel, SIGNAL(dataChanged()), this, SLOT(computeWorkout()) );


    IntervalDelegate *delegateTableView = new IntervalDelegate(ui->tableView);
    ui->tableView->setItemDelegate(delegateTableView);
    ui->tableView->setModel(intervalModel);



    ui->tableView->setColumnWidth(0, 35);  //drag selector
    ui->tableView->setColumnWidth(1, 90);  //time
    ui->tableView->setColumnWidth(2, 160); //power
    ui->tableView->setColumnWidth(3, 160); //cadence
    ui->tableView->setColumnWidth(4, 160); //hr
    ui->tableView->setColumnWidth(5, 130); // display message
    ui->tableView->setColumnWidth(6, 130); // dummy column
    ui->tableView->setColumnWidth(7, 130); // dummy column


    //Connect Edit trigger (fix bug that sometimes select Cell instead of edit, TOFIX: edit: editing failed on columns not editable..)
    connect(ui->tableView, SIGNAL(clicked(QModelIndex)), ui->tableView, SLOT(edit(QModelIndex)));


    widthLast2Column = ui->tableView->columnWidth(ui->tableView->model()->columnCount()-1);
    //    widthLast2Column += ui->tableView->columnWidth(ui->tableView->model()->columnCount()-2);
    totalWidthColumms = 0;
    for (int i=0; i<ui->tableView->model()->columnCount(); i++) {
        totalWidthColumms += ui->tableView->columnWidth(i);
    }
    //    qDebug() << "totalWidgthColumn :" << totalWidthColumms;




    ui->tableView->verticalHeader()->setDefaultSectionSize(50); //55

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableView->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

    /// Disable resize vertical header
    ui->tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);


    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setDropIndicatorShown(true);

    ui->tableView->horizontalHeader()->setMouseTracking(true);
    ui->tableView->verticalHeader()->setMouseTracking(true);
    ui->tableView->horizontalHeader()->installEventFilter(ui->tableView);
    ui->tableView->verticalHeader()->installEventFilter(ui->tableView);


    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(tableViewSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customMenuRequested(QPoint)) );
    connect(ui->tableView->verticalHeader(), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customMenuRequested(QPoint)) );


    contextMenu = new QMenu(this);
    actionRepeat = contextMenu->addAction(QIcon(":/image/icon/repeat"), tr("Repeat"));
    actionCopy = contextMenu->addAction(QIcon(":/image/icon/copy"), tr("Copy"));
    actionDelete = contextMenu->addAction(QIcon(":/image/icon/delete"), tr("Delete"));

    connect(actionDelete, SIGNAL(triggered()), this, SLOT(on_pushButton_delete_clicked()) );
    connect(actionCopy, SIGNAL(triggered()), this, SLOT(on_pushButton_copy_clicked()) );
    connect(actionRepeat, SIGNAL(triggered()), this, SLOT(on_pushButton_repeat_clicked()) );


    ui->pushButton_repeat->setEnabled(false);
    ui->pushButton_copy->setEnabled(false);
    ui->pushButton_delete->setEnabled(false);
    actionRepeat->setEnabled(false);
    actionCopy->setEnabled(false);
    actionDelete->setEnabled(false);


    connect(ui->widget_plot, SIGNAL(rightClickedGraph(QPointF)), this, SLOT(rightClickedGraph(QPointF)));
    connect(ui->widget_plot, SIGNAL(shapeClicked(QString)), this, SLOT(hightlightSelectedSourceRow(QString)) );



    computeWorkout();






}



///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::populateInputsFromWorkout() {

    /// Sync the native input widgets with the current `workout` state.
    /// Called from resetWorkout()/editWorkout() — replaces the old
    /// fillWorkoutCreatorPageWeb() that pushed values into the embedded
    /// jQuery page via runJavaScript().
    ui->lineEdit_name_workout->setText(workout.getName());
    ui->lineEdit_plan_workout->setText(workout.getPlan());
    ui->lineEdit_creator_workout->setText(workout.getCreatedBy());
    ui->plainTextEdit_description_workout->setPlainText(workout.getDescription());

    int typeIndex = ui->comboBox_type_workout->findData(static_cast<int>(workout.getType()));
    if (typeIndex < 0) typeIndex = 0;
    ui->comboBox_type_workout->setCurrentIndex(typeIndex);
}




//------------------------------------------------------------------------------------------------
void WorkoutCreator::rightClickedGraph(QPointF pos) {

    Q_UNUSED(pos);

    //// get current mouse pos, show menu
    contextMenu->popup(QCursor::pos());

}

//------------------------------------------------------------------------------------------------
void WorkoutCreator::hightlightSelectedSourceRow(QString sourceRowIdentifier) {

    int rowToSelect = sourceRowIdentifier.toInt();
    ui->tableView->selectRow(rowToSelect);

}




///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::resetWorkout() {

    qDebug() << "WORKOUT CREATOR SETTING WORKOUT!";

    ///Sure you want to reset?
    workout.setFilePath("");

    ///Clear current data
    intervalModel->resetLstInterval();
    qDeleteAll(lstRepeatWidget);
    lstRepeatWidget.clear();
    lstRepeatData.clear();

    workout.setName("");
    workout.setPlan("-");
    workout.setCreator(account->display_name);
    workout.setDescription("");
    workout.setType(Workout::T_ENDURANCE);
    populateInputsFromWorkout();

    computeWorkout();



}


///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::computeWorkout() {


    //    qDebug() << "****ROW COUNT INTERVAL MODEL!***" << intervalModel->rowCount();
    qDebug() << "WorkoutCreator::computeWorkout start";

    /// Get list RepeatData
    QList<RepeatData> lstRepData;
    foreach (RepeatWidget *wid, lstRepeatWidget) {
        RepeatData *rep = wid->getRepeatData();
        lstRepData.append(*rep);
    }

    qDebug() << "WorkoutCreator::computeWorkout mid ";

    QString nameTmp = workout.getName();
    QString creatorTmp = workout.getCreatedBy();
    QString descTmp = workout.getDescription();
    QString plan = workout.getPlan();
    Workout::Type type = workout.getType();
    workout = Workout(workout.getFilePath(), Workout::USER_MADE, intervalModel->getLstInterval(), lstRepData, nameTmp, creatorTmp, descTmp, plan, type);


    checkToEnableButtonSave();

    ui->widget_plot->updateWorkout(workout);

    qDebug() << "WorkoutCreator::computeWorkout end ";
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::customMenuRequested(QPoint pos) {

    //    qDebug() << "WC:customMenuRequested";
    QModelIndex index = ui->tableView->indexAt(pos);
    if (!index.isValid() )
        return;

    contextMenu->popup(ui->tableView->viewport()->mapToGlobal(pos));


}





///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::editWorkout(Workout workoutToEdit) {


    workout = workoutToEdit;


    qDebug() << "WORKOUT PATH IS !" << workout.getFilePath();

    /// Clear last Repeat widget
    qDeleteAll(lstRepeatWidget);
    lstRepeatWidget.clear();
    lstRepeatData.clear();

    lstRepeatData = workoutToEdit.getLstRepeat();

    /// Restore idRepeatWidget to latest repeatId
    int idMaxFound = 0;
    foreach (RepeatData rep, lstRepeatData) {
        if (rep.getId() > idMaxFound)
            idMaxFound = rep.getId();
    }
    idRepeatWidget = idMaxFound + 1;


    intervalModel->setListInterval(workoutToEdit.getLstIntervalSource());
    restoreRepeatWidgetInterface();

    populateInputsFromWorkout();
    checkToEnableButtonSave();
}



////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::checkToEnableButtonSave() {

    qDebug() << "OK WORKOUT CREATOR CHECK BUTTON SAVE";

    const bool hasIntervals = intervalModel->rowCount() > 0;
    const bool hasName    = !ui->lineEdit_name_workout->text().isEmpty();
    const bool hasPlan    = !ui->lineEdit_plan_workout->text().isEmpty();
    const bool hasCreator = !ui->lineEdit_creator_workout->text().isEmpty();

    ui->pushButton_save_workout->setEnabled(hasIntervals && hasName && hasPlan && hasCreator);
}


////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::on_pushButton_save_workout_clicked() {

    createWorkout(ui->lineEdit_name_workout->text(),
                  ui->lineEdit_plan_workout->text(),
                  ui->lineEdit_creator_workout->text(),
                  ui->plainTextEdit_description_workout->toPlainText(),
                  ui->comboBox_type_workout->currentIndex());
}



////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::restoreRepeatWidgetInterface()
{

    for (int i=0; i<lstRepeatData.size(); i++)
    {
        RepeatData rep = lstRepeatData.at(i);

        qDebug() << "repeat  numberRepeat!" << rep.getNumberRepeat() <<
                    "firstRow" << rep.getFirstRow() << "last row" << rep.getLastRow();

        QModelIndex firstIndex = intervalModel->index(rep.getFirstRow(), 0, QModelIndex());
        QModelIndex lastIndex = intervalModel->index(rep.getLastRow(), 0, QModelIndex());

        qDebug() << "ok first index" << firstIndex.row() << "last" << lastIndex.row();

        /// REDRAW
        QRect recFirstSelection(ui->tableView->visualRect(firstIndex));
        QRect recLastSelection(ui->tableView->visualRect(lastIndex));
        QRect rectCompleteSelection(recFirstSelection.topLeft(), recLastSelection.bottomRight() );
        rectCompleteSelection.setWidth(totalWidthColumms);

        int id = rep.getId();
        int firstRow = rep.getFirstRow();
        int lastRow = rep.getLastRow();
        int numberRepeat = rep.getNumberRepeat();

        RepeatData *repData = new RepeatData(id, firstRow, lastRow, numberRepeat);
        RepeatWidget *repeatWidgetCopy = new RepeatWidget(repData, ui->tableView->viewport());
        repeatWidgetCopy->move(rectCompleteSelection.topLeft());
        repeatWidgetCopy->resize(rectCompleteSelection.size().width(), rectCompleteSelection.size().height());
        repeatWidgetCopy->setRightWidth(widthLast2Column);
        repeatWidgetCopy->show();


        connect(repeatWidgetCopy, SIGNAL(deleteSignal(int)), this, SLOT(deletedRepeatWidget(int)) );
        connect(repeatWidgetCopy, SIGNAL(updateSignal(int)), this, SLOT(updatedRepeatWidget(int)) );
        connect(repeatWidgetCopy, SIGNAL(clickedRightPartWidget()), this, SLOT(unselectSelectionNow()) );

        lstRepeatWidget.append(repeatWidgetCopy);
    }

    computeWorkout();
}

////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::unselectSelectionNow() {

    qDebug() << "unselectSelectionNow";
    ui->tableView->selectionModel()->clear();
    ui->widget_plot->removeHightlight();

}

////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::on_pushButton_repeat_clicked()
{

    qDebug() << "on_pushButton_repeat_clicked";
    QModelIndexList lstIndex = ui->tableView->selectionModel()->selectedRows();

    if (lstIndex.size() < 1) {
        return;
    }

    QModelIndex firstIndex = lstIndex.at(0);
    QModelIndex lastIndex = lstIndex.at(lstIndex.size()-1);



    /// Appen when you press control and select 2 sepearate rows
    if (firstIndex.row() > lastIndex.row()) {
        QModelIndex tmpIndex = firstIndex;
        firstIndex = lastIndex;
        lastIndex = tmpIndex;
    }

    qDebug() << "Repeat:***************firstINdex" <<firstIndex.row();
    qDebug() << "lastIndex" <<lastIndex.row();
    qDebug() << "***************";


    QRect recFirstSelection(ui->tableView->visualRect(firstIndex));
    QRect recLastSelection(ui->tableView->visualRect(lastIndex));
    QRect rectCompleteSelection(recFirstSelection.topLeft(), recLastSelection.bottomRight() );
    rectCompleteSelection.setWidth(totalWidthColumms);


    RepeatData *repData = new RepeatData(idRepeatWidget, firstIndex.row(), lastIndex.row(), 2);
    RepeatWidget *repeatWidget = new RepeatWidget(repData, ui->tableView->viewport());
    repeatWidget->move(rectCompleteSelection.topLeft());
    repeatWidget->resize(rectCompleteSelection.size().width(), rectCompleteSelection.size().height());
    repeatWidget->setRightWidth(widthLast2Column);
    repeatWidget->show();



    connect(repeatWidget, SIGNAL(deleteSignal(int)), this, SLOT(deletedRepeatWidget(int)) );
    connect(repeatWidget, SIGNAL(updateSignal(int)), this, SLOT(updatedRepeatWidget(int)) );
    connect(repeatWidget, SIGNAL(clickedRightPartWidget()), this, SLOT(unselectSelectionNow()) );


    /// Add widget to QList, order first Repeat = first in QList
    idRepeatWidget ++;
    lstRepeatWidget.append(repeatWidget);
    std::sort(lstRepeatWidget.begin(), lstRepeatWidget.end(), RepeatWidget::myLessThan );


    tableViewSelectionChanged(QItemSelection(), QItemSelection());
    computeWorkout();
}





///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::tableViewSelectionChanged(QItemSelection, QItemSelection) {

    qDebug() << "TABLE VIEW SELECTION CHANGED, HIGHLIGHT GRAPH INTERVALS";

    /// default behavior = disabled
    ui->pushButton_repeat->setEnabled(false);
    ui->pushButton_copy->setEnabled(false);
    ui->pushButton_delete->setEnabled(false);
    actionRepeat->setEnabled(false);
    actionCopy->setEnabled(false);
    actionDelete->setEnabled(false);



    QModelIndexList lstIndex = ui->tableView->selectionModel()->selectedRows();
    qDebug() << "selecte size is--" << lstIndex.size();

    if (lstIndex.size() < 1) {
        ui->widget_plot->removeHightlight();
        return;
    }

    QSet<QString> setIntervalToHightlight;
    foreach (QModelIndex index,  lstIndex) {
        Interval interval = intervalModel->getIntervalAtRow(index);
        setIntervalToHightlight.insert(QString::number(interval.getSourceRowLstInterval()) );
    }
    ui->widget_plot->updateBackgroundHighlight(setIntervalToHightlight);
    //    ///////////////


    qDebug() << "updateBackgroundHighlight ok";

    ui->pushButton_copy->setEnabled(true);
    ui->pushButton_delete->setEnabled(true);
    actionCopy->setEnabled(true);
    actionDelete->setEnabled(true);


    QModelIndex firstIndex = lstIndex.at(0);
    QModelIndex lastIndex = lstIndex.at(lstIndex.size()-1);


    /// --- Check All overlappign possibilities (see if we can enable repeat button)
    foreach (RepeatWidget *wid, lstRepeatWidget) {

        if (firstIndex.row() >= wid->getRepeatData()->getFirstRow() && firstIndex.row() <= wid->getRepeatData()->getLastRow() )  {
            //            qDebug() << "1 REFUSE";
            return;
        }
        else if (lastIndex.row() <= wid->getRepeatData()->getLastRow() && lastIndex.row() >= wid->getRepeatData()->getFirstRow())  {
            //            qDebug() << "2 REFUSE";
            return;
        }
        else if (firstIndex.row() < wid->getRepeatData()->getFirstRow() && lastIndex.row() > wid->getRepeatData()->getLastRow())  {
            //            qDebug() << "3 REFUSE";
            return;
        }
    }



    ui->pushButton_repeat->setEnabled(true);
    actionRepeat->setEnabled(true);

    qDebug() << "tableViewSelectionChanged finished";

}


/////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::on_pushButton_add_clicked()
{
    intervalModel->insertRow(intervalModel->rowCount());
    ajustRepeatWidgetPosition();
    ui->tableView->scrollToBottom();
}



/////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::on_pushButton_copy_clicked()
{

    QModelIndexList lstIndex = ui->tableView->selectionModel()->selectedRows();

    if (lstIndex.size() < 1) {
        return;
    }

    foreach (QModelIndex index,  lstIndex) {
        intervalModel->copyRows(index.row());
    }
    ajustRepeatWidgetPosition();
    ui->tableView->scrollToBottom();
}



//--------------------------------------------------------------------------------------------------------
void WorkoutCreator::on_pushButton_delete_clicked()
{

    ////////////////////--------------------------------------------------------------
    QModelIndexList lstIndex = ui->tableView->selectionModel()->selectedRows();

    qDebug() << "lstINdex delete Size is" << lstIndex.size();

    ///Deselect if we are editing a line, because cause crash edition+delete
    unselectSelectionNow();

    if (lstIndex.size() < 1) {
        return;
    }

    /// Sort list in ascending order to remove from end to start (indexes stay valid)
    std::sort(lstIndex.begin(), lstIndex.end(), std::less<QModelIndex>());

    ////////////////////---------------------------------------------------------------



    foreach (RepeatWidget *wid, lstRepeatWidget)
    {
        int numberRowDeletedInRepeat = 0;
        int numberRowInRepeat = wid->getRepeatData()->getLastRow() - wid->getRepeatData()->getFirstRow() + 1;
        int numberRowBeforeRepeat = 0;


        foreach (QModelIndex index,  lstIndex) {

            /// included in repeat, delete it
            if ( (index.row() >=  wid->getRepeatData()->getFirstRow()) && (index.row() <= wid->getRepeatData()->getLastRow()) )
                numberRowDeletedInRepeat++;
            /// before repeat
            if ( index.row() <  wid->getRepeatData()->getFirstRow() )
                numberRowBeforeRepeat++;

        }

        /// if deleted all interval, delete repeat
        if (numberRowDeletedInRepeat == numberRowInRepeat) {
            qDebug() << "DELETE IT HERE";
            lstRepeatWidget.removeOne(wid);
            delete wid;
        }
        /// Some interval deleted, update frame repeat
        else if (numberRowDeletedInRepeat != 0 || numberRowBeforeRepeat != 0 ) {
            qDebug() << "update frame here!";
            int newFirstRow = wid->getRepeatData()->getFirstRow() - numberRowBeforeRepeat;
            int newLastRow = wid->getRepeatData()->getLastRow() - numberRowDeletedInRepeat - numberRowBeforeRepeat;
            wid->getRepeatData()->setFirstRow(newFirstRow);
            wid->getRepeatData()->setLastRow(newLastRow);
        }
    }



    ///Start to remove from the end
    for (int i=lstIndex.count() - 1; i >= 0; --i) {
        qDebug() << "Deleting row no:" << lstIndex.at(i).row();
        intervalModel->removeRow(lstIndex.at(i).row());
    }


    ajustRepeatWidgetPosition();

}


/////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::ajustRepeatWidgetPosition() {

    qDebug() << "ajustRepeatWidgetPosition!";

    foreach (RepeatWidget *wid, lstRepeatWidget)
    {

        qDebug() << "ajustRepeat firstRow" << wid->getRepeatData()->getFirstRow() << "last row" << wid->getRepeatData()->getLastRow();

        QModelIndex firstIndex = ui->tableView->model()->index(wid->getRepeatData()->getFirstRow(), 0);
        QModelIndex lastIndex = ui->tableView->model()->index(wid->getRepeatData()->getLastRow(), 0);

        QRect recFirstSelection(ui->tableView->visualRect(firstIndex));
        QRect recLastSelection(ui->tableView->visualRect(lastIndex));
        QRect rectCompleteSelection(recFirstSelection.topLeft(), recLastSelection.bottomRight() );
        rectCompleteSelection.setWidth(totalWidthColumms);

        wid->move(rectCompleteSelection.topLeft());
        wid->resize(rectCompleteSelection.size().width(), rectCompleteSelection.size().height());
        wid->setRightWidth(widthLast2Column);
        wid->show();
    }



}


/////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::deletedRepeatWidget(int id) {



    qDebug() << "deletedRepeatWidget" << id;

    QItemSelection selection = ui->tableView->selectionModel()->selection();

    RepeatWidget *widgetToDelete;
    /// Better way to delete without iterating over QList? Array worth it? small data set..
    foreach (RepeatWidget *wid, lstRepeatWidget) {
        if (wid->getRepeatData()->getId() == id) {

            //Delete Repeat Increase Data associated with theses interval..
            intervalModel->removeIntervalRepeatData(wid->getRepeatData()->getFirstRow(), wid->getRepeatData()->getLastRow());
            wid->setVisible(false);
            widgetToDelete = wid;
            break;
        }
    }
    lstRepeatWidget.removeOne(widgetToDelete);
    delete widgetToDelete;


    ui->tableView->selectionModel()->select(selection, QItemSelectionModel::Select);
    ui->tableView->setFocus();
    tableViewSelectionChanged(QItemSelection(), QItemSelection());
    computeWorkout();
    ajustRepeatWidgetPosition();

}

/////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::updatedRepeatWidget(int id) {

    qDebug() << "updatedRepeatWidget" << id;
    computeWorkout();
    ajustRepeatWidgetPosition();
}





/////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::createWorkout(const QString& name, const QString& plan, const QString& creator, const QString& description, int type_workout) {


    qDebug() << "name11:" << name << "creator:" << creator << "desc:" << description << "typeInt:" << type_workout;
    qDebug() << "WORKOUT PATH:" << workout.getFilePath();



    //// ---- Set workout attributes -----
    workout.setName(name);
    workout.setPlan(plan);
    workout.setCreator(creator);
    workout.setDescription(description);
    Workout::Type typeWorkout = static_cast<Workout::Type>(type_workout);
    workout.setType(typeWorkout);


    QString pathWorkout;
    if (workout.getFilePath() == "" || (workout.getFilePath().length() > 1 && workout.getFilePath().at(0) == ':') ) {  //also check if it's an included workout (resource)
        pathWorkout = Util::getSystemPathWorkout() + "/" + workout.getName() + ".workout";
        workout.setFilePath(pathWorkout);
    }
    else {
        QFileInfo fileInfo(workout.getFilePath());
        pathWorkout = fileInfo.absolutePath() + "/" + workout.getName() + ".workout";
        workout.setFilePath(pathWorkout);
    }

    if (Util::getSystemPathWorkout() == "invalid_writable_path") {
        return;
    }


    /// ---- Save workout to disk -----
    bool fileCreated = false;


    qDebug() << "SHOULD SAVE AS ***" << pathWorkout;
    bool exist = Util::checkFileNameAlreadyExist(workout.getFilePath());


    if (exist)
    {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(tr("A workout already exists with that name."));
        msgBox.setInformativeText(tr("Overwrite it?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        if (msgBox.exec() == QMessageBox::Yes) {
            qDebug() << "Yes was clicked";
            /// Delete local file xml
            Util::deleteLocalFile(workout.getFilePath());


            fileCreated = XmlUtil::createWorkoutXml(workout, "");
            qDebug() << "Emit workoutOverwrited";
            emit workoutOverwrited(workout);
        }
    }
    else {
        fileCreated = XmlUtil::createWorkoutXml(workout, "");
        qDebug() << "Emit workoutCreated";
        emit workoutCreated(workout);
    }

    if (fileCreated) {

        QString textToShow;
        if (!exist)
            textToShow = tr("Workout  \"%1\" created").arg(workout.getFilePath());
        else
            textToShow = tr("Workout  \"%1\" overwrited").arg(workout.getFilePath());
        emit showStatusBarMessage(textToShow, 7000);
    }
}








/////////////////////////////////////////////////////////////////////////////////////////
void WorkoutCreator::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

