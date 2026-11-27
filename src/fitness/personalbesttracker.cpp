#include "personalbesttracker.h"

#include <QCoreApplication>

#include <algorithm>

const QVector<int> &PersonalBestTracker::trackedDurations()
{
    static const QVector<int> durations{5, 60, 300, 1200};
    return durations;
}

void PersonalBestTracker::start(const QVector<Record> &records)
{
    m_slots.clear();
    for (const Record &record : records)
        if (record.seconds > 0)
            m_slots.append(Slot{record, {}, 0.0, 0.0, 0});
    m_active = true;
}

void PersonalBestTracker::stop()
{
    m_slots.clear();
    m_active = false;
}

PersonalBestTracker::Level PersonalBestTracker::levelFor(const Slot &slot, double watts)
{
    // Whole watts, as shown: 298.4 W does not beat a 298 W best.
    const int rounded = qRound(watts);
    if (slot.record.allTimeWatts > 0 && rounded > qRound(slot.record.allTimeWatts))
        return AllTime;
    if (slot.record.recentWatts > 0 && rounded > qRound(slot.record.recentWatts))
        return Recent;
    return None;
}

int PersonalBestTracker::settleSeconds(int seconds)
{
    return std::clamp(seconds / 10, 3, 30);
}

PersonalBestTracker::Event PersonalBestTracker::takePending(Slot &slot)
{
    const Level level = levelFor(slot, slot.peak);
    Event event;
    event.seconds = slot.record.seconds;
    event.watts = slot.peak;
    event.allTime = level == AllTime;
    event.previousWatts = event.allTime ? slot.record.allTimeWatts : slot.record.recentWatts;
    // Today's effort is the record to beat from now on, so a stronger effort
    // later in the ride is reported too.
    slot.record.recentWatts = slot.peak;
    if (event.allTime)
        slot.record.allTimeWatts = slot.peak;
    slot.peak = 0.0;
    slot.secondsSincePeak = 0;
    return event;
}

QVector<PersonalBestTracker::Event> PersonalBestTracker::addSecond(double watts)
{
    QVector<Event> events;
    if (!m_active)
        return events;
    const double sample = std::max(0.0, watts);

    for (Slot &slot : m_slots) {
        slot.window.push_back(sample);
        slot.sum += sample;
        if (static_cast<int>(slot.window.size()) > slot.record.seconds) {
            slot.sum -= slot.window.front();
            slot.window.pop_front();
        }
        if (static_cast<int>(slot.window.size()) < slot.record.seconds)
            continue;

        const double average = slot.sum / slot.record.seconds;
        if (levelFor(slot, average) != None && average > slot.peak) {
            slot.peak = average;
            slot.secondsSincePeak = 0;
        } else if (slot.peak > 0.0 && ++slot.secondsSincePeak >= settleSeconds(slot.record.seconds)) {
            events.append(takePending(slot));
        }
    }
    return events;
}

QVector<PersonalBestTracker::Event> PersonalBestTracker::finish()
{
    QVector<Event> events;
    for (Slot &slot : m_slots)
        if (slot.peak > 0.0)
            events.append(takePending(slot));
    return events;
}

QString PersonalBestTracker::durationLabel(int seconds)
{
    if (seconds < 60)
        return QCoreApplication::translate("PersonalBestTracker", "%1 s").arg(seconds);
    return QCoreApplication::translate("PersonalBestTracker", "%1 min").arg(seconds / 60);
}
