#ifndef PLANADHERENCE_H
#define PLANADHERENCE_H

#include <QString>
#include <QDate>

/// A single plan-adherence record for one workout session.
struct PlanAdherenceEntry
{
    enum Status {
        Completed,    ///< FIT file saved / auto-detected
        Skipped,      ///< User manually marked as skipped
        Substituted   ///< User performed a different workout
    };

    QDate   date;
    QString workoutName;
    Status  status      = Completed;
    QString note;
    QString fitFilePath; ///< Set for Completed entries

    bool isValid() const { return date.isValid() && !workoutName.isEmpty(); }
};

#endif // PLANADHERENCE_H
