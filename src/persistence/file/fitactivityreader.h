#ifndef FITACTIVITYREADER_H
#define FITACTIVITYREADER_H

#include <QString>
#include "workouthistorysummary.h"
#include "fit_session_mesg_listener.hpp"

class FitActivityReader : public fit::SessionMesgListener
{
public:
    static WorkoutHistorySummary readFile(const QString &filePath);

private:
    void OnMesg(fit::SessionMesg &mesg) override;

    WorkoutHistorySummary m_summary;
};

#endif // FITACTIVITYREADER_H
