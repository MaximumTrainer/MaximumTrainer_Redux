#ifndef POWEREDITOR_H
#define POWEREDITOR_H

#include <QWidget>
#include "interval.h"
#include "account.h"

namespace Ui {
class PowerEditor;
}

class PowerEditor : public QWidget
{
    Q_OBJECT

public:
    explicit PowerEditor(QWidget *parent = 0);
    ~PowerEditor();


    void setInterval(const Interval &interval);
    Interval getInterval() { return myInterval; }


signals :
    void endEdit();
    void cancelEdit();


private slots:
    void on_comboBox_power_currentIndexChanged(int index);
    void on_spinBox_ftpStart_valueChanged(double arg1);
    void on_spinBox_ftpEnd_valueChanged(double arg1);
    void on_spinBox_rangePower_valueChanged(int arg1);
    void on_spinBox_leftBalance_valueChanged(int arg1);
    void on_spinBox_rightBalance_valueChanged(int arg1);

    void on_pushButton_ok_clicked();

    void on_comboBox_FTPorWatts_currentIndexChanged(int index);

    void on_checkBox_testInterval_clicked(bool checked);

    void on_pushButton_default_clicked();

    void on_checkBox_balance_clicked(bool checked);

private :
    void updateInterval();


private:
    Ui::PowerEditor *ui;
    Interval myInterval;

    bool editingInWatts;
    Account *account;
};

#endif // POWEREDITOR_H
