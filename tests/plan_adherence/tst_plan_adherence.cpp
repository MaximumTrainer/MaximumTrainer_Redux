/*
 * tst_plan_adherence.cpp
 *
 * Qt Test suite for PlanAdherenceStore.
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────
 * addCompleted   – basic record, upsert behaviour, FIT file path stored
 * addSkipped     – skipped record, note stored
 * addSubstituted – substituted record, note stored
 * remove         – remove an entry
 * adherencePct   – percentage calculation (Completed vs Skipped/Substituted)
 * adherencePctRecent – rolling N-day window
 * counts         – totalCount / completedCount / skippedCount / substitutedCount
 * encode/decode  – round-trip serialisation (pipe-escaped special chars)
 * storeChanged   – signal emitted on mutations
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QDate>

#include "planadherencestore.h"
#include "planadherence.h"

class TstPlanAdherence : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // ── addCompleted ──────────────────────────────────────────────────────────
    void testAddCompleted_basic();
    void testAddCompleted_upsertStatus();
    void testAddCompleted_fitFilePath();

    // ── addSkipped ────────────────────────────────────────────────────────────
    void testAddSkipped_basic();
    void testAddSkipped_noteStored();

    // ── addSubstituted ────────────────────────────────────────────────────────
    void testAddSubstituted_basic();

    // ── remove ────────────────────────────────────────────────────────────────
    void testRemove_existing();
    void testRemove_nonExisting_noError();

    // ── adherencePct ──────────────────────────────────────────────────────────
    void testAdherencePct_allCompleted();
    void testAdherencePct_allSkipped();
    void testAdherencePct_mixed();
    void testAdherencePct_noEntries();

    // ── adherencePctRecent ────────────────────────────────────────────────────
    void testAdherencePctRecent_onlyRecentCounted();

    // ── counts ────────────────────────────────────────────────────────────────
    void testCounts();

    // ── encode / decode round-trip ────────────────────────────────────────────
    void testEncodeDecodeRoundTrip_normal();
    void testEncodeDecodeRoundTrip_specialChars();

    // ── storeChanged signal ───────────────────────────────────────────────────
    void testStoreChanged_emittedOnAdd();
    void testStoreChanged_emittedOnRemove();

private:
    PlanAdherenceStore *store = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
void TstPlanAdherence::init()
{
    store = new PlanAdherenceStore();
    // Use a unique QSettings scope per test via INI file to avoid cross-test contamination
    // (PlanAdherenceStore uses QSettings with default scope)
    // We rely on cleanup() to clear it after each test.
}

void TstPlanAdherence::cleanup()
{
    // Clear all stored entries
    const auto es = store->entries();
    for (const PlanAdherenceEntry &e : es)
        store->remove(e.date, e.workoutName);
    delete store;
    store = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// addCompleted tests
// ─────────────────────────────────────────────────────────────────────────────

void TstPlanAdherence::testAddCompleted_basic()
{
    const QDate d(2024, 6, 1);
    store->addCompleted(d, QStringLiteral("Sweet Spot 60min"));

    QCOMPARE(store->entries().size(), 1);
    const PlanAdherenceEntry &e = store->entries().first();
    QCOMPARE(e.date,         d);
    QCOMPARE(e.workoutName,  QStringLiteral("Sweet Spot 60min"));
    QCOMPARE(e.status,       PlanAdherenceEntry::Completed);
}

void TstPlanAdherence::testAddCompleted_upsertStatus()
{
    // If the same date+name already exists (e.g. previously Skipped), addCompleted upgrades it
    const QDate d(2024, 6, 2);
    store->addSkipped(d, QStringLiteral("Threshold 45min"), QStringLiteral("felt sick"));
    QCOMPARE(store->entries().first().status, PlanAdherenceEntry::Skipped);

    store->addCompleted(d, QStringLiteral("Threshold 45min"), QStringLiteral("/history/2024-06-02.fit"));
    QCOMPARE(store->entries().size(), 1);  // upserted, not duplicated
    QCOMPARE(store->entries().first().status,      PlanAdherenceEntry::Completed);
    QCOMPARE(store->entries().first().fitFilePath, QStringLiteral("/history/2024-06-02.fit"));
}

void TstPlanAdherence::testAddCompleted_fitFilePath()
{
    const QDate d(2024, 6, 3);
    store->addCompleted(d, QStringLiteral("VO2max 5x5"), QStringLiteral("/data/2024-06-03.fit"));

    QCOMPARE(store->entries().first().fitFilePath, QStringLiteral("/data/2024-06-03.fit"));
}

// ─────────────────────────────────────────────────────────────────────────────
// addSkipped / addSubstituted tests
// ─────────────────────────────────────────────────────────────────────────────

void TstPlanAdherence::testAddSkipped_basic()
{
    const QDate d(2024, 6, 4);
    store->addSkipped(d, QStringLiteral("Endurance 2h"));

    QCOMPARE(store->entries().size(), 1);
    QCOMPARE(store->entries().first().status, PlanAdherenceEntry::Skipped);
}

void TstPlanAdherence::testAddSkipped_noteStored()
{
    const QDate d(2024, 6, 5);
    store->addSkipped(d, QStringLiteral("Tempo 60min"), QStringLiteral("travel day"));
    QCOMPARE(store->entries().first().note, QStringLiteral("travel day"));
}

void TstPlanAdherence::testAddSubstituted_basic()
{
    const QDate d(2024, 6, 6);
    store->addSubstituted(d, QStringLiteral("Intervals 8x3"), QStringLiteral("Did Zwift race instead"));

    QCOMPARE(store->entries().size(), 1);
    QCOMPARE(store->entries().first().status, PlanAdherenceEntry::Substituted);
    QCOMPARE(store->entries().first().note,   QStringLiteral("Did Zwift race instead"));
}

// ─────────────────────────────────────────────────────────────────────────────
// remove tests
// ─────────────────────────────────────────────────────────────────────────────

void TstPlanAdherence::testRemove_existing()
{
    const QDate d(2024, 6, 7);
    store->addCompleted(d, QStringLiteral("Recovery 30min"));
    QCOMPARE(store->entries().size(), 1);

    store->remove(d, QStringLiteral("Recovery 30min"));
    QCOMPARE(store->entries().size(), 0);
}

void TstPlanAdherence::testRemove_nonExisting_noError()
{
    // removing an entry that doesn't exist must not crash or change state
    store->remove(QDate(2024, 1, 1), QStringLiteral("ghost workout"));
    QCOMPARE(store->entries().size(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// adherencePct tests
// ─────────────────────────────────────────────────────────────────────────────

void TstPlanAdherence::testAdherencePct_allCompleted()
{
    const QDate base(2024, 7, 1);
    for (int i = 0; i < 5; ++i)
        store->addCompleted(base.addDays(i), QStringLiteral("W%1").arg(i));

    const double pct = store->adherencePct(base, base.addDays(4));
    QCOMPARE(pct, 100.0);
}

void TstPlanAdherence::testAdherencePct_allSkipped()
{
    const QDate base(2024, 7, 10);
    for (int i = 0; i < 3; ++i)
        store->addSkipped(base.addDays(i), QStringLiteral("W%1").arg(i));

    const double pct = store->adherencePct(base, base.addDays(2));
    QCOMPARE(pct, 0.0);
}

void TstPlanAdherence::testAdherencePct_mixed()
{
    // 3 completed, 1 skipped, 1 substituted → 3/5 = 60%
    const QDate base(2024, 7, 20);
    store->addCompleted(   base,            QStringLiteral("W1"));
    store->addCompleted(   base.addDays(1), QStringLiteral("W2"));
    store->addCompleted(   base.addDays(2), QStringLiteral("W3"));
    store->addSkipped(     base.addDays(3), QStringLiteral("W4"));
    store->addSubstituted( base.addDays(4), QStringLiteral("W5"));

    const double pct = store->adherencePct(base, base.addDays(4));
    QCOMPARE(pct, 60.0);
}

void TstPlanAdherence::testAdherencePct_noEntries()
{
    const QDate base(2024, 8, 1);
    const double pct = store->adherencePct(base, base.addDays(7));
    QCOMPARE(pct, 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// adherencePctRecent
// ─────────────────────────────────────────────────────────────────────────────

void TstPlanAdherence::testAdherencePctRecent_onlyRecentCounted()
{
    // Add one very old completed entry (should be excluded from 7-day window)
    // and two recent entries (1 completed, 1 skipped)
    const QDate old    = QDate::currentDate().addDays(-100);
    const QDate recent = QDate::currentDate().addDays(-1);
    const QDate today  = QDate::currentDate();

    store->addCompleted(old,    QStringLiteral("OldWorkout"));
    store->addCompleted(recent, QStringLiteral("RecentCompleted"));
    store->addSkipped(  today,  QStringLiteral("TodaySkipped"));

    const double pct7d = store->adherencePctRecent(7);
    // Expect 1 completed / 2 recent = 50%
    QCOMPARE(pct7d, 50.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// counts
// ─────────────────────────────────────────────────────────────────────────────

void TstPlanAdherence::testCounts()
{
    const QDate base(2024, 9, 1);
    store->addCompleted(   base,            QStringLiteral("A"));
    store->addCompleted(   base.addDays(1), QStringLiteral("B"));
    store->addSkipped(     base.addDays(2), QStringLiteral("C"));
    store->addSubstituted( base.addDays(3), QStringLiteral("D"));

    QCOMPARE(store->totalCount(),       4);
    QCOMPARE(store->completedCount(),   2);
    QCOMPARE(store->skippedCount(),     1);
    QCOMPARE(store->substitutedCount(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// encode / decode round-trip
// ─────────────────────────────────────────────────────────────────────────────

void TstPlanAdherence::testEncodeDecodeRoundTrip_normal()
{
    const QDate d(2024, 10, 15);
    store->addCompleted(d, QStringLiteral("Morning Ride"), QStringLiteral("/home/user/history/ride.fit"));
    store->save();

    PlanAdherenceStore store2;
    store2.load();

    bool found = false;
    for (const PlanAdherenceEntry &e : store2.entries()) {
        if (e.date == d && e.workoutName == QStringLiteral("Morning Ride")) {
            QCOMPARE(e.status,      PlanAdherenceEntry::Completed);
            QCOMPARE(e.fitFilePath, QStringLiteral("/home/user/history/ride.fit"));
            found = true;
        }
    }
    QVERIFY2(found, "Entry not found after save/load round-trip");

    // cleanup store2
    const auto es = store2.entries();
    for (const PlanAdherenceEntry &e : es)
        store2.remove(e.date, e.workoutName);
}

void TstPlanAdherence::testEncodeDecodeRoundTrip_specialChars()
{
    // Workout name contains pipe character (must be escaped) and note contains newline
    const QDate d(2024, 10, 16);
    const QString nameWithPipe = QStringLiteral("Speed|Power Test");
    const QString noteWithNewline = QStringLiteral("legs felt heavy\ntook a break");

    store->addSkipped(d, nameWithPipe, noteWithNewline);
    store->save();

    PlanAdherenceStore store2;
    store2.load();

    bool found = false;
    for (const PlanAdherenceEntry &e : store2.entries()) {
        if (e.date == d) {
            QCOMPARE(e.workoutName, nameWithPipe);
            QCOMPARE(e.note,        noteWithNewline);
            found = true;
        }
    }
    QVERIFY2(found, "Special-char entry not found after save/load");

    const auto es = store2.entries();
    for (const PlanAdherenceEntry &e : es)
        store2.remove(e.date, e.workoutName);
}

// ─────────────────────────────────────────────────────────────────────────────
// storeChanged signal
// ─────────────────────────────────────────────────────────────────────────────

void TstPlanAdherence::testStoreChanged_emittedOnAdd()
{
    QSignalSpy spy(store, &PlanAdherenceStore::storeChanged);
    store->addCompleted(QDate::currentDate(), QStringLiteral("SignalTest"));
    QCOMPARE(spy.count(), 1);
}

void TstPlanAdherence::testStoreChanged_emittedOnRemove()
{
    const QDate d = QDate::currentDate();
    store->addCompleted(d, QStringLiteral("RemoveSignalTest"));

    QSignalSpy spy(store, &PlanAdherenceStore::storeChanged);
    store->remove(d, QStringLiteral("RemoveSignalTest"));
    QCOMPARE(spy.count(), 1);
}

// ─────────────────────────────────────────────────────────────────────────────
QTEST_MAIN(TstPlanAdherence)
#include "tst_plan_adherence.moc"
