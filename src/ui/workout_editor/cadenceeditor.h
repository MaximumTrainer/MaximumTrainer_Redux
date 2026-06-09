#ifndef CADENCEEDITOR_H
#define CADENCEEDITOR_H

#include <QWidget>
#include "interval.h"

namespace Ui {
class CadenceEditor;
}

class CadenceEditor : public QWidget
{
    Q_OBJECT

public:
    explicit CadenceEditor(QWidget *parent = 0);
    ~CadenceEditor();
//    void setRow(int row) { this->row = row; }
//    int getRow() { return this->row; }

    void setInterval(const Interval &interval);
    Interval getInterval() { return myInterval; }

signals:
    void endEdit();
    void cancelEdit();

private slots:
    void on_comboBox_cadence_currentIndexChanged(int index);
    void on_spinBox_cadenceStart_valueChanged(int arg1);
    void on_spinBox_cadenceEnd_valueChanged(int arg1);
    void on_spinBox_rangeCadence_valueChanged(int arg1);

    void on_pushButton_ok_clicked();

    void on_pushButton_default_clicked();

private:
    Ui::CadenceEditor *ui;

    Interval myInterval;
};

#endif // CADENCEEDITOR_H






