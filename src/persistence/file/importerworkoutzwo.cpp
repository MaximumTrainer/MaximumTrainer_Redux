#include "importerworkoutzwo.h"

#include <QXmlStreamReader>
#include <QDebug>
#include <QTime>

#include "interval.h"

// ───────────────────────────────────────────────────────────────────────────────
// Helper: convert seconds to QTime
// ───────────────────────────────────────────────────────────────────────────────
static QTime secondsToQTime(int seconds)
{
    return QTime(0, 0, 0).addSecs(seconds);
}

// ───────────────────────────────────────────────────────────────────────────────
// Helper: build a single flat-power interval
// ───────────────────────────────────────────────────────────────────────────────
static Interval makeFlatInterval(int durationSecs, double power)
{
    return Interval(secondsToQTime(durationSecs),
                    QString(),
                    Interval::FLAT, power, power, 20, -1,
                    Interval::NONE, 0, 0, 5,
                    Interval::NONE, 0.0, 0.0, 15,
                    false, 0.0, 0, 0.0);
}

// ───────────────────────────────────────────────────────────────────────────────
// Helper: build a single progressive-power interval
// ───────────────────────────────────────────────────────────────────────────────
static Interval makeProgressiveInterval(int durationSecs, double powerLow, double powerHigh)
{
    return Interval(secondsToQTime(durationSecs),
                    QString(),
                    Interval::PROGRESSIVE, powerLow, powerHigh, 20, -1,
                    Interval::NONE, 0, 0, 5,
                    Interval::NONE, 0.0, 0.0, 15,
                    false, 0.0, 0, 0.0);
}

// ───────────────────────────────────────────────────────────────────────────────
Workout ImporterWorkoutZwo::importFromByteArray(const QByteArray &data,
                                                const QString &workoutName)
{
    QXmlStreamReader xml(data);

    QString name;
    QString author;
    QString description;
    QList<Interval> intervals;

    bool insideWorkoutFile = false;
    bool insideWorkout     = false;
    QString currentTextTag;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            const auto tag = xml.name();

            if (tag == QStringLiteral("workout_file")) {
                insideWorkoutFile = true;

            } else if (insideWorkoutFile && !insideWorkout) {
                // Metadata elements
                if (tag == QStringLiteral("name")
                    || tag == QStringLiteral("author")
                    || tag == QStringLiteral("description"))
                {
                    currentTextTag = tag.toString();
                } else if (tag == QStringLiteral("workout")) {
                    insideWorkout = true;
                }

            } else if (insideWorkout) {
                QXmlStreamAttributes attrs = xml.attributes();

                auto intAttr = [&](const char *attr, int def = 0) {
                    return attrs.hasAttribute(QLatin1String(attr))
                               ? attrs.value(QLatin1String(attr)).toInt()
                               : def;
                };
                auto dblAttr = [&](const char *attr, double def = 0.0) {
                    return attrs.hasAttribute(QLatin1String(attr))
                               ? attrs.value(QLatin1String(attr)).toDouble()
                               : def;
                };

                if (tag == QStringLiteral("SteadyState")) {
                    int    dur   = intAttr("Duration");
                    double power = dblAttr("Power");
                    // Intervals.icu exports a power-range step as a SteadyState
                    // with PowerLow/PowerHigh and no single Power — use the
                    // midpoint so it matches the manual ERG/MRC import instead of
                    // parsing as 0 W (#295).
                    if (power <= 0.0 &&
                        (attrs.hasAttribute(QLatin1String("PowerLow")) ||
                         attrs.hasAttribute(QLatin1String("PowerHigh")))) {
                        const double lo = dblAttr("PowerLow");
                        const double hi = dblAttr("PowerHigh", lo);
                        power = (lo + hi) / 2.0;
                    }
                    if (dur > 0)
                        intervals.append(makeFlatInterval(dur, power));

                } else if (tag == QStringLiteral("Warmup")
                           || tag == QStringLiteral("Ramp")
                           || tag == QStringLiteral("Cooldown")) {
                    int    dur       = intAttr("Duration");
                    double powerLow  = dblAttr("PowerLow");
                    double powerHigh = dblAttr("PowerHigh");
                    if (dur > 0)
                        intervals.append(makeProgressiveInterval(dur, powerLow, powerHigh));

                } else if (tag == QStringLiteral("IntervalsT")) {
                    int    repeat   = intAttr("Repeat", 1);
                    int    onDur    = intAttr("OnDuration");
                    int    offDur   = intAttr("OffDuration");
                    double onPower  = dblAttr("OnPower");
                    double offPower = dblAttr("OffPower");
                    for (int r = 0; r < repeat; ++r) {
                        if (onDur  > 0) intervals.append(makeFlatInterval(onDur,  onPower));
                        if (offDur > 0) intervals.append(makeFlatInterval(offDur, offPower));
                    }

                } else if (tag == QStringLiteral("FreeRide")) {
                    int dur = intAttr("Duration");
                    if (dur > 0) {
                        // FreeRide has no structured power target — NONE lets the trainer run in free/slope mode
                        Interval interval(secondsToQTime(dur),
                                          QString(),
                                          Interval::NONE, 0.0, 0.0, 20, -1,
                                          Interval::NONE, 0, 0, 5,
                                          Interval::NONE, 0.0, 0.0, 15,
                                          false, 0.0, 0, 0.0);
                        intervals.append(interval);
                    }

                } else {
                    qDebug() << "ImporterWorkoutZwo: skipping unrecognised element" << tag;
                }
            }

        } else if (token == QXmlStreamReader::Characters && !currentTextTag.isEmpty()) {
            // QXmlStreamReader may deliver text content in multiple Characters tokens;
            // accumulate them all to avoid dropping partial text.
            const QString text = xml.text().toString();
            if (currentTextTag == QStringLiteral("name"))
                name += text;
            else if (currentTextTag == QStringLiteral("author"))
                author += text;
            else if (currentTextTag == QStringLiteral("description"))
                description += text;

        } else if (token == QXmlStreamReader::EndElement) {
            const auto endTag = xml.name();
            if (endTag == QStringLiteral("name")
                || endTag == QStringLiteral("author")
                || endTag == QStringLiteral("description"))
            {
                currentTextTag.clear();
            } else if (endTag == QStringLiteral("workout")) {
                insideWorkout = false;
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "ImporterWorkoutZwo: XML parse error:" << xml.errorString()
                   << "at line" << xml.lineNumber();
        return Workout();
    }

    if (name.isEmpty()) {
        const QString trimmedId = workoutName.trimmed();
        name = trimmedId.isEmpty() ? QStringLiteral("Intervals.icu Workout") : trimmedId;
    } else {
        name = name.trimmed();
        if (name.isEmpty()) {
            const QString trimmedId = workoutName.trimmed();
            name = trimmedId.isEmpty() ? QStringLiteral("Intervals.icu Workout") : trimmedId;
        } else if (!workoutName.isEmpty() && name != workoutName.trimmed()) {
            // Append the workout ID to the name from the ZWO file so that two
            // different Intervals.icu workouts with the same title do not collide.
            name = name + QStringLiteral(" (") + workoutName.trimmed() + QStringLiteral(")");
        }
    }
    if (author.isEmpty())
        author = QStringLiteral("-");
    else
        author = author.trimmed();
    if (description.isEmpty())
        description = QStringLiteral("-");
    else
        description = description.trimmed();

    if (intervals.isEmpty()) {
        qWarning() << "ImporterWorkoutZwo: no intervals parsed from ZWO data";
        return Workout();
    }

    Workout workout(QString(), Workout::USER_MADE, intervals,
                    name, author, description,
                    QStringLiteral("-"), Workout::T_ENDURANCE);
    return workout;
}
