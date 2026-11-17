#ifndef IMPORTERWORKOUT_H
#define IMPORTERWORKOUT_H

#include <QObject>

#include "workout.h"

class ImporterWorkout : public QObject
{
    Q_OBJECT
public:
    explicit ImporterWorkout(QObject *parent = nullptr);
    ~ImporterWorkout() override;


    static Workout importWorkoutFromFile(QString filename, int userFTP);
    static bool batchImportWorkoutFromFolder(QString folderName, int userFTP);




private :
    static Workout parseMrcErgFile(QString filename, int userFTP);


};

#endif // IMPORTERWORKOUT_H
