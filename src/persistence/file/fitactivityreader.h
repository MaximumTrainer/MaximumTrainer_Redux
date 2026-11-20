#ifndef FITACTIVITYREADER_H
#define FITACTIVITYREADER_H

#include <QString>
#include "workouthistorysummary.h"
#include "fit_session_mesg_listener.hpp"

class FitActivityReader : public fit::SessionMesgListener
{
public:
    static WorkoutHistorySummary readFile(const QString &filePath);
    /// Workout name from a history file name: yyyy-MM-dd(hh-mm-ss)-MT-username-WorkoutName.fit
    static QString workoutNameFromFileName(const QString &filePath);

private:
    void OnMesg(fit::SessionMesg &mesg) override;

    WorkoutHistorySummary m_summary;
};

#endif // FITACTIVITYREADER_H
