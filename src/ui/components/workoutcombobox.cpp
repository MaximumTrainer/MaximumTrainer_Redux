#include "workoutcombobox.h"
#include "workout.h"

#include <QDebug>


WorkoutComboBox::WorkoutComboBox(QWidget *parent) : QComboBox(parent) {

    updateUi();
}


void WorkoutComboBox::updateUi() {

    this->clear();
    addItem("", QVariant() );


    this->model()->sort(Qt::AscendingOrder);


}

void WorkoutComboBox::changeEvent(QEvent *event) {

    if (event->type() == QEvent::LanguageChange) {
        updateUi();
    }
    else {
        QComboBox::changeEvent(event);
    }
}

