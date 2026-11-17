#ifndef PLANOBJECT_H
#define PLANOBJECT_H

#include <QObject>

class PlanObject : public QObject
{
    Q_OBJECT
public:
    explicit PlanObject(QObject *parent = nullptr);



public slots:
    void goToWorkout(QString name);



signals:
    void signal_goToWorkout(QString name);
};

#endif // PLANOBJECT_H
