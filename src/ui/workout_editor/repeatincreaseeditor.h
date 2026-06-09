#ifndef REPEATINCREASEEDITOR_H
#define REPEATINCREASEEDITOR_H

#include <QWidget>
#include "interval.h"

namespace Ui {
class RepeatIncreaseEditor;
}

class RepeatIncreaseEditor : public QWidget
{
    Q_OBJECT

public:
    explicit RepeatIncreaseEditor(QWidget *parent = 0);
    ~RepeatIncreaseEditor();

    void setInterval(const Interval &interval);
    Interval getInterval() { return myInterval; }

signals:
    void endEdit();
    void cancelEdit();


private slots:
    void on_doubleSpinBox_increaseFTP_valueChanged(double arg1);

    void on_spinBox_increaseCadence_valueChanged(int arg1);

    void on_doubleSpinBox_increaseLTHR_valueChanged(double arg1);

private:
    Ui::RepeatIncreaseEditor *ui;

    Interval myInterval;
};

#endif // REPEATINCREASEEDITOR_H
