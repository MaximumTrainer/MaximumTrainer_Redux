#include "radiotablemodel.h"


#include <QColor>


RadioTableModel::RadioTableModel(QObject *parent) : QAbstractTableModel(parent) {

    rowHovered = -2;

    activeIndex = QModelIndex();
}

void RadioTableModel::setActiveIndex(QModelIndex index) {

    beginResetModel();

    this->activeIndex = index;


    endResetModel();

}


void RadioTableModel::changeRowHovered(int row) {
    this->rowHovered = row;
}


void RadioTableModel::addListRadio(QList<Radio> lstRadio) {

    beginResetModel();
    this->lstRadio.append(lstRadio);
    endResetModel();
}


void RadioTableModel::setRadios(const QList<Radio> &radios)
{
    beginResetModel();
    lstRadio = radios;
    endResetModel();
}

Radio RadioTableModel::getRadioAtRow(const QModelIndex &index) {

        return (lstRadio.at(index.row()));

}

void RadioTableModel::addRadio(const Radio& radio) {

    const int row = lstRadio.size();
    beginInsertRows(QModelIndex(), row, row);
    lstRadio.append(radio);
    endInsertRows();
}


void RadioTableModel::replaceRadioAtRow(int row, const Radio& radio) {

    if (row < 0 || row >= lstRadio.size())
        return;

    lstRadio.replace(row, radio);
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}


void RadioTableModel::removeRadioAtRow(int row) {

    if (row < 0 || row >= lstRadio.size())
        return;

    beginRemoveRows(QModelIndex(), row, row);
    lstRadio.removeAt(row);
    endRemoveRows();
}


bool RadioTableModel::moveRadio(int row, int step) {

    const int target = row + step;
    if (row < 0 || row >= lstRadio.size() || target < 0 || target >= lstRadio.size() || target == row)
        return false;

    // beginMoveRows' destination is the row the item lands before.
    const int destination = step > 0 ? target + 1 : target;
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), destination);
    lstRadio.move(row, target);
    endMoveRows();
    if (activeIndex.isValid() && (activeIndex.row() == row || activeIndex.row() == target))
        activeIndex = index(activeIndex.row() == row ? target : row, 0);
    return true;
}


QList<Radio> RadioTableModel::getAllRadios() const {
    return lstRadio;
}


int RadioTableModel::rowCount(const QModelIndex &parent) const {

    Q_UNUSED(parent);
    return lstRadio.size();

}


int RadioTableModel::columnCount(const QModelIndex &parent) const {

    Q_UNUSED(parent);
    return 4;

}


QVariant RadioTableModel::headerData(int section, Qt::Orientation  orientation, int role) const {


    if (role == Qt::ToolTipRole && orientation == Qt::Horizontal) {
        if (section == 0) {
            return tr("Name");
        }
        else if (section == 1) {
            return tr("Genre");
        }
        else if (section == 2) {
            return tr("Bitrate");
        }
        else if (section == 3) {
            return tr("Language");
        }
        else {
            return tr("colum not defined");
        }
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical) {
        return QString::number(section);
    }
    else {
        if (section == 0) {
            return tr("Name");
        }
        else if (section == 1) {
            return tr("Genre");
        }
        else if (section == 2) {
            return tr("Bitrate");
        }
        else if (section == 3) {
            return tr("Language");
        }
        else {
            return tr("colum not defined");
        }
    }

}

QVariant RadioTableModel::data(const QModelIndex &index, int role) const {


    if (!index.isValid())
        return QVariant();

    if (index.row() >= lstRadio.size() || index.row() < 0)
        return QVariant();


    Radio radio = lstRadio.at(index.row());


    /// TOOLTIP
    //    if (role == Qt::ToolTipRole) {
    //        return work.getDescription();
    //    }


    ///Background
    if (role == Qt::BackgroundRole && index.row() == activeIndex.row()) {
        QColor colorCadenceShapeTarget(150,150,150);
        return colorCadenceShapeTarget;
    }


    if (role == Qt::DisplayRole) {


        if (index.column() == 0)  {
            return radio.getName();
        }
        else if(index.column() == 1) {
            return radio.getGenre();
        }
        else if(index.column() == 2) {
            return radio.getBitrate();
        }
        else if(index.column() == 3) {
            return radio.getLanguage();
        }
        else {
            return tr("column not defined");
        }
    }


    return QVariant();
}

