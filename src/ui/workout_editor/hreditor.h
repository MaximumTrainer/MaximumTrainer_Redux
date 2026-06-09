#ifndef HREDITOR_H
#define HREDITOR_H

#include <QWidget>
#include "interval.h"
#include "account.h"

namespace Ui {
class HrEditor;
}

class HrEditor : public QWidget
{
    Q_OBJECT

public:
    explicit HrEditor(QWidget *parent = 0);
    ~HrEditor();

    void setInterval(const Interval &interval);
    Interval getInterval() { return myInterval; }

signals :
    void endEdit();
    void cancelEdit();

private slots:
    void on_comboBox_hr_currentIndexChanged(int index);
    void on_spinBox_lthrStart_valueChanged(double arg1);
    void on_spinBox_lthrEnd_valueChanged(double arg1);
    void on_spinBox_rangeLthr_valueChanged(int arg1);

    void on_pushButton_ok_clicked();

    void on_pushButton_default_clicked();

    void on_comboBox_LTHRorBpm_currentIndexChanged(int index);

private:
    Ui::HrEditor *ui;

    Interval myInterval;

    Account *account;
    bool editingInBpm;
};

#endif // HREDITOR_H
