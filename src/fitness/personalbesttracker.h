#ifndef PERSONALBESTTRACKER_H
#define PERSONALBESTTRACKER_H

#include <QString>
#include <QVector>

#include <deque>

/// Spots new power bests during a ride. For each tracked duration it keeps a
/// rolling average and compares it with the rider's best of the last 90 days
/// and of all time. An effort is reported once it is over (the average stopped
/// rising for a few seconds), so the reported value is the effort's peak, not
/// the first watt above the old record.
class PersonalBestTracker
{
public:
    struct Record {
        int seconds = 0;
        double recentWatts = 0.0;   ///< Best of the last 90 days; 0 = none.
        double allTimeWatts = 0.0;  ///< 0 = none.
    };

    struct Event {
        int seconds = 0;
        double watts = 0.0;
        double previousWatts = 0.0;
        bool allTime = false;
    };

    /// 5 s, 1 min, 5 min and 20 min.
    static const QVector<int> &trackedDurations();
    static constexpr int kRecentDays = 90;

    /// Starts tracking against \a records (one per tracked duration). A
    /// duration with no earlier best never fires, so a first ride is quiet.
    void start(const QVector<Record> &records);
    void stop();
    bool isActive() const { return m_active; }

    /// One second of power (negative = no data, counted as 0). Returns the
    /// efforts that just ended above a record.
    QVector<Event> addSecond(double watts);
    /// Reports efforts still in progress; call when the ride ends.
    QVector<Event> finish();

    /// "5 s", "1 min", "20 min".
    static QString durationLabel(int seconds);

private:
    enum Level { None = 0, Recent = 1, AllTime = 2 };

    struct Slot {
        Record record;
        std::deque<double> window;
        double sum = 0.0;
        double peak = 0.0;          ///< Highest average of the effort in progress.
        int secondsSincePeak = 0;
        Level reported = None;      ///< Each level is reported once per ride.
    };

    static Level levelFor(const Slot &slot, double watts);
    static int settleSeconds(int seconds);
    static Event takePending(Slot &slot);

    QVector<Slot> m_slots;
    bool m_active = false;
};

#endif // PERSONALBESTTRACKER_H
