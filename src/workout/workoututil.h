#ifndef WORKOUTUTIL_H
#define WORKOUTUTIL_H

#include "workout.h"
#include "interval.h"



class WorkoutUtil
{
public:

    WorkoutUtil();


    static QList<Workout> getListWorkoutBase();
    static Workout getWorkoutMap(int userFTP);


private:

    static Workout FTP();
    static Workout FTP_8min();

    static Workout MAP(int userFTP);




};

#endif // WORKOUTUTIL_H
