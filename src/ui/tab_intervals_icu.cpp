#include "tab_intervals_icu.h"

#include <utility>
#include "ui_tab_intervals_icu.h"

namespace {
// Age after which the week is fetched again when the tab is shown, or when
// the app comes back to the front (usually after editing the calendar).
constexpr qint64 kShowRefreshSeconds = 300;
constexpr qint64 kActivateRefreshSeconds = 30;
} // namespace

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QMessageBox>
#include <QDebug>
#include <QApplication>
#include <QLabel>
#include <QLocale>
#include <QTimer>

#include "account.h"
#include "util.h"
#include "importerworkoutzwo.h"
#include "logger.h"
#include "xmlutil.h"

// ─────────────────────────────────────────────────────────────────────────────
// WASM test hooks: allow Playwright to inject credentials and trigger a
// calendar refresh without touching the real UI or QSettings.
//
// mt_setIntervalsCredentials(apiKey, athleteId) — writes the credentials
//   directly into the Account object on qApp and calls refreshCredentials()
//   so the service also picks them up.  This bypasses QSettings/localStorage
//   format concerns and works regardless of browser-storage backend.
//
// mt_intervalsRefresh() — queues an onRefreshClicked() call on the Qt event
//   loop.  The test must wait for the next event-loop tick (e.g., 5 s) for
//   the request to be dispatched.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef GC_WASM_BUILD
#include <emscripten.h>
#include <QPointer>

static QPointer<TabIntervalsIcu> g_intervalsTab;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void mt_intervals_icu_do_refresh()
{
    if (g_intervalsTab)
        QMetaObject::invokeMethod(g_intervalsTab.data(), "onRefreshClicked",
                                  Qt::QueuedConnection);
}

// Sets credentials directly on the Account object and propagates them to the
// service layer.  Called from JS via Module._mt_intervals_icu_set_credentials()
// with heap-allocated UTF-8 strings produced by the shim below.
EMSCRIPTEN_KEEPALIVE
void mt_intervals_icu_set_credentials(const char *apiKey, const char *athleteId)
{
    auto *account = qApp->property("Account").value<Account *>();
    if (account) {
        account->intervals_icu_api_key    = QString::fromUtf8(apiKey);
        account->intervals_icu_athlete_id = QString::fromUtf8(athleteId);
    }
    if (g_intervalsTab)
        g_intervalsTab->refreshCredentials();
}

} // extern "C"

EM_JS(void, js_exposeIntervalsTestApi, (), {
    window.mt_intervalsRefresh = function() {
        Module._mt_intervals_icu_do_refresh();
    };
    // Inject credentials directly into the running Account object, bypassing
    // QSettings so Playwright tests don't depend on the localStorage key format.
    //
    // Uses TextEncoder (browser-native) + Module.HEAPU8 / _malloc / _free so
    // this does not depend on any optional Emscripten EXPORTED_RUNTIME_METHODS.
    window.mt_setIntervalsCredentials = function(apiKey, athleteId) {
        var enc    = new TextEncoder();
        var keyBuf = enc.encode(String(apiKey   || "") + '\0');
        var idBuf  = enc.encode(String(athleteId || "") + '\0');
        var keyPtr = _malloc(keyBuf.length);
        var idPtr  = _malloc(idBuf.length);
        HEAPU8.set(keyBuf, keyPtr);
        HEAPU8.set(idBuf,  idPtr);
        Module._mt_intervals_icu_set_credentials(keyPtr, idPtr);
        _free(keyPtr);
        _free(idPtr);
    };
});
#endif

// ─────────────────────────────────────────────────────────────────────────────
TabIntervalsIcu::TabIntervalsIcu(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabIntervalsIcu)
    , m_service(new IntervalsIcuService(this))
    , m_weekStart(QDate::currentDate())
{
    ui->setupUi(this);

    // Align week start to Monday
    const int dow = m_weekStart.dayOfWeek(); // 1=Mon … 7=Sun
    m_weekStart = m_weekStart.addDays(1 - dow);
    updateWeekLabel();

    // Stretch the "Name" column to fill available space
    ui->tableWidget_calendar->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);

#ifdef GC_WASM_BUILD
    g_intervalsTab = this;
    js_exposeIntervalsTestApi();
#endif
    // A small reload icon with the schedule's age, instead of a Refresh button:
    // the week loads by itself.
    ui->pushButton_refresh->setText(QString());
    ui->pushButton_refresh->setFlat(true);
    ui->pushButton_refresh->setToolTip(tr("Reload this week from Intervals.icu"));
    m_updatedLabel = new QLabel(this);
    m_updatedLabel->setStyleSheet(QStringLiteral("color: #8d8d8d;"));
    ui->horizontalLayout_nav->insertWidget(ui->horizontalLayout_nav->indexOf(ui->pushButton_refresh), m_updatedLabel);
    auto *freshnessTimer = new QTimer(this);
    freshnessTimer->setInterval(30000);
    connect(freshnessTimer, &QTimer::timeout, this, &TabIntervalsIcu::updateFreshnessLabel);
    freshnessTimer->start();
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            autoRefresh(kActivateRefreshSeconds);
    });
    connect(ui->pushButton_refresh,  &QPushButton::clicked,
            this, &TabIntervalsIcu::onRefreshClicked);
    connect(ui->pushButton_prevWeek, &QPushButton::clicked,
            this, &TabIntervalsIcu::onPrevWeekClicked);
    connect(ui->pushButton_nextWeek, &QPushButton::clicked,
            this, &TabIntervalsIcu::onNextWeekClicked);
    connect(ui->pushButton_loadWorkout, &QPushButton::clicked,
            this, &TabIntervalsIcu::onLoadWorkoutClicked);
    connect(ui->pushButton_syncAll, &QPushButton::clicked,
            this, &TabIntervalsIcu::onSyncAllClicked);
    connect(ui->tableWidget_calendar,
            &QTableWidget::itemSelectionChanged, this, [this]() {
                const int row =
                    ui->tableWidget_calendar->currentRow();
                const bool hasWorkout =
                    row >= 0 && row < m_rowEventIds.size() &&
                    !m_rowEventIds[row].isEmpty();
                ui->pushButton_loadWorkout->setEnabled(hasWorkout);
            });
}

// ─────────────────────────────────────────────────────────────────────────────
TabIntervalsIcu::~TabIntervalsIcu()
{
    delete ui;
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::refreshCredentials()
{
    auto *account = qApp->property("Account").value<Account *>();
    if (!account) {
        LOG_WARN("TabIntervalsIcu", QStringLiteral("refreshCredentials: Account property not available"));
        return;
    }

    m_service->setCredentials(account->intervals_icu_api_key,
                               account->intervals_icu_athlete_id);
    m_service->setAccessToken(account->intervals_icu_access_token);

    const bool hasCredentials = !account->intervals_icu_athlete_id.isEmpty() &&
        (!account->intervals_icu_api_key.isEmpty() ||
         !account->intervals_icu_access_token.isEmpty());

    if (!m_networkOnline) {
        setCalendarAvailable(false, tr("Intervals.icu sync is not available while offline. "
                                        "Please check your internet connection."));
        return;
    }

    if (!hasCredentials) {
        // Clear any stale calendar data so the old content isn't visible.
        ui->tableWidget_calendar->setRowCount(0);
        m_rowEventIds.clear();
        ui->pushButton_loadWorkout->setEnabled(false);
        if (m_downloadReply)
            ui->tableWidget_calendar->setSelectionMode(QAbstractItemView::SingleSelection);
        abortCalendarRequests();
        setBusy(false);
        setCalendarAvailable(false, tr("Sign in with Intervals.icu to see your training calendar here. "
                                       "Choose MaximumTrainer → Log Out to return to the sign-in screen."));
        return;
    }

    setCalendarAvailable(true);
    setStatus(tr("Ready."));
    autoRefresh(kShowRefreshSeconds);
}

void TabIntervalsIcu::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    autoRefresh(kShowRefreshSeconds);
}

void TabIntervalsIcu::autoRefresh(qint64 maxAgeSeconds)
{
    if (!isVisible() || !m_calendarAvailable)
        return;
    if (m_calendarReply || m_downloadReply || m_syncCalendarReply || m_syncDownloadReply)
        return;
    if (maxAgeSeconds > 0 && m_loadedWeek == m_weekStart && m_loadedAt.isValid()
        && m_loadedAt.secsTo(QDateTime::currentDateTime()) < maxAgeSeconds)
        return;
    onRefreshClicked();
}

void TabIntervalsIcu::updateFreshnessLabel()
{
    if (m_calendarReply) {
        m_updatedLabel->setText(tr("Updating..."));
        return;
    }
    if (!m_loadedAt.isValid() || m_loadedWeek != m_weekStart) {
        m_updatedLabel->clear();
        return;
    }
    const qint64 seconds = m_loadedAt.secsTo(QDateTime::currentDateTime());
    if (seconds < 60)
        m_updatedLabel->setText(tr("Updated just now"));
    else if (seconds < 3600)
        m_updatedLabel->setText(tr("Updated %n min ago", "", static_cast<int>(seconds / 60)));
    else
        m_updatedLabel->setText(tr("Updated at %1").arg(QLocale().toString(m_loadedAt.time(), QLocale::ShortFormat)));
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::setOnlineMode(bool isOnline)
{
    m_networkOnline = isOnline;
    if (!isOnline)
        abortCalendarRequests();
    refreshCredentials();
}

void TabIntervalsIcu::abortCalendarRequests()
{
    for (QNetworkReply **reply : {&m_calendarReply, &m_downloadReply,
                                  &m_syncCalendarReply, &m_syncDownloadReply}) {
        if (*reply) {
            (*reply)->abort();
            (*reply)->deleteLater();
            *reply = nullptr;
        }
    }
}

void TabIntervalsIcu::setCalendarAvailable(bool available, const QString &banner)
{
    m_calendarAvailable = available;
    ui->label_offline->setText(banner);
    ui->label_offline->setVisible(!available);
    for (QWidget *control : std::initializer_list<QWidget *>{
             ui->pushButton_refresh, ui->pushButton_prevWeek, ui->pushButton_nextWeek,
             ui->pushButton_loadWorkout, ui->pushButton_syncAll, ui->label_week,
             ui->tableWidget_calendar, ui->label_status})
        control->setVisible(available);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onRefreshClicked()
{
    auto *account = qApp->property("Account").value<Account *>();
    if (!account) {
        setStatus(tr("Please configure Intervals.icu credentials in "
                     "Preferences → Cloud Sync first."));
        return;
    }

    const bool hasCredentials = !account->intervals_icu_athlete_id.isEmpty() &&
        (!account->intervals_icu_api_key.isEmpty() ||
         !account->intervals_icu_access_token.isEmpty());
    if (!hasCredentials) {
        setStatus(tr("Please configure Intervals.icu credentials in "
                     "Preferences → Cloud Sync first."));
        return;
    }

    // Drop any request in flight; disconnect first, as abort() emits finished().
    if (QNetworkReply *previous = std::exchange(m_calendarReply, nullptr)) {
        previous->disconnect(this);
        previous->abort();
        previous->deleteLater();
    }

    setBusy(true);
    setStatus(tr("Fetching calendar…"));
    m_updatedLabel->setText(tr("Updating..."));

    m_fetchingWeek = m_weekStart;
    const QDate newest = m_weekStart.addDays(6);
    m_calendarReply = m_service->fetchCalendar(m_weekStart, newest);
    if (!m_calendarReply) {
        setBusy(false);
        setStatus(tr("Network error: could not initiate calendar request."));
        LOG_WARN("TabIntervalsIcu",
                 QStringLiteral("onRefreshClicked: fetchCalendar returned null reply"));
        return;
    }
    connect(m_calendarReply, &QNetworkReply::finished,
            this, &TabIntervalsIcu::onCalendarFetchFinished);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onPrevWeekClicked()
{
    m_weekStart = m_weekStart.addDays(-7);
    updateWeekLabel();
    ui->tableWidget_calendar->setRowCount(0);
    m_rowEventIds.clear();
    m_loadedWeek = QDate();
    ui->pushButton_loadWorkout->setEnabled(false);
    autoRefresh(0);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onNextWeekClicked()
{
    m_weekStart = m_weekStart.addDays(7);
    updateWeekLabel();
    ui->tableWidget_calendar->setRowCount(0);
    m_rowEventIds.clear();
    m_loadedWeek = QDate();
    ui->pushButton_loadWorkout->setEnabled(false);
    autoRefresh(0);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onLoadWorkoutClicked()
{
    const int row = ui->tableWidget_calendar->currentRow();
    if (row < 0 || row >= m_rowEventIds.size())
        return;

    const QString eventId = m_rowEventIds[row];
    if (eventId.isEmpty())
        return;

    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    setBusy(true);
    setStatus(tr("Downloading workout file…"));

    // Capture workout name now so a mid-flight selection change can't
    // affect which file name we write.
    const int nameRow = ui->tableWidget_calendar->currentRow();
    const QTableWidgetItem *nameItem = (nameRow >= 0)
        ? ui->tableWidget_calendar->item(nameRow, 1)
        : nullptr;
    m_pendingWorkoutName = nameItem ? nameItem->text() : "intervals_workout";

    // Disable selection while download is in progress
    ui->tableWidget_calendar->setSelectionMode(QAbstractItemView::NoSelection);

    m_downloadReply = m_service->downloadEventZwo(eventId);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &TabIntervalsIcu::onWorkoutDownloadFinished);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onCalendarFetchFinished()
{
    setBusy(false);

    if (!m_calendarReply)
        return;

    if (m_calendarReply->error() != QNetworkReply::NoError) {
        const QString errMsg = m_calendarReply->errorString();
        LOG_WARN("TabIntervalsIcu",
                 QStringLiteral("Calendar fetch failed: ") + errMsg);
        setStatus(tr("Error fetching calendar: %1").arg(errMsg));
        m_calendarReply->deleteLater();
        m_calendarReply = nullptr;
        updateFreshnessLabel();
        return;
    }

    const QByteArray data = m_calendarReply->readAll();
    m_calendarReply->deleteLater();
    m_calendarReply = nullptr;

    const QList<IntervalsIcuService::CalendarEvent> events =
        IntervalsIcuService::parseEvents(data);

    populateTable(events);
    m_loadedWeek = m_fetchingWeek;
    m_loadedAt = QDateTime::currentDateTime();
    updateFreshnessLabel();

    if (events.isEmpty()) {
        setStatus(tr("No events found for this week."));
    } else {
        setStatus(tr("%1 event(s) loaded.").arg(events.size()));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onWorkoutDownloadFinished()
{
    setBusy(false);

    if (!m_downloadReply)
        return;

    if (m_downloadReply->error() != QNetworkReply::NoError) {
        const QString errMsg = m_downloadReply->errorString();
        LOG_WARN("TabIntervalsIcu",
                 QStringLiteral("Workout download failed: ") + errMsg);
        setStatus(tr("Error downloading workout: %1").arg(errMsg));
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
        // Restore selection mode on error too
        ui->tableWidget_calendar->setSelectionMode(QAbstractItemView::SingleSelection);
        return;
    }

    const QByteArray zwoData = m_downloadReply->readAll();
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;

    // Restore normal row selection
    ui->tableWidget_calendar->setSelectionMode(QAbstractItemView::SingleSelection);

    // Intervals.icu exports an empty <workout/> when it could not read the
    // steps (e.g. "10min 80%" instead of "10m 80%"): say so instead of saving
    // a file the library then silently rejects.
    if (ImporterWorkoutZwo::importFromByteArray(zwoData, m_pendingWorkoutName).getLstInterval().isEmpty()) {
        LOG_WARN("TabIntervalsIcu", QStringLiteral("Workout '%1' has no steps in Intervals.icu; not imported")
                                        .arg(m_pendingWorkoutName));
        setStatus(tr("'%1' has no steps in Intervals.icu (its graph there is empty), so there is nothing to load. "
                     "Write each step as a duration and a target, for example 10m 80%.").arg(m_pendingWorkoutName));
        return;
    }

    // Use the name captured when the download was started
    QString workoutName = m_pendingWorkoutName;

    // Sanitise for filesystem use (strip leading/trailing whitespace, then
    // replace everything except safe characters with underscores)
    workoutName = workoutName.trimmed();
    workoutName.replace(QRegularExpression("[^A-Za-z0-9_\\-.]"), "_");

    if (workoutName.isEmpty())
        workoutName = "intervals_workout";

    const QString dir =
        Util::getSystemPathWorkout() + QDir::separator() + "intervals_icu";
    if (!QDir().mkpath(dir)) {
        setStatus(tr("Could not create workout directory: %1").arg(dir));
        return;
    }

    const QString filePath =
        dir + QDir::separator() + workoutName + ".zwo";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatus(tr("Could not save workout file: %1").arg(filePath));
        return;
    }
    file.write(zwoData);
    file.close();

    setStatus(tr("Workout saved: %1").arg(workoutName + ".zwo"));
    emit workoutDownloaded(workoutName);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::updateWeekLabel()
{
    const QDate weekEnd = m_weekStart.addDays(6);
    ui->label_week->setText(
        m_weekStart.toString("d MMM") + " – " +
        weekEnd.toString("d MMM yyyy"));
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::populateTable(
    const QList<IntervalsIcuService::CalendarEvent> &events)
{
    ui->tableWidget_calendar->setRowCount(0);
    m_rowEventIds.clear();
    ui->pushButton_loadWorkout->setEnabled(false);

    for (const IntervalsIcuService::CalendarEvent &ev : events) {
        const int row = ui->tableWidget_calendar->rowCount();
        ui->tableWidget_calendar->insertRow(row);

        ui->tableWidget_calendar->setItem(
            row, 0, new QTableWidgetItem(ev.date.toString("ddd d MMM")));
        ui->tableWidget_calendar->setItem(
            row, 1, new QTableWidgetItem(ev.name));
        ui->tableWidget_calendar->setItem(
            row, 2, new QTableWidgetItem(ev.type));

        // Format duration as h:mm or m:ss
        QString durStr;
        if (ev.duration_sec > 0) {
            const int h = ev.duration_sec / 3600;
            const int m = (ev.duration_sec % 3600) / 60;
            durStr = h > 0
                ? QString("%1h %2m").arg(h).arg(m, 2, 10, QChar('0'))
                : QString("%1 min").arg(m);
        }
        ui->tableWidget_calendar->setItem(
            row, 3, new QTableWidgetItem(durStr));

        m_rowEventIds.append(ev.isDownloadableWorkout() ? ev.id : QString());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::setStatus(const QString &msg)
{
    ui->label_status->setText(msg);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::setBusy(bool busy)
{
    ui->pushButton_refresh->setEnabled(!busy);
    ui->pushButton_prevWeek->setEnabled(!busy);
    ui->pushButton_nextWeek->setEnabled(!busy);
    ui->pushButton_syncAll->setEnabled(!busy);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::startBatchSync(const QDate &from, const QDate &to)
{
    auto *account = qApp->property("Account").value<Account *>();
    if (!account) {
        emit syncFailed(tr("Intervals.icu credentials are not configured. "
                           "Please set them in Preferences → Cloud Sync."));
        return;
    }

    const bool hasCredentials = !account->intervals_icu_athlete_id.isEmpty() &&
        (!account->intervals_icu_api_key.isEmpty() ||
         !account->intervals_icu_access_token.isEmpty());
    if (!hasCredentials) {
        emit syncFailed(tr("Intervals.icu credentials are not configured. "
                           "Please set them in Preferences → Cloud Sync."));
        return;
    }

    if (m_syncCalendarReply) {
        m_syncCalendarReply->abort();
        m_syncCalendarReply->deleteLater();
        m_syncCalendarReply = nullptr;
    }
    if (m_syncDownloadReply) {
        m_syncDownloadReply->abort();
        m_syncDownloadReply->deleteLater();
        m_syncDownloadReply = nullptr;
    }
    m_syncQueue.clear();
    m_syncCount = 0;
    m_syncTotal = 0;

    setBusy(true);
    setStatus(tr("Sync: fetching calendar…"));

    m_syncCalendarReply = m_service->fetchCalendar(from, to);
    connect(m_syncCalendarReply, &QNetworkReply::finished,
            this, &TabIntervalsIcu::onSyncCalendarFetched);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onSyncAllClicked()
{
    startBatchSync(m_weekStart, m_weekStart.addDays(6));
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onSyncCalendarFetched()
{
    if (!m_syncCalendarReply) return;

    if (m_syncCalendarReply->error() != QNetworkReply::NoError) {
        const QString err = m_syncCalendarReply->errorString();
        LOG_WARN("TabIntervalsIcu",
                 QStringLiteral("Sync calendar fetch failed: ") + err);
        m_syncCalendarReply->deleteLater();
        m_syncCalendarReply = nullptr;
        setBusy(false);
        setStatus(tr("Sync failed: %1").arg(err));
        emit syncFailed(err);
        return;
    }

    const QByteArray data = m_syncCalendarReply->readAll();
    m_syncCalendarReply->deleteLater();
    m_syncCalendarReply = nullptr;

    const QList<IntervalsIcuService::CalendarEvent> allEvents =
        IntervalsIcuService::parseEvents(data);

    for (const IntervalsIcuService::CalendarEvent &ev : allEvents) {
        if (ev.isDownloadableWorkout())
            m_syncQueue.append(ev);
    }

    m_syncTotal = m_syncQueue.size();
    m_syncCount = 0;

    if (m_syncQueue.isEmpty()) {
        setBusy(false);
        setStatus(tr("Sync: no downloadable workouts found for this week."));
        emit syncFinished(0);
        return;
    }

    setStatus(tr("Sync: downloading %1 workout(s)…").arg(m_syncTotal));
    downloadNextSyncWorkout();
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::downloadNextSyncWorkout()
{
    if (m_syncQueue.isEmpty()) {
        setBusy(false);
        setStatus(tr("Sync complete — %1 workout(s) imported.").arg(m_syncCount));
        emit syncFinished(m_syncCount);
        return;
    }

    const IntervalsIcuService::CalendarEvent ev = m_syncQueue.takeFirst();
    m_pendingSyncWorkoutName = ev.name.trimmed();
    if (m_pendingSyncWorkoutName.isEmpty())
        m_pendingSyncWorkoutName = ev.id;

    m_syncDownloadReply = m_service->downloadEventZwo(ev.id);
    connect(m_syncDownloadReply, &QNetworkReply::finished,
            this, &TabIntervalsIcu::onSyncWorkoutDownloaded);
}

// ─────────────────────────────────────────────────────────────────────────────
void TabIntervalsIcu::onSyncWorkoutDownloaded()
{
    if (!m_syncDownloadReply) return;

    const QNetworkReply::NetworkError err = m_syncDownloadReply->error();
    if (err != QNetworkReply::NoError) {
        LOG_WARN("TabIntervalsIcu",
                 QStringLiteral("Sync download failed for workout '")
                 + m_pendingSyncWorkoutName + QStringLiteral("': ")
                 + m_syncDownloadReply->errorString());
        // Continue with remaining queue rather than aborting the whole sync
        m_syncDownloadReply->deleteLater();
        m_syncDownloadReply = nullptr;
        downloadNextSyncWorkout();
        return;
    }

    const QByteArray zwoData = m_syncDownloadReply->readAll();
    m_syncDownloadReply->deleteLater();
    m_syncDownloadReply = nullptr;

    Workout imported = ImporterWorkoutZwo::importFromByteArray(zwoData, m_pendingSyncWorkoutName);
    if (!imported.getLstInterval().isEmpty()) {
        QString safeName = imported.getName();
        safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")),
                         QStringLiteral("_"));
        if (safeName.isEmpty())
            safeName = m_pendingSyncWorkoutName;
        safeName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_\\-. ]")),
                         QStringLiteral("_"));

        const QString workoutDir =
            Util::getSystemPathWorkout() + QDir::separator() + QStringLiteral("intervals");
        if (QDir().mkpath(workoutDir)) {
            QString uniqueName = safeName;
            for (int n = 1;
                 QFile::exists(workoutDir + QDir::separator() + uniqueName + QStringLiteral(".workout"));
                 ++n)
            {
                uniqueName = safeName + QStringLiteral("_") + QString::number(n);
            }

            const QString destPath =
                workoutDir + QDir::separator() + uniqueName + QStringLiteral(".workout");
            if (XmlUtil::createWorkoutXml(imported, destPath)) {
                ++m_syncCount;
                const int remaining = m_syncQueue.size();
                if (remaining > 0)
                    setStatus(tr("Sync: imported '%1' (%2 remaining)…")
                                  .arg(imported.getName())
                                  .arg(remaining));
            }
        }
    }

    downloadNextSyncWorkout();
}
