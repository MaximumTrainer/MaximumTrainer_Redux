#ifndef USERSTUDIOWIDGET_H
#define USERSTUDIOWIDGET_H

#include <QWidget>
#include <QLocale>

namespace Ui {
class UserStudioWidget;
}

class UserStudioWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UserStudioWidget(int userID, QString displayName, double FTP, double LTHR, QWidget *parent = nullptr);
    ~UserStudioWidget() override;

    void setStopped(bool b);

    void setPowerSectionHidden();
    void setCadSectionHidden();
    void setHrSectionHidden();

    void setResultTest(QString text);



public slots:

    void setPowerValue(int value);
    void setCadenceValue(int value);
    void setHrValue(int value);

    void setTargetPower(double percentageFTP, int range);
    void setTargetCadence(int value, int range);
    void setTargetHr(double percentageLTHR, int range);

    void setMaxIntervalPower(double val);
    void setMaxWorkoutPower(double val);
    void setAvgIntervalPower(double val);
    void setAvgWorkoutPower(double val);

    void setMaxIntervalCad(double val);
    void setMaxWorkoutCad(double val);
    void setAvgIntervalCad(double val);
    void setAvgWorkoutCad(double val);

    void setMaxIntervalHr(double val);
    void setMaxWorkoutHr(double val);
    void setAvgIntervalHr(double val);
    void setAvgWorkoutHr(double val);


    void setNormalizedPower(double normalizedPower);
    void setIntensityFactor(double intensityFactor);
    void setTrainingStressScore(double trainingStressScore);
    void setCalories(double calories);




private:
    Ui::UserStudioWidget *ui;
    QLocale locale;

    bool isStopped;
    int currentColorPower; // 0 = Okay, -1= TooLow, 1=TooHigh
    int currentColorCad;
    int currentColorHr;

    int targetPower;
    int targetPowerRange;

    int targetCad;
    int targetCadRange;

    int targetHr;
    int targetHrRange;

    int userID;
    QString displayName;
    double FTP;
    double LTHR;
};

#endif // USERSTUDIOWIDGET_H
