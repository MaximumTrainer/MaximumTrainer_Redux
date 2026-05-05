#include "main_workoutpage.h"
#include "ui_main_workoutpage.h"

#include <QPainter>
#include <QMessageBox>
#include <QScrollBar>

#include "workoutdialog.h"
#include "util.h"
#include "environnement.h"
#include "workoututil.h"

#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEnginePage>
#include <QWebEngineScriptCollection>
#include <QWebChannel>





/////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DESTRUCTOR
/////////////////////////////////////////////////////////////////////////////////////////////////////////
Main_WorkoutPage::~Main_WorkoutPage() {
    delete ui;


}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CONSTRUCTOR
/////////////////////////////////////////////////////////////////////////////////////////////////////////
Main_WorkoutPage::Main_WorkoutPage(QWidget *parent) : QWidget(parent), ui(new Ui::Main_WorkoutPage) {

    ui->setupUi(this);

    this->account = qApp->property("Account").value<Account*>();
    this->settings = qApp->property("User_Settings").value<Settings*>();

    loadFilterFields();



    tableModel = new WorkoutTableModel(this);
    xmlUtil = new XmlUtil(settings->language, this);


    delegateRow = new delegateRowHover(false, this);
    ui->tableView_workout->setItemDelegate(delegateRow);


    proxyModel = new SortFilterProxyModel(this);
    proxyModel->setSourceModel(tableModel);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->addFilterColumn(0);
    proxyModel->addFilterColumn(1);
    proxyModel->addFilterColumn(2);
    proxyModel->addFilterColumn(3);

    ui->tableView_workout->setModel(proxyModel);
    ui->tableView_workout->verticalHeader()->setDefaultSectionSize(60);

    connectWebChannelWorkout();
    ui->webView_workouts->setUrl(QUrl(Environnement::getUrlWorkout()));


    ui->tableView_workout->sortByColumn(0, Qt::AscendingOrder);
    ui->tableView_workout->setColumnWidth(0, 200);
    ui->tableView_workout->setColumnWidth(1, 115);
    ui->tableView_workout->setColumnWidth(2, 115);
    ui->tableView_workout->setColumnWidth(3, 80);
    ui->tableView_workout->setColumnWidth(4, 65);
    ui->tableView_workout->setColumnWidth(5, 55);
    ui->tableView_workout->setColumnWidth(6, 40);
    ui->tableView_workout->setColumnWidth(7, 40);
    ui->tableView_workout->setColumnWidth(8, 40);
    ui->tableView_workout->setColumnWidth(9, 40);
    ui->tableView_workout->horizontalHeader()->setStretchLastSection(true);
    ui->tableView_workout->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableView_workout->setSelectionBehavior(QAbstractItemView::SelectRows);


    ui->tableView_workout->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableView_workout->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);



    connect(ui->tableView_workout->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(tableViewSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->tableView_workout, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customMenuRequested(QPoint)) );
    connect(ui->tableView_workout->verticalHeader(), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customMenuRequested(QPoint)) );


    contextMenu = new QMenu(this);
    actionEdit = contextMenu->addAction(QIcon(":/image/icon/edit"), tr("Edit"));
    actionDelete = contextMenu->addAction(QIcon(":/image/icon/delete"), tr("Delete"));
    actionSetAsDone = contextMenu->addAction(QIcon(":/image/icon/check"), tr("Set As Done"));
    contextMenu->addSeparator();
    actionOpenFolder = contextMenu->addAction(QIcon(":/image/icon/folder"), tr("Open in folder"));
    actionExportPDF = contextMenu->addAction(QIcon(":/image/icon/pdf"), tr("Export to PDF"));
    contextMenu->addSeparator();
    actionAddToQueue = contextMenu->addAction(QIcon(":/image/icon/add"), tr("Add to Queue"));

    connect(actionEdit, SIGNAL(triggered()), this, SLOT(editWorkout()) );
    connect(actionDelete, SIGNAL(triggered()), this, SLOT(deleteWorkout()) );
    connect(actionSetAsDone, SIGNAL(triggered()), this, SLOT(setAsDone()) );
    connect(actionOpenFolder, SIGNAL(triggered()), this, SLOT(openFolderWorkout()) );
    connect(actionExportPDF, SIGNAL(triggered()), this, SLOT(exportToPdf()) );
    connect(actionAddToQueue, SIGNAL(triggered()), this, SLOT(addToQueue()));
    actionEdit->setEnabled(false);
    actionDelete->setEnabled(false);
    actionOpenFolder->setEnabled(false);


    connect(ui->webView_workouts, SIGNAL(loadFinished(bool)), this, SLOT(fillWorkoutPage()));

}



///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::parseIncludedWorkouts() {

    xmlUtil = new XmlUtil(settings->language, this);
    QList<Workout> lstWorkout;

    QList<Workout> lstWorkoutBase = WorkoutUtil::getListWorkoutBase();
    QList<Workout> sufferfest = xmlUtil->getLstWorkoutSufferfest();
    QList<Workout> planBT = xmlUtil->getLstWorkoutBt16WeeksPlan();
    QList<Workout> rachel = xmlUtil->getLstWorkoutRachel();

    lstWorkout.append(lstWorkoutBase);
    lstWorkout.append(sufferfest);
    lstWorkout.append(planBT);
    lstWorkout.append(rachel);
    tableModel->addListWorkout(lstWorkout);

}
//-----------------------------------------------------
void Main_WorkoutPage::parseUserWorkouts() {

    xmlUtil = new XmlUtil(settings->language, this);
    QList<Workout> lstWorkoutUser = xmlUtil->getLstUserWorkout();
    tableModel->addListWorkout(lstWorkoutUser);
}
//-----------------------------------------------------
void Main_WorkoutPage::parseMapWorkout(int userFTP) {

    Workout workout = WorkoutUtil::getWorkoutMap(userFTP);
    tableModel->addWorkout(workout);

}




//---------------------------------------------------------------------------------------------------------
void Main_WorkoutPage::connectWebChannelWorkout() {


    qDebug() << "connectWebChannelWorkout";

    QFile webChannelJsFile(":/qtwebchannel/qwebchannel.js");
    if(  !webChannelJsFile.open(QIODevice::ReadOnly) ) {
        qDebug() << QString("Couldn't open qwebchannel.js file: %1").arg(webChannelJsFile.errorString());
    }
    else {
        qDebug() << "OK webEngineProfile";
        QByteArray webChannelJs = webChannelJsFile.readAll();
        webChannelJs.append(
                    "\n"
                    "var workoutObject"
                    "\n"
                    "new QWebChannel(qt.webChannelTransport, function(channel) {"
                    "     workoutObject = channel.objects.workoutObject;"
                    "});"
                    "\n"
                    );

        QWebChannel *channel = new QWebChannel(ui->webView_workouts);
        QWebEngineScript script;
        script.setSourceCode(webChannelJs);
        script.setName("qwebchannel.js");
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(false);

        ui->webView_workouts->page()->scripts().insert(script);
        ui->webView_workouts->page()->setWebChannel(channel);
        channel->registerObject("workoutObject", this);
    }


    if (!account->show_included_workout) {
        ui->checkBox->setChecked(false);
        filterChangedWorkoutType(false);
    }

}


//---------------------------------------------------------------------------------------------------------
void Main_WorkoutPage::fillWorkoutPage() {


    qDebug() << "fillWorkoutPage";

    QString jsCode;
    jsCode = QString("$('#name-workout').val('%1');").arg(nameFilter);
    jsCode += QString("$('#plan-workout').val('%1');").arg(planFilter);
    jsCode += QString("$('#creator-workout').val('%1');").arg(creatorFilter);

    if (typeFilter != -1) {
        jsCode += QString("$('#select-type-workout').val(%1);").arg(typeFilter);
        jsCode += "$('#select-type-workout').selectpicker('refresh');";
        jsCode += "$('#select-type-workout').trigger('change');";
    }


    qDebug() << "JSTOEXECUTE IS:" << jsCode;
    // Guard against pages where jQuery is not yet loaded (e.g. screenshot/CI
    // mode where the external URL was replaced with blank HTML).
    ui->webView_workouts->page()->runJavaScript(
        QStringLiteral("if(typeof window.$==='function'){") + jsCode + QStringLiteral("}"));

    filterChanged("name", nameFilter);
    filterChanged("plan", planFilter);
    filterChanged("creator", creatorFilter);

}


//---------------------------------------------------------------------------------------------------------
void Main_WorkoutPage::tableViewSelectionChanged(QItemSelection, QItemSelection) {


    /// default behavior = disabled
    actionEdit->setEnabled(false);
    actionDelete->setEnabled(false);
    actionSetAsDone->setEnabled(false);
    actionOpenFolder->setEnabled(false);



    QModelIndexList lstIndex = ui->tableView_workout->selectionModel()->selectedRows();

    if (lstIndex.size() < 1) {
        return;
    }

    QModelIndex index = lstIndex.at(0);
    indexSourceSelected = proxyModel->mapToSource(index);
    Workout workout = tableModel->getWorkoutAtRow(indexSourceSelected);

    qDebug() << "name:" << workout.getName() << "workout creator:" << workout.getCreatedBy() << " NP" << workout.getNormalizedPower();

    if (account->hashWorkoutDone.contains(workout.getName())) {
        actionSetAsDone->setText(tr("Unset Done"));
    }
    else {
        actionSetAsDone->setText(tr("Set As Done"));
    }
    actionSetAsDone->setEnabled(true);

    if (workout.getWorkoutNameEnum() != Workout::USER_MADE) {
        actionDelete->setEnabled(false);
        actionOpenFolder->setEnabled(false);
        actionEdit->setEnabled(true); //let user edit, no copyright on a workout..
        //        if (workout.getWorkoutNameEnum() == Workout::MAP_TEST)
        //            actionEdit->setEnabled(false);
    }
    else {
        actionEdit->setEnabled(true);
        actionDelete->setEnabled(true);
        actionOpenFolder->setEnabled(true);
    }


}




///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::customMenuRequested(const QPoint& pos) {

    qDebug() << "customMenuRequested2";

    QModelIndex index = ui->tableView_workout->indexAt(pos);
    if (!index.isValid() )
        return;

    contextMenu->popup(ui->tableView_workout->viewport()->mapToGlobal(pos));

    qDebug() << "customMenuRequested End";
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::addWorkout(const Workout& workout) {

    qDebug() << "Main_WorkoutPage::addWorkout";


    setFilterPlanName(""); //reset filter


    ui->tableView_workout->setFocus();
    ui->tableView_workout->clearSelection();
    tableModel->addWorkout(workout);


    /// Select workout in listView
    for (int i=tableModel->columnCount(); i>=0; i--)
    {
        QModelIndex index = tableModel->index(tableModel->rowCount()-1, i);
        QModelIndex proxyIndex = proxyModel->mapFromSource(index);
        ui->tableView_workout->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::Select);
    }

    qDebug() << "End Main_WorkoutPage::addWorkout";

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::overwriteWorkout(const Workout& workout) {

    qDebug() << "Main_WorkoutPage::overwriteWorkout";

    setFilterPlanName(""); //reset filter

    ui->tableView_workout->clearSelection();

    qDebug() << "DELETE WORKOUT WITH THIS NAME: " << workout.getName();
    tableModel->deleteWorkoutWithName(workout.getName());
    tableModel->addWorkout(workout);

    /// Select workout in listView
    for (int i=tableModel->columnCount(); i>=0; i--)
    {
        QModelIndex index = tableModel->index(tableModel->rowCount()-1, i);
        QModelIndex proxyIndex = proxyModel->mapFromSource(index);
        ui->tableView_workout->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::Select);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::openFolderWorkout() {

    Workout workout = tableModel->getWorkoutAtRow(indexSourceSelected);

    qDebug() << "openFolder" << workout.getFilePath();
    Util::openWorkoutFolder(workout.getFilePath());
}



//--------------------------------------------------------------------------------------
void Main_WorkoutPage::exportToPdf() {

    Workout workout = tableModel->getWorkoutAtRow(indexSourceSelected);
    emit signal_exportWorkoutToPdf(workout);

}

//--------------------------------------------------------------------------------------
void Main_WorkoutPage::addToQueue() {

    Workout workout = tableModel->getWorkoutAtRow(indexSourceSelected);
    emit addWorkoutToQueue(workout);

}


///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::editWorkout() {

    qDebug() << "editWorkout";
    Workout workout = tableModel->getWorkoutAtRow(indexSourceSelected);



    qDebug() << "workout repeat size here Before signal" << workout.getLstRepeat().size();
    qDebug() << "workout name is" << workout.getName();


    emit editWorkout(workout);
}


//--------------------------------------------------------------------------------------------
void Main_WorkoutPage::deleteWorkout() {


    Workout workout = tableModel->getWorkoutAtRow(indexSourceSelected);

    /// Ask confirmation
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Question);
    QString textToShow = tr("Are you sure you want to delete <b>%1</b>?").arg(workout.getName());
    msgBox.setText(textToShow);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if (msgBox.exec() == QMessageBox::Yes) {
        qDebug() << "Yes was clicked";
        /// Delete from model
        tableModel->removeRows(indexSourceSelected.row(), 1, QModelIndex());

        /// Delete local file xml
        Util::deleteLocalFile(workout.getFilePath());

        //  remove from lst done
        account->hashWorkoutDone.remove(workout.getName());
    }
    tableViewSelectionChanged(QItemSelection(), QItemSelection());



}

//--------------------------------------------------------------------------------------------
void Main_WorkoutPage::setAsDone() {


    Workout workout = tableModel->getWorkoutAtRow(indexSourceSelected);

    qDebug() << "Set AS DONE!" << workout.getName();

    if (account->hashWorkoutDone.contains(workout.getName())) {
        account->hashWorkoutDone.remove(workout.getName());
    }
    else {
        account->hashWorkoutDone.insert(workout.getName());
    }
    ui->tableView_workout->selectionModel()->clearSelection();
}


//--------------------------------------------------------------------------------------------
void Main_WorkoutPage::on_tableView_workout_doubleClicked(const QModelIndex &index) {


    QModelIndex sourceIndex = proxyModel->mapToSource(index);
    Workout workoutToDo = tableModel->getWorkoutAtRow(sourceIndex);

    if(account->enable_studio_mode && workoutToDo.getWorkoutNameEnum() == Workout::MAP_TEST) {
        QMessageBox msgBox;
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText(tr("MAP Test can only be done alone. Please disable studio mode to continue"));
        msgBox.setStandardButtons(QMessageBox::Close);
        msgBox.exec();
    }
    else {
        emit executeWorkout(workoutToDo);
    }


}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::updateTableViewMetrics() {
    tableModel->updateWorkoutsMetrics();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::setFilterPlanName(const QString& plan) {


    qDebug() << "setFilterPlanName";

    // Escape backslashes and single quotes so the value is safe inside a JS single-quoted string
    QString jsPlan = plan;
    jsPlan.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    jsPlan.replace(QLatin1Char('\''), QStringLiteral("\\'"));

    QString jsToExecute = "$('#name-workout').val( '' ); ";
    jsToExecute += QString("$('#plan-workout').val( '%1' ); ").arg(jsPlan);
    jsToExecute += "$('#creator-workout').val( '' ); ";

    jsToExecute += "$('#select-type-workout option:selected').prop('selected', false); ";
    jsToExecute += "$('#select-type-workout').selectpicker('refresh'); ";


    ui->webView_workouts->page()->runJavaScript(jsToExecute);


    filterChanged("", "");
    filterChanged("plan", plan);
    if (!ui->checkBox->isChecked() && plan != "") {
        ui->checkBox->setChecked(true);
        filterChangedWorkoutType(true);
    }

    ui->tableView_workout->scrollToTop();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::setFilterWorkoutName(const QString& workoutName) {


    qDebug() << "setFilterWorkoutName";

    // Escape backslashes and single quotes so the value is safe inside a JS single-quoted string
    QString jsWorkoutName = workoutName;
    jsWorkoutName.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    jsWorkoutName.replace(QLatin1Char('\''), QStringLiteral("\\'"));

    QString jsToExecute = "$('#plan-workout').val( '' ); ";
    jsToExecute += QString("$('#name-workout ').val( '%1' ); ").arg(jsWorkoutName);
    jsToExecute += "$('#creator-workout').val( '' ); ";

    jsToExecute += "$('#select-type-workout option:selected').prop('selected', false); ";
    jsToExecute += "$('#select-type-workout').selectpicker('refresh'); ";

    ui->webView_workouts->page()->runJavaScript(jsToExecute);


    filterChanged("", "");
    filterChanged("name", workoutName);

    ui->tableView_workout->scrollToTop();
}


//-------------------------------------------------------------
void Main_WorkoutPage::saveFilterFields() {

    QSettings settings;

    // Save Filter fields
    settings.beginGroup("Filter");
    settings.setValue("nameFilter", nameFilter);
    settings.setValue("planFilter", planFilter);
    settings.setValue("creatorFilter", creatorFilter);
    settings.setValue("typeFilter", typeFilter);
    settings.endGroup();
}
//-------------------------------------------------------------
void Main_WorkoutPage::loadFilterFields() {

    QSettings settings;

    // Save Filter fields
    settings.beginGroup("Filter");

    nameFilter = settings.value("nameFilter", "").toString();
    planFilter = settings.value("planFilter", "").toString();
    creatorFilter = settings.value("creatorFilter", "").toString();
    typeFilter = settings.value("typeFilter", -1).toInt();
    settings.endGroup();


    qDebug() << "Setings to load filter should be" << nameFilter << planFilter << creatorFilter << typeFilter;
}


//---------------------------------------------------------------
void Main_WorkoutPage::filterChangedList(int id) {

    //Just to save to settings later on
    qDebug() << "ok it changed here" << id;
    typeFilter = id;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::filterChanged(const QString& field, const QString& value) {

    proxyModel->setSortCaseSensitivity(Qt::CaseSensitive);

    qDebug() << "filterChanged inside Workout Page!";
    qDebug() << field << "value" << value;

    if (field == "name") {
        proxyModel->addFilterFixedString(0, value);
        nameFilter = value;
    }
    else if (field == "plan") {
        proxyModel->addFilterFixedString(1, value);
        planFilter = value;
    }
    else if (field == "creator") {
        proxyModel->addFilterFixedString(2, value);
        creatorFilter = value;
    }
    else if (field == "workoutType") {
        proxyModel->addFilterFixedString(3, value);
    }
    /// Reset filter
    else {
        for (int i=0; i<4; i++) {
            proxyModel->addFilterFixedString(i, "");
            nameFilter = planFilter = creatorFilter = "";
            typeFilter = -1;
        }
    }

    proxyModel->invalidate();







}




//----------------------------------------------------------------------------
void Main_WorkoutPage::filterChangedWorkoutType(bool includedWorkout) {

    qDebug() << "included workout should be used" << includedWorkout;


    //    USER_MADE, 0
    //    FTP_TEST,1
    //    INCLUDED_WORKOUT,2

    if (includedWorkout)
        proxyModel->addFilterWorkoutType(Workout::INCLUDED_WORKOUT);
    else
        proxyModel->addFilterWorkoutType(Workout::USER_MADE);

    proxyModel->invalidate();


    account->show_included_workout = includedWorkout;
}


//------------------------------------------------------------------------------
void Main_WorkoutPage::on_checkBox_clicked(bool checked)
{
    qDebug() << "OK NOW INCLUDED WORKOUT SHOULD BE INCLUDED?" << checked;

    filterChangedWorkoutType(checked);
}










//-------------------------------------------------------------------------
void Main_WorkoutPage::on_pushButton_refresh_clicked()
{
    refreshUserWorkout();
}
//-------------------------------------------------------------------------
void Main_WorkoutPage::refreshUserWorkout() {

    //Delete user workout and reparse them
    tableModel->deleteAllUserMadeWorkouts();
    parseUserWorkouts();
}

//--------------------------------------------------------------
void Main_WorkoutPage::refreshMapWorkout() {

    tableModel->deleleteMapWorkout();
    parseMapWorkout(account->FTP);
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_WorkoutPage::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}



