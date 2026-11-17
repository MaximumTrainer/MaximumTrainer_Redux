#ifndef INFOSWORKOUT_H
#define INFOSWORKOUT_H

#include <QWidget>
#include <QLocale>

namespace Ui {
class infosWorkout;
}


class infosWorkout : public QWidget
{
    Q_OBJECT


public:
    explicit infosWorkout(QWidget *parent = nullptr);
    ~infosWorkout() override;

    void setStopped(bool b);
    void setDistanceVisible(bool b);
    void setDistanceInMile(bool b);



public slots:
    void NP_Changed(double a);
    void IF_Changed(double a);
    void TSS_Changed(double a);
    void calories_Changed(double a);

    void distanceChanged(double meters);


private:
    Ui::infosWorkout *ui;

    bool inMile;

    bool isStopped;
    QLocale locale;
};

#endif // INFOSWORKOUT_H
