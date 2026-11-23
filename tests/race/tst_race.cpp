#include <QtTest>

#include "racebot.h"
#include "racecourse.h"
#include "racephysics.h"
#include "racesimulation.h"

#include <cmath>

namespace {

RaceCourse loadCourse(const QString &id)
{
    const auto course = RaceCourse::fromFile(QStringLiteral(MT_COURSES_DIR "/%1.json").arg(id));
    if (!course)
        qFatal("cannot load course %s", qPrintable(id));
    return *course;
}

double steadySpeedKph(double watts, double massKg, double grade, double draft = 0.0)
{
    double speed = 0.0;
    for (int tick = 0; tick < 4000; ++tick)
        speed = RacePhysics::stepSpeed(speed, watts, massKg, 0.30, grade, draft, 0.1);
    return speed * 3.6;
}

} // namespace

class TstRace : public QObject
{
    Q_OBJECT

private slots:
    void bundledCourses_loadWithSaneShape_data();
    void bundledCourses_loadWithSaneShape();
    void loopCourse_wrapsLapsAndStopsAtTheFinish();
    void flatCourse_staysFlat();
    void climb_gradeAndNextClimbMatchTheProfile();
    void malformedCourse_isRejected();

    void physics_speedsMatchKnownNumbers();
    void drafting_fadesWithGapAndIsCapped();
    void drafting_savesMoreOnTheFlatThanOnAClimb();

    void defaultField_hasAPaceGroupAndASpread();
    void customField_followsCountAndRange();
    void bots_atTheSameWattsPerKgRideTogether();
    void youAtThreeWattsPerKg_canFollowThePaceGroupAndDraft();
    void youAtThreeWattsPerKg_climbWithThePaceGroup();
    void race_finishersAreRankedByTime();
};

void TstRace::bundledCourses_loadWithSaneShape_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<bool>("loop");
    QTest::addColumn<double>("minLapKm");
    QTest::addColumn<double>("maxLapKm");
    QTest::newRow("longchamp") << "longchamp" << true << 4.0 << 7.0;
    QTest::newRow("alpe-dhuez") << "alpe-dhuez" << false << 13.0 << 16.0;
    QTest::newRow("mont-ventoux") << "mont-ventoux" << false << 20.5 << 22.5;
}

void TstRace::bundledCourses_loadWithSaneShape()
{
    QFETCH(QString, id);
    QFETCH(bool, loop);
    QFETCH(double, minLapKm);
    QFETCH(double, maxLapKm);
    const RaceCourse course = loadCourse(id);
    QCOMPARE(course.id(), id);
    QCOMPARE(course.isLoop(), loop);
    QVERIFY(!course.name().isEmpty());
    QVERIFY(!course.place().isEmpty());
    QVERIFY(course.attribution().contains(QStringLiteral("OpenStreetMap")));
    QVERIFY2(course.lapDistanceM() > minLapKm * 1000 && course.lapDistanceM() < maxLapKm * 1000,
             qPrintable(QString::number(course.lapDistanceM())));
    QVERIFY(course.points().size() > 100);
}

void TstRace::loopCourse_wrapsLapsAndStopsAtTheFinish()
{
    const RaceCourse course = loadCourse(QStringLiteral("longchamp"));
    QVERIFY(course.laps() > 1);
    QCOMPARE(course.totalDistanceM(), course.lapDistanceM() * course.laps());

    const RaceCourse::Point first = course.pointAt(250.0);
    const RaceCourse::Point third = course.pointAt(2 * course.lapDistanceM() + 250.0);
    QCOMPARE(third.latitude, first.latitude);
    QCOMPARE(third.longitude, first.longitude);
    QCOMPARE(third.elevationM, first.elevationM);

    const RaceCourse::Point finish = course.pointAt(course.totalDistanceM() + 500.0);
    QCOMPARE(finish.latitude, course.points().constLast().latitude);
}

void TstRace::flatCourse_staysFlat()
{
    const RaceCourse course = loadCourse(QStringLiteral("longchamp"));
    for (double distance = 0.0; distance < course.lapDistanceM(); distance += 20.0)
        QVERIFY2(std::abs(course.gradeAt(distance)) < 0.05,
                 qPrintable(QStringLiteral("%1 %% at %2 m").arg(course.gradeAt(distance) * 100).arg(distance)));
    const double seam = course.lapDistanceM();
    QVERIFY(std::abs(course.elevationAt(seam - 1.0) - course.elevationAt(seam + 1.0)) < 1.0);
}

void TstRace::climb_gradeAndNextClimbMatchTheProfile()
{
    const RaceCourse course = loadCourse(QStringLiteral("alpe-dhuez"));
    const double rise = course.points().constLast().elevationM - course.points().constFirst().elevationM;
    QVERIFY2(rise > 1000.0 && rise < 1200.0, qPrintable(QString::number(rise)));

    const double middle = course.lapDistanceM() / 2;
    QVERIFY2(course.gradeAt(middle) > 0.04, qPrintable(QString::number(course.gradeAt(middle))));

    const auto climb = course.nextClimb(0.0);
    QVERIFY(climb.has_value());
    QVERIFY(climb->startsInM < 3000.0);
    QVERIFY(climb->lengthM >= 200.0);
    QVERIFY(climb->averageGrade >= 0.06);

    const RaceCourse flat = loadCourse(QStringLiteral("longchamp"));
    QVERIFY(!flat.nextClimb(0.0).has_value());
}

void TstRace::malformedCourse_isRejected()
{
    QVERIFY(!RaceCourse::fromJson("{}").has_value());
    QVERIFY(!RaceCourse::fromJson(R"({"points":[[0,1,2,3]]})").has_value());
    QVERIFY(!RaceCourse::fromJson(R"({"points":[[0,1,2,3],[0,1,2,3]]})").has_value());
    QVERIFY(RaceCourse::fromJson(R"({"points":[[0,1,2,3],[20,1,2,4]]})").has_value());
}

void TstRace::physics_speedsMatchKnownNumbers()
{
    const double flat = steadySpeedKph(250, 75, 0.0);
    QVERIFY2(flat > 37.0 && flat < 42.0, qPrintable(QString::number(flat)));
    const double climb = steadySpeedKph(250, 75, 0.08);
    QVERIFY2(climb > 12.5 && climb < 15.5, qPrintable(QString::number(climb)));
    const double heavier = steadySpeedKph(250, 90, 0.08);
    QVERIFY(heavier < climb);
    QVERIFY(steadySpeedKph(250, 75, 0.0, 0.30) > flat + 3.0);
}

void TstRace::drafting_fadesWithGapAndIsCapped()
{
    QCOMPARE(RacePhysics::draftReduction({}), 0.0);
    QCOMPARE(RacePhysics::draftReduction({0.8}), 0.30);
    QCOMPARE(RacePhysics::draftReduction({1.0}), 0.30);
    QVERIFY(std::abs(RacePhysics::draftReduction({5.5}) - 0.15) < 1e-9);
    QCOMPARE(RacePhysics::draftReduction({10.0}), 0.0);
    QCOMPARE(RacePhysics::draftReduction({12.0}), 0.0);
    QVERIFY(std::abs(RacePhysics::draftReduction({1.0, 3.0}) - 0.33) < 1e-9);
    QCOMPARE(RacePhysics::draftReduction({1, 2, 3, 4, 5, 6, 7, 8}), RacePhysics::kMaxDraftReduction);
}

void TstRace::drafting_savesMoreOnTheFlatThanOnAClimb()
{
    const double onFlat = RacePhysics::powerSaving(11.0, 75, 0.30, 0.0, 0.30);
    const double onClimb = RacePhysics::powerSaving(3.9, 75, 0.30, 0.08, 0.30);
    QVERIFY2(onFlat > 0.25 && onFlat < 0.30, qPrintable(QString::number(onFlat)));
    QVERIFY2(onClimb < 0.03, qPrintable(QString::number(onClimb)));
    for (double grade = -0.10; grade <= 0.12; grade += 0.01)
        for (double speed = 1.0; speed <= 20.0; speed += 1.0)
            QVERIFY(RacePhysics::powerSaving(speed, 75, 0.30, grade, 0.30) <= 0.30 + 1e-12);
}

void TstRace::defaultField_hasAPaceGroupAndASpread()
{
    const QVector<RaceBot> field = RaceBot::defaultField(7);
    QCOMPARE(field.size(), 20);
    int paceGroup = 0;
    double lowest = 10.0;
    double highest = 0.0;
    for (const RaceBot &bot : field) {
        if (bot.name().startsWith(QStringLiteral("Pace"))) {
            ++paceGroup;
            QCOMPARE(bot.wattsPerKg(), 3.0);
        }
        lowest = std::min(lowest, bot.wattsPerKg());
        highest = std::max(highest, bot.wattsPerKg());
        QVERIFY(bot.massKg() >= 62.0 && bot.massKg() <= 82.0);
    }
    QCOMPARE(paceGroup, 6);
    QCOMPARE(lowest, 2.2);
    QCOMPARE(highest, 4.0);
}

void TstRace::customField_followsCountAndRange()
{
    const QVector<RaceBot> forty = RaceBot::field({40, 1.5, 5.0}, 1);
    QCOMPARE(forty.size(), 40);
    QCOMPARE(std::count_if(forty.cbegin(), forty.cend(), [](const RaceBot &bot) {
        return bot.name().startsWith(QStringLiteral("Pace")); }), 12);
    QSet<QString> names;
    for (const RaceBot &bot : forty) {
        QVERIFY(bot.wattsPerKg() >= 1.5 && bot.wattsPerKg() <= 5.0);
        names.insert(bot.name());
    }
    QCOMPARE(names.size(), 40);   // every bot is told apart in the standings

    // A range above 3.0 W/kg moves the pace group to the bottom of it.
    const QVector<RaceBot> strong = RaceBot::field({10, 3.5, 4.5}, 2);
    QCOMPARE(strong.constFirst().wattsPerKg(), 3.5);

    // Reversed limits are read the right way round; tiny fields have no pace group.
    for (const RaceBot &bot : RaceBot::field({2, 4.0, 2.0}, 3)) {
        QVERIFY(!bot.name().startsWith(QStringLiteral("Pace")));
        QVERIFY(bot.wattsPerKg() >= 2.0 && bot.wattsPerKg() <= 4.0);
    }
    QCOMPARE(RaceBot::field({0, 2.0, 4.0}, 4).size(), 0);
    QCOMPARE(RaceBot::field({500, 2.0, 4.0}, 4).size(), RaceBot::kMaxBots);
}

void TstRace::bots_atTheSameWattsPerKgRideTogether()
{
    RaceSimulation race(loadCourse(QStringLiteral("longchamp")), QStringLiteral("You"), 75, 0, 0.30);
    for (const RaceBot &bot : RaceBot::defaultField(11))
        if (bot.name().startsWith(QStringLiteral("Pace")))
            race.addBot(bot);
    race.setYourPower(0.0);
    for (int tick = 0; tick < 10 * 60 * 4; ++tick)
        race.step(0.25);

    double front = 0.0;
    double back = 1e9;
    for (const RaceSimulation::Rider &rider : race.riders()) {
        if (rider.isYou)
            continue;
        front = std::max(front, rider.distanceM);
        back = std::min(back, rider.distanceM);
    }
    QVERIFY2(front - back < 150.0, qPrintable(QStringLiteral("pace group spread %1 m").arg(front - back)));
    QVERIFY(back > 5000.0);   // ten minutes at ~35 km/h
}

void TstRace::youAtThreeWattsPerKg_canFollowThePaceGroupAndDraft()
{
    constexpr double kYourMassKg = 75.0;
    RaceSimulation race(loadCourse(QStringLiteral("longchamp")), QStringLiteral("You"), kYourMassKg, 0, 0.30);
    for (const RaceBot &bot : RaceBot::defaultField(3))
        if (bot.name().startsWith(QStringLiteral("Pace")))
            race.addBot(bot);

    int ticksDrafting = 0;
    double bestSaving = 0.0;
    for (int tick = 0; tick < 10 * 60 * 4; ++tick) {
        race.setYourPower(3.0 * kYourMassKg);
        race.step(0.25);
        if (race.you().draftReduction > 0.0) {
            ++ticksDrafting;
            bestSaving = std::max(bestSaving, race.you().powerSaving);
        }
    }
    QVERIFY2(ticksDrafting > 0, "never drafted");
    QVERIFY2(bestSaving > 0.10, qPrintable(QString::number(bestSaving)));
    QVERIFY(race.you().draftingBehind != 0);

    double groupBack = 1e9;
    for (int index = 1; index < race.riders().size(); ++index)
        groupBack = std::min(groupBack, race.riders().at(index).distanceM);
    QVERIFY2(race.you().distanceM > groupBack - 100.0,
             qPrintable(QStringLiteral("dropped: %1 m behind the group")
                            .arg(groupBack - race.you().distanceM)));
}

void TstRace::youAtThreeWattsPerKg_climbWithThePaceGroup()
{
    constexpr double kRiderKg = 72.0;
    RaceSimulation race(loadCourse(QStringLiteral("alpe-dhuez")), QStringLiteral("You"), kRiderKg, RaceSimulation::kBotBikeMassKg, 0.30);
    for (const RaceBot &bot : RaceBot::defaultField(5))
        if (bot.name().startsWith(QStringLiteral("Pace")))
            race.addBot(bot);
    while (!race.youFinished() && race.elapsedSec() < 3 * 3600) {
        race.setYourPower(3.0 * kRiderKg);
        race.step(0.5);
    }
    QVERIFY(race.youFinished());
    for (int index = 1; index < race.riders().size(); ++index) {
        const double gap = std::abs(race.gapToYouSec(index));
        QVERIFY2(gap < 120.0, qPrintable(QStringLiteral("%1 finished %2 s from you")
                                             .arg(race.riders().at(index).name).arg(gap)));
    }
}

void TstRace::race_finishersAreRankedByTime()
{
    RaceSimulation race(loadCourse(QStringLiteral("alpe-dhuez")), QStringLiteral("You"), 70, 0, 0.30);
    race.addBot(RaceBot(QStringLiteral("Slow"), 2.2, 70, 1));
    race.addBot(RaceBot(QStringLiteral("Fast"), 4.0, 70, 2));
    race.setYourPower(3.0 * 70);
    for (int tick = 0; tick < 3 * 3600 * 2 && !race.riders().at(1).finishTimeSec; ++tick)
        race.step(0.5);

    QVERIFY(race.youFinished());
    const QVector<int> order = race.standings();
    QCOMPARE(race.riders().at(order.at(0)).name, QStringLiteral("Fast"));
    QCOMPARE(race.riders().at(order.at(1)).name, QStringLiteral("You"));
    QCOMPARE(race.riders().at(order.at(2)).name, QStringLiteral("Slow"));
    QCOMPARE(race.yourPosition(), 2);
    QVERIFY(race.gapToYouSec(order.at(0)) < 0.0);
    QVERIFY(race.gapToYouSec(order.at(2)) > 0.0);

    const double minutes = *race.you().finishTimeSec / 60.0;
    QVERIFY2(minutes > 50.0 && minutes < 90.0, qPrintable(QString::number(minutes)));
}

QTEST_GUILESS_MAIN(TstRace)
#include "tst_race.moc"
