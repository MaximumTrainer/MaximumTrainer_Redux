#include "intervaltablemodel.h"
#include <QFont>
#include <QBrush>
#include <QCursor>


IntervalTableModel::IntervalTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{

    this->account = qApp->property("Account").value<Account*>();
}


void IntervalTableModel::updateRowSourceInterval() {


    for (int i=0; i<lstInterval.size(); i++)
    {
        lstInterval[i].setSourceRowLstInterval(i);
    }
}

void IntervalTableModel::setListInterval(QList<Interval> lstInterval) {

    beginResetModel();
    this->lstInterval = std::move(lstInterval);

    updateRowSourceInterval();
    endResetModel();

}

void IntervalTableModel::resetLstInterval() {

    beginResetModel();
    this->lstInterval.clear();
    endResetModel();
}


void  IntervalTableModel::removeIntervalRepeatData(int firstRow, int lastRow) {

    if (lstInterval.size() < 1)
        return;

    for(int i=firstRow; i<=lastRow; i++) {
        if (i < lstInterval.size()) {
            Interval interval = lstInterval.at(i);
            interval.setRepeatIncreaseFTP(0);
            interval.setRepeatIncreaseCadence(0);
            interval.setRepeatIncreaseLTHR(0);
            lstInterval.replace(i, interval);
        }
    }
}

QList<Interval> IntervalTableModel::getLstInterval() {
    return this->lstInterval;
}


Interval IntervalTableModel::getIntervalAtRow(const QModelIndex &index) {

    return (lstInterval.at(index.row()));
}


int IntervalTableModel::rowCount(const QModelIndex &parent) const {

    Q_UNUSED(parent);
    return lstInterval.size();

}


int IntervalTableModel::columnCount(const QModelIndex &parent) const {

    Q_UNUSED(parent);
    return 8;

    /// last column is not used (dummy column)

}


QVariant IntervalTableModel::data(const QModelIndex &index, int role) const {


    if (!index.isValid())
        return QVariant();

    if (index.row() >= lstInterval.size() || index.row() < 0)
        return QVariant();

    if (index.column() == 7 )  //dummy column for repeat data
        return QVariant();


    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;


    if (role == Qt::EditRole) {

        Interval interval = lstInterval.at(index.row());

        /// Duration
        if (index.column() == 1)  {
            return interval.getDurationQTime();
        }
        /// Power - Cadence - Hr
        else if (index.column() == 2 || index.column() == 3 || index.column() == 4 || index.column() == 6) {
            QVariant stored;
            stored.setValue(interval);
            return stored;
        }
        /// Display Msg
        else if (index.column() == 5)  {
            return interval.getDisplayMessage();
        }
    }

    if (role == Qt::DisplayRole) {

        Interval interval = lstInterval.at(index.row());

        if (index.column() == 0)  {
            return "::";
        }
        /// Duration
        else if (index.column() == 1)  {
            if (interval.getDurationQTime().hour() > 0)
                return interval.getDurationQTime().toString("h:mm:ss");
            else
                return interval.getDurationQTime().toString("mm:ss");
        }
        /// ----------------------- Power -------------------------------------------------------------
        else if(index.column() == 2)
        {
            double ftpStart = interval.getFTP_start() *100;
            double ftpEnd = interval.getFTP_end() *100;
            int rightTarget = interval.getRightPowerTarget();
            double userFTP = account->FTP;


            QString toShow;
            if (interval.getPowerStepType() == Interval::PROGRESSIVE) {

                QString ftpStartValue;
                if (ftpStart - ((int)ftpStart) != 0.0)
                    ftpStartValue = QString::number(ftpStart, 'f', 1);
                else
                    ftpStartValue = QString::number(ftpStart);
                QString ftpEndValue;
                if (ftpEnd - ((int)ftpEnd) != 0.0)
                    ftpEndValue = QString::number(ftpEnd, 'f', 1);
                else
                    ftpEndValue = QString::number(ftpEnd);

                int wattsStart = qRound(ftpStart*userFTP/100.0);
                int wattsEnd = qRound(ftpEnd*userFTP/100.0);

                toShow += " " + ftpStartValue + "-" + ftpEndValue + "%";
                toShow += " (" + QString::number(wattsStart) + "-" + QString::number(wattsEnd) + " watts)";

            }
            else if (interval.getPowerStepType() == Interval::FLAT) {

                QString ftpStartValue;
                if (ftpStart - ((int)ftpStart) != 0.0)
                    ftpStartValue = QString::number(ftpStart, 'f', 1);
                else
                    ftpStartValue = QString::number(ftpStart);

                int wattsStart = qRound(ftpStart*userFTP/100.0);

                toShow += " " + ftpStartValue + "%";
                toShow += " (" + QString::number(wattsStart) +  " watts)";
            }
            /// Show balance percentage
            if (rightTarget != -1) {
                toShow += tr("\nLeft:") + QString::number(100-rightTarget) + tr(" Right:") + QString::number(rightTarget);
            }
            return toShow;
        }

        /// ----------------------- Cadence -------------------------------------------------------------
        else if (index.column() == 3)
        {
            int cadenceStart = interval.getCadence_start();
            int cadenceEnd = interval.getCadence_end();

            QString toShow;
            if (interval.getCadenceStepType() == Interval::PROGRESSIVE) {
                toShow +=  QString::number(cadenceStart) + "-" + QString::number(cadenceEnd) + " rpm";
            }
            else if (interval.getCadenceStepType() == Interval::FLAT) {
                toShow +=  QString::number(cadenceStart) + " rpm";;
            }
            return toShow;
        }

        /// ----------------------- Hr -------------------------------------------------------------
        else if (index.column() == 4)
        {
            double hrStart = interval.getHR_start() * 100;
            double hrEnd = interval.getHR_end() * 100;
            double userLTHR = account->LTHR;


            QString toShow;
            if (interval.getHRStepType() == Interval::PROGRESSIVE) {

                QString LTHRStartValue;
                if (hrStart - ((int)hrStart) != 0.0)
                    LTHRStartValue = QString::number(hrStart, 'f', 1);
                else
                    LTHRStartValue = QString::number(hrStart);
                QString LTHREndValue;
                if (hrEnd - ((int)hrEnd) != 0.0)
                    LTHREndValue = QString::number(hrEnd, 'f', 1);
                else
                    LTHREndValue = QString::number(hrEnd);

                int bpmStart = qRound(hrStart*userLTHR/100.0);
                int bpmEnd = qRound(hrEnd*userLTHR/100.0);

                toShow += " " + LTHRStartValue + "-" + LTHREndValue + "%";
                toShow += " (" + QString::number(bpmStart) + "-" + QString::number(bpmEnd) + " bpm)";
            }
            else if (interval.getHRStepType() == Interval::FLAT) {

                QString LTHRStartValue;
                if (hrStart - ((int)hrStart) != 0.0)
                    LTHRStartValue = QString::number(hrStart, 'f', 1);
                else
                    LTHRStartValue = QString::number(hrStart);

                int bpmStart = qRound(hrStart*userLTHR/100.0);

                toShow += " " + LTHRStartValue + "%";
                toShow += " (" + QString::number(bpmStart) +  " bpm)";
            }
            return toShow;


        }
        /// Display Msg
        else if(index.column() == 5) {
            return interval.getDisplayMessage();
        }
        /// Repeat Interval percentage increase — only shows when values are non-zero,
        /// which only occurs when a repeat widget covers this row.
        else if (index.column() == 6) {
            QString textToShow = "";
            double repeatPercFtp = interval.getRepeatIncreaseFTP();
            if (repeatPercFtp != 0) {
                textToShow += QString::number(repeatPercFtp) + " " + tr("%FTP");
            }
            int repeatCadence = interval.getRepeatIncreaseCadence();
            if (repeatCadence != 0) {
                if (textToShow != "") textToShow += "\n";
                textToShow += QString::number(repeatCadence) + " " + tr("rpm");
            }
            double repeatLthr = interval.getRepeatIncreaseLTHR();
            if (repeatLthr != 0) {
                if (textToShow != "") textToShow += "\n";
                textToShow += QString::number(repeatLthr) + " " + tr("%LTHR");
            }
            return textToShow;
        }
        else {
            return "-";
        }
    }

    if (role == Qt::UserRole) {
        Interval interval = lstInterval.at(index.row());
        QVariant stored;
        stored.setValue(interval);
        return stored;
    }

    return QVariant();

}


bool IntervalTableModel::setData(const QModelIndex &index, const QVariant &value, int role) {


    if (role == Qt::EditRole) {

        Interval interval = lstInterval.at(index.row());

        /// Duration
        if (index.column() == 1)  {
            QTime time = value.toTime();
            interval.setTime(time);
            lstInterval[index.row()].setTime(time);
        }
        /// Power, Cadence, Hr
        else if (index.column() == 2 || index.column() == 3 || index.column() == 4 || index.column() == 6)  {
            Interval interval = value.value<Interval>();
            lstInterval.replace(index.row(), interval);
        }
        /// Display Msg
        else if (index.column() == 5)  {
            QString msg = value.toString();
            interval.setDisplayMsg(msg);
            lstInterval[index.row()].setDisplayMsg(msg);
        }
    }


    if (role == Qt::UserRole) {
        Interval interval = value.value<Interval>();
        lstInterval.replace(index.row(), interval);
    }


    emit dataChanged();
    return true;
}


QVariant IntervalTableModel::headerData(int section, Qt::Orientation  orientation, int role) const {


    if (role == Qt::ToolTipRole) {


        if (section == 1) {
            return tr("Duration (hh:mm:ss)");
        }
        else if (section == 2) {
            return tr("Target - Power (%FTP)");
        }
        else if (section == 3) {
            return tr("Target - Cadence (rpm)");
        }
        else if (section == 4) {
            return tr("Target - Heart Rate (%LTHR)");
        }
        else if (section == 5) {
            return tr("Display message shown at the start of the step");
        }
        else if (section == 5) {
            return tr("Target change on each Repeat loop");
        }
        else if (section == 5) {
            return tr("Number of Repeats");
        }
    }

    if (role != Qt::DisplayRole)
        return QVariant();


    if (orientation == Qt::Vertical) {
        return QString::number(section+1);
    }
    else {
        if (section == 0) {
            return "";
        }
        else if (section == 1) {
            return tr("Duration");
        }
        else if (section == 2) {
            return tr("Target - Power");
        }
        else if (section == 3) {
            return tr("Target - Cadence");
        }
        else if (section == 4) {
            return tr("Target - Heart Rate");
        }
        else if (section == 5) {
            return tr("Display Message");
        }
        else if (section == 6) {
            return tr("Repeat Change");
        }
        else {
            return tr("# Repeat");
        }
    }
}


bool IntervalTableModel::copyRows(int row) {

    beginInsertRows(QModelIndex(), rowCount(), rowCount());

    Interval interval = lstInterval.at(row);
    lstInterval.append(interval);
    updateRowSourceInterval();

    endInsertRows();
    emit dataChanged();
    return true;
}


bool IntervalTableModel::insertRows(int position, int rows, const QModelIndex &index)
{
    Q_UNUSED(index);
    beginInsertRows(QModelIndex(), position, position+rows-1);

    for (int row=0; row < rows; row++) {
        Interval interval(QTime(0,5,0), "",
                          Interval::FLAT,  0.5, 0.5, 20, -1,
                          Interval::NONE,  90, 90, 5,
                          Interval::NONE, 0.5, 0.5, 15,
                          false, 0, 0, 0);
        lstInterval.insert(position, interval);
    }
    updateRowSourceInterval();

    endInsertRows();
    emit dataChanged();
    return true;
}


bool IntervalTableModel::removeRows(int position, int rows, const QModelIndex &index)
{
    Q_UNUSED(index);
    beginRemoveRows(QModelIndex(), position, position+rows-1);


    for (int row=0; row < rows; ++row) {
        lstInterval.removeAt(position);
    }


    updateRowSourceInterval();


    endRemoveRows();
    emit dataChanged();

    return true;
}


Qt::DropActions IntervalTableModel::supportedDropActions() const {
    return Qt::MoveAction;
}

Qt::ItemFlags IntervalTableModel::flags(const QModelIndex &index) const
{

    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);


    if (index.isValid()) {

        if (index.column() == 0) {
            return  Qt::ItemIsDragEnabled | defaultFlags;
        }
        else if (index.column() == 7) {
            return Qt::NoItemFlags;
        }
        else {
            return Qt::ItemIsEditable | Qt::ItemIsDragEnabled | defaultFlags;
        }
    }
    // To show drop indicator
    else {
        return Qt::ItemIsDropEnabled | defaultFlags;
    }

}


QStringList IntervalTableModel::mimeTypes() const
{
    QStringList types;
    types << "mt/interval";
    return types;
}

QMimeData* IntervalTableModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);


    QList<int> lstRow;
    for (const QModelIndex &index : std::as_const(indexes))
    {
        if (!lstRow.contains(index.row()) && index.isValid()) {
            lstRow.append(index.row());
            Interval interval = data(index, Qt::UserRole).value<Interval>();
            stream << interval;
        }
    }
    mimeData->setData("mt/interval", encodedData);
    return mimeData;

}


bool IntervalTableModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{

    Q_UNUSED(column);

    if (action == Qt::IgnoreAction)
        return true;

    if (!data->hasFormat("mt/interval"))
        return false;


    int beginRow;
    if (row != -1)
        beginRow = row;

    else if (parent.isValid())
        beginRow = parent.row();
    else
        beginRow = rowCount(QModelIndex());

    QByteArray encodedData = data->data("mt/interval");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    QList<Interval> lstInterval;
    int rows = 0;

    while (!stream.atEnd()) {
        Interval interval;
        /// Deserialize stream into Object
        stream >> interval;
        lstInterval.append(interval);
        ++rows;
    }

    insertRows(beginRow, rows, QModelIndex());
    for (const Interval &interval : std::as_const(lstInterval)) {
        QModelIndex idx = index(beginRow, 0, QModelIndex());
        QVariant stored;
        stored.setValue(interval);
        setData(idx, stored, Qt::UserRole);
        beginRow++;
    }


    return true;
}
