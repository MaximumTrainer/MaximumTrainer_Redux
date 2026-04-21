#ifndef PLANADHERENCESTORE_H
#define PLANADHERENCESTORE_H

#include <QObject>
#include <QList>
#include <QDate>
#include "planadherence.h"

/// Persists plan-adherence records to QSettings.
///
/// Records are stored under QSettings group "planAdherence" as a
/// JSON-like array encoded in a single string value.  Each record is
/// stored as "ISODate|workoutName|status|note|fitFilePath" entries
/// separated by newlines, making the storage human-readable and safe
/// for all platforms.
///
/// Auto-complete flow:
///   MainWindow::checkToUploadFile() → PlanAdherenceStore::addCompleted()
///
/// Manual flows (from PlanAdherenceWidget context menu):
///   addSkipped() / addSubstituted()
class PlanAdherenceStore : public QObject
{
    Q_OBJECT
public:
    explicit PlanAdherenceStore(QObject *parent = nullptr);

    /// Record a completed workout from a saved FIT file.
    /// If an entry with the same date+name already exists, it is updated.
    void addCompleted(const QDate &date, const QString &workoutName,
                      const QString &fitFilePath = QString());

    /// Manually mark a planned session as skipped.
    void addSkipped(const QDate &date, const QString &workoutName,
                    const QString &note = QString());

    /// Manually mark a session as performed with a substitute workout.
    void addSubstituted(const QDate &date, const QString &workoutName,
                        const QString &note = QString());

    /// Remove entry at the given date + workout name.
    void remove(const QDate &date, const QString &workoutName);

    /// All entries, newest-date first.
    QList<PlanAdherenceEntry> entries() const;

    /// Adherence percentage for entries within the given date range.
    /// Counts Completed as 1, Skipped/Substituted as 0.
    /// Returns 0 if no entries exist in range.
    double adherencePct(const QDate &from, const QDate &to) const;

    /// Convenience: adherence over the last \a days calendar days.
    double adherencePctRecent(int days = 30) const;

    int totalCount()     const;
    int completedCount() const;
    int skippedCount()   const;
    int substitutedCount() const;

    void save() const;
    void load();

signals:
    void storeChanged();

private:
    QList<PlanAdherenceEntry> m_entries;

    void upsert(const PlanAdherenceEntry &e);
    static QString encodeEntry(const PlanAdherenceEntry &e);
    static PlanAdherenceEntry decodeEntry(const QString &line);
};

#endif // PLANADHERENCESTORE_H
