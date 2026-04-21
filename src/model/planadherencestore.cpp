#include "planadherencestore.h"

#include <QSettings>
#include <algorithm>

static const char kGroup[]  = "planAdherence";
static const char kKey[]    = "entries";
static const char kSep      = '|';

PlanAdherenceStore::PlanAdherenceStore(QObject *parent) : QObject(parent)
{
    load();
}

// ── Public API ──────────────────────────────────────────────────────────────

void PlanAdherenceStore::addCompleted(const QDate &date,
                                      const QString &workoutName,
                                      const QString &fitFilePath)
{
    PlanAdherenceEntry e;
    e.date        = date;
    e.workoutName = workoutName;
    e.status      = PlanAdherenceEntry::Completed;
    e.fitFilePath = fitFilePath;
    upsert(e);
}

void PlanAdherenceStore::addSkipped(const QDate &date,
                                    const QString &workoutName,
                                    const QString &note)
{
    PlanAdherenceEntry e;
    e.date        = date;
    e.workoutName = workoutName;
    e.status      = PlanAdherenceEntry::Skipped;
    e.note        = note;
    upsert(e);
}

void PlanAdherenceStore::addSubstituted(const QDate &date,
                                        const QString &workoutName,
                                        const QString &note)
{
    PlanAdherenceEntry e;
    e.date        = date;
    e.workoutName = workoutName;
    e.status      = PlanAdherenceEntry::Substituted;
    e.note        = note;
    upsert(e);
}

void PlanAdherenceStore::remove(const QDate &date, const QString &workoutName)
{
    const QString name = workoutName.trimmed().toLower();
    auto it = std::remove_if(m_entries.begin(), m_entries.end(),
        [&](const PlanAdherenceEntry &e) {
            return e.date == date && e.workoutName.trimmed().toLower() == name;
        });
    if (it != m_entries.end()) {
        m_entries.erase(it, m_entries.end());
        save();
        emit storeChanged();
    }
}

QList<PlanAdherenceEntry> PlanAdherenceStore::entries() const
{
    QList<PlanAdherenceEntry> sorted = m_entries;
    std::sort(sorted.begin(), sorted.end(),
        [](const PlanAdherenceEntry &a, const PlanAdherenceEntry &b) {
            return a.date > b.date;
        });
    return sorted;
}

double PlanAdherenceStore::adherencePct(const QDate &from, const QDate &to) const
{
    int total = 0, completed = 0;
    for (const auto &e : m_entries) {
        if (e.date >= from && e.date <= to) {
            ++total;
            if (e.status == PlanAdherenceEntry::Completed) ++completed;
        }
    }
    return total == 0 ? 0.0 : 100.0 * completed / total;
}

double PlanAdherenceStore::adherencePctRecent(int days) const
{
    const QDate to   = QDate::currentDate();
    const QDate from = to.addDays(-days + 1);
    return adherencePct(from, to);
}

int PlanAdherenceStore::totalCount() const
{
    return m_entries.size();
}

int PlanAdherenceStore::completedCount() const
{
    int n = 0;
    for (const auto &e : m_entries)
        if (e.status == PlanAdherenceEntry::Completed) ++n;
    return n;
}

int PlanAdherenceStore::skippedCount() const
{
    int n = 0;
    for (const auto &e : m_entries)
        if (e.status == PlanAdherenceEntry::Skipped) ++n;
    return n;
}

int PlanAdherenceStore::substitutedCount() const
{
    int n = 0;
    for (const auto &e : m_entries)
        if (e.status == PlanAdherenceEntry::Substituted) ++n;
    return n;
}

// ── Persistence ─────────────────────────────────────────────────────────────

void PlanAdherenceStore::save() const
{
    QStringList lines;
    for (const auto &e : m_entries)
        lines.append(encodeEntry(e));

    QSettings s;
    s.beginGroup(kGroup);
    s.setValue(kKey, lines.join('\n'));
    s.endGroup();
}

void PlanAdherenceStore::load()
{
    QSettings s;
    s.beginGroup(kGroup);
    const QString raw = s.value(kKey).toString();
    s.endGroup();

    m_entries.clear();
    if (raw.isEmpty()) return;

    const QStringList lines = raw.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        PlanAdherenceEntry e = decodeEntry(line);
        if (e.isValid())
            m_entries.append(e);
    }
}

// ── Private helpers ──────────────────────────────────────────────────────────

void PlanAdherenceStore::upsert(const PlanAdherenceEntry &e)
{
    const QString name = e.workoutName.trimmed().toLower();
    for (auto &existing : m_entries) {
        if (existing.date == e.date && existing.workoutName.trimmed().toLower() == name) {
            existing = e;
            save();
            emit storeChanged();
            return;
        }
    }
    m_entries.append(e);
    save();
    emit storeChanged();
}

QString PlanAdherenceStore::encodeEntry(const PlanAdherenceEntry &e)
{
    // Format: date|workoutName|status|note|fitFilePath
    // Backslashes are escaped first (\→\\), then pipes (|→\|) and newlines (\n→\n).
    auto escape = [](const QString &s) {
        return QString(s)
            .replace(QLatin1Char('\\'), QLatin1String("\\\\"))
            .replace(QLatin1Char('|'),  QLatin1String("\\|"))
            .replace(QLatin1Char('\n'), QLatin1String("\\n"));
    };
    return QStringList{
        e.date.toString(Qt::ISODate),
        escape(e.workoutName),
        QString::number(static_cast<int>(e.status)),
        escape(e.note),
        escape(e.fitFilePath)
    }.join(kSep);
}

PlanAdherenceEntry PlanAdherenceStore::decodeEntry(const QString &line)
{
    // Split on unescaped |, handling \\ → \, \| → |, \n → newline
    QStringList parts;
    QString cur;
    for (int i = 0; i < line.size(); ++i) {
        if (line[i] == QLatin1Char('\\') && i + 1 < line.size()) {
            const QChar next = line[i + 1];
            if (next == QLatin1Char('\\')) { cur += QLatin1Char('\\'); ++i; }
            else if (next == QLatin1Char('|')) { cur += QLatin1Char('|'); ++i; }
            else if (next == QLatin1Char('n')) { cur += QLatin1Char('\n'); ++i; }
            else { cur += line[i]; }   // unknown escape — keep the backslash
        } else if (line[i] == QLatin1Char('|')) {
            parts.append(cur);
            cur.clear();
        } else {
            cur += line[i];
        }
    }
    parts.append(cur);

    if (parts.size() < 3) return {};

    bool ok = false;
    const int statusInt = parts.at(2).toInt(&ok);
    if (!ok || statusInt < 0 || statusInt > static_cast<int>(PlanAdherenceEntry::Substituted))
        return {};  // Out-of-range or non-numeric status — treat as decode failure

    PlanAdherenceEntry e;
    e.date        = QDate::fromString(parts.at(0), Qt::ISODate);
    e.workoutName = parts.at(1);
    e.status      = static_cast<PlanAdherenceEntry::Status>(statusInt);
    e.note        = parts.size() > 3 ? parts.at(3) : QString();
    e.fitFilePath = parts.size() > 4 ? parts.at(4) : QString();
    return e;
}
