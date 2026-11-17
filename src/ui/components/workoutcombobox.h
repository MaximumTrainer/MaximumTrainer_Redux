#ifndef WORKOUTCOMBOBOX_H
#define WORKOUTCOMBOBOX_H

#include <QEvent>
#include <QComboBox>

class WorkoutComboBox : public QComboBox
{
     Q_OBJECT

public:
    explicit WorkoutComboBox(QWidget *parent = nullptr);

    void changeEvent(QEvent *event) override;

private:
    void updateUi();
};

#endif // WORKOUTCOMBOBOX_H
