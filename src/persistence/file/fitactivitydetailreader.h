#ifndef FITACTIVITYDETAILREADER_H
#define FITACTIVITYDETAILREADER_H

#include <QString>

#include "workouthistorydetail.h"
#include "fit_record_mesg_listener.hpp"
#include "fit_lap_mesg_listener.hpp"

/// Re-reads the full time-series (records) and per-lap summaries from a stored
/// activity FIT file, for the History drill-down detail view. Distinct from
/// FitActivityReader, which only extracts the single SessionMesg summary.
class FitActivityDetailReader : public fit::RecordMesgListener,
                                public fit::LapMesgListener
{
public:
    static WorkoutHistoryDetail readFile(const QString &filePath);

    void OnMesg(fit::RecordMesg &mesg) override;
    void OnMesg(fit::LapMesg &mesg) override;

private:
    WorkoutHistoryDetail m_detail;
    qint64               m_firstTimestamp = -1;  // FIT timestamp of first record
};

#endif // FITACTIVITYDETAILREADER_H
