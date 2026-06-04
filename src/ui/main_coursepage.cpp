#include "main_coursepage.h"
#include "ui_main_coursepage.h"

#include <QMessageBox>
#include "environnement.h"

#include "util.h"


Main_CoursePage::~Main_CoursePage()
{
    delete ui;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
Main_CoursePage::Main_CoursePage(QWidget *parent) : QWidget(parent), ui(new Ui::Main_CoursePage) {

    ui->setupUi(this);

    //TEmporary label
    QLabel *labelComingSoon = new QLabel(tr("Courses - Coming soon"), this);
    labelComingSoon->setMinimumSize(900, 100);
    labelComingSoon->setContentsMargins(100,0,0,0);
    QFont boldFont;
    boldFont.setPointSizeF(30);
    boldFont.setBold(true);
    labelComingSoon->setFont(boldFont);



    ui->webView_courses->page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    ///// TEST QWEBVIEW -----------------------------------------
    QNetworkAccessManager *nam = qApp->property("NetworkManager").value<QNetworkAccessManager*>();
    ui->webView_courses->page()->setNetworkAccessManager(nam);

    // Signal is emitted before frame loads any web content:
    connect(ui->webView_courses->page()->mainFrame(), SIGNAL(javaScriptWindowObjectCleared()),
            this, SLOT(addJSObject_CoursePage()));
    ui->webView_courses->setUrl(QUrl(Environnement::getUrlWorkout()));



    this->account = qApp->property("Account").value<Account*>();
    this->settings = qApp->property("User_Settings").value<Settings*>();


    tableModel = new CourseTableModel(this);
    xmlUtil = new XmlUtil(this);

    delegateRow = new delegateRowHover(false, this);
    ui->tableView_course->setItemDelegate(delegateRow);


    proxyModel = new SortFilterProxyModelCourse(this);
    proxyModel->setSourceModel(tableModel);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->addFilterColumn(0); //name
    proxyModel->addFilterColumn(1); //location
    //    proxyModel->addFilterColumn(2);
    //    proxyModel->addFilterColumn(3);

    ui->tableView_course->setModel(proxyModel);
    ui->tableView_course->verticalHeader()->setDefaultSectionSize(60);


    ui->tableView_course->sortByColumn(0, Qt::AscendingOrder);
    ui->tableView_course->setColumnWidth(0, 300);
    ui->tableView_course->setColumnWidth(1, 150);
    ui->tableView_course->setColumnWidth(2, 100);
    ui->tableView_course->setColumnWidth(3, 100);
    ui->tableView_course->setColumnWidth(4, 100);
    ui->tableView_course->horizontalHeader()->setStretchLastSection(true);
    ui->tableView_course->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tableView_course->setSelectionBehavior(QAbstractItemView::SelectRows);

    //    ui->tableView_course->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);


    ui->tableView_course->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableView_course->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->tableView_course->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(tableViewSelectionChanged(QItemSelection,QItemSelection)));
    connect(ui->tableView_course, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customMenuRequested(QPoint)) );
    connect(ui->tableView_course->verticalHeader(), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customMenuRequested(QPoint)) );


    contextMenu = new QMenu(this);
    actionEdit = contextMenu->addAction(QIcon(":/image/icon/edit"), tr("Edit"));
    actionDelete = contextMenu->addAction(QIcon(":/image/icon/delete"), tr("Delete"));
    actionSetAsDone = contextMenu->addAction(QIcon(":/image/icon/check"), tr("Set As Done"));
    contextMenu->addSeparator();
    actionOpenFolder = contextMenu->addAction(QIcon(":/image/icon/folder"), tr("Open in folder"));


    connect(actionEdit, SIGNAL(triggered()), this, SLOT(editCourse()) );
    connect(actionDelete, SIGNAL(triggered()), this, SLOT(deleteCourse()) );
    connect(actionSetAsDone, SIGNAL(triggered()), this, SLOT(setAsDone()) );
    connect(actionOpenFolder, SIGNAL(triggered()), this, SLOT(openFolderCourse()) );
    actionEdit->setEnabled(false);
    actionDelete->setEnabled(false);
    actionOpenFolder->setEnabled(false);



}


///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::editCourse() {

    qDebug() << "editWorkout";
    Course course = tableModel->getCourseAtRow(indexSourceSelected);

//    qDebug() << "course name is" << course.getName();

    emit editCourse(course);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::deleteCourse() {

    Course course = tableModel->getCourseAtRow(indexSourceSelected);

    // Ask confirmation
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Question);
    QString textToShow = tr("Are you sure you want to delete <b>%1</b>?").arg(course.getName());
    msgBox.setText(textToShow);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if (msgBox.exec() == QMessageBox::Yes) {
        qDebug() << "Yes was clicked";
        /// Delete from model
        tableModel->removeRows(indexSourceSelected.row(), 1, QModelIndex());

        /// Delete local file xml
        Util::deleteLocalFile(course.getFilePath());

        //  remove from lst done
        account->hashCourseDone.remove(course.getName());
    }
    tableViewSelectionChanged(QItemSelection(), QItemSelection());

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::setAsDone() {

    Course course = tableModel->getCourseAtRow(indexSourceSelected);

    qDebug() << "Set AS DONE!" << course.getName();

    if (account->hashCourseDone.contains(course.getName())) {
        account->hashCourseDone.remove(course.getName());
    }
    else {
        account->hashCourseDone.insert(course.getName());
    }
    ui->tableView_course->selectionModel()->clearSelection();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::openFolderCourse() {

    Course course = tableModel->getCourseAtRow(indexSourceSelected);

    qDebug() << "openFolder" << course.getFilePath();
    Util::openCourseFolder(course.getFilePath());
}



///////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::customMenuRequested(QPoint pos) {

    qDebug() << "customMenuRequested2";

    QModelIndex index = ui->tableView_course->indexAt(pos);
    if (!index.isValid() )
        return;

    contextMenu->popup(ui->tableView_course->viewport()->mapToGlobal(pos));

    qDebug() << "customMenuRequested End";
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::tableViewSelectionChanged(QItemSelection, QItemSelection) {


    /// default behavior = disabled
    actionEdit->setEnabled(false);
    actionDelete->setEnabled(false);
    actionSetAsDone->setEnabled(false);
    actionOpenFolder->setEnabled(false);



    QModelIndexList lstIndex = ui->tableView_course->selectionModel()->selectedRows();

    if (lstIndex.size() < 1) {
        return;
    }

    QModelIndex index = lstIndex.at(0);
    indexSourceSelected = proxyModel->mapToSource(index);
    Course course = tableModel->getCourseAtRow(indexSourceSelected);

    qDebug() << "name:" << course.getName() << "course distance:" << course.getDistanceMeters() << "courseTyPe:" << course.getCourseType();

    if (account->hashCourseDone.contains(course.getName())) {
        actionSetAsDone->setText(tr("Unset Done"));
    }
    else {
        actionSetAsDone->setText(tr("Set As Done"));
    }
    actionSetAsDone->setEnabled(true);

    if (course.getCourseType() != Course::COURSE_TYPE::USER_MADE) {
        actionDelete->setEnabled(false);
        actionOpenFolder->setEnabled(false);
        actionEdit->setEnabled(true);
    }
    else {
        actionEdit->setEnabled(true);
        actionDelete->setEnabled(true);
        actionOpenFolder->setEnabled(true);
    }


}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::addJSObject_CoursePage() {


    qDebug() << "**** addJSObject_CoursePage";
    ui->webView_courses->page()->mainFrame()->addToJavaScriptWindowObject(QString("coursePage"), this);


    if (!account->show_included_course) {
        ui->checkBox_showIncludeCourse->setChecked(false);
        filterChangedCourseType(false);
    }


}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::on_pushButton_refresh_clicked()
{
    refreshUserCourse();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::refreshUserCourse() {

    qDebug() << "refresh Course list!";

    //Delete user workout and reparse them
    tableModel->deleteAllUserMadeCourses();
    parseUserCourses();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::parseIncludedCourses() {

    xmlUtil = new XmlUtil(this);

    QList<Course> lstCourseIncluded = xmlUtil->getLstCourseIncluded();

    tableModel->addListCourse(lstCourseIncluded);

}

/////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::parseUserCourses() {

    xmlUtil = new XmlUtil(this);
    QList<Course> lstCourseUser = xmlUtil->getLstUserCourse();
    tableModel->addListCourse(lstCourseUser);
}

/////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::on_checkBox_showIncludeCourse_clicked(bool checked)
{
    qDebug() << "OK NOW Include course INCLUDED?" << checked;

    filterChangedCourseType(checked);
}


/////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::filterChangedCourseType(bool includeCourse) {

    qDebug() << "included course should be shown" << includeCourse;

    if (includeCourse)
        proxyModel->addFilterCourseType(Course::COURSE_TYPE::INCLUDED);
    else
        proxyModel->addFilterCourseType(Course::COURSE_TYPE::USER_MADE);

    proxyModel->invalidate();


    account->show_included_course = includeCourse;
    account->saveDisplayPrefs(); // persist locally so the choice survives a restart
}








/////////////////////////////////////////////////////////////////////////////////////////////////////////
void Main_CoursePage::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}


