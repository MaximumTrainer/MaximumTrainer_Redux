#include "postworkoutpanel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QPainter>
#include <QPainterPath>
#include <QLabel>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "account.h"
#include "achievementcatalog.h"
#include "app_settings.h"
#include "managerachievement.h"
#include "fitactivitydetailreader.h"
#include "fitactivityreader.h"
#include "ftpestimator.h"
#include "powerzones.h"
#include "ridesummary.h"
#include "intervalsicuftpsync.h"
#include "setting_keys.h"
#include "intervalsicuservice.h"
#include "logger.h"
#include "strava_service.h"
#include "util.h"

namespace {

QString clockText(int seconds)
{
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;
    return hours > 0 ? QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'))
                     : QStringLiteral("%1:%2").arg(minutes).arg(secs, 2, 10, QLatin1Char('0'));
}

/// Time in each power zone as one stacked bar.
class ZoneBar : public QWidget
{
public:
    ZoneBar(const RideSummary &ride, QWidget *parent) : QWidget(parent), m_ride(ride)
    {
        setFixedHeight(14);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(rect(), 5, 5);
        painter.setClipPath(clip);
        painter.fillRect(rect(), QColor(255, 255, 255, 20));
        const int total = qMax(1, m_ride.zoneTotalSeconds());
        double x = 0;
        for (int zone = 0; zone < m_ride.zoneSeconds.size(); ++zone) {
            const double w = width() * double(m_ride.zoneSeconds.at(zone)) / total;
            painter.fillRect(QRectF(x, 0, w, height()), PowerZones::color(zone));
            x += w;
        }
    }

private:
    RideSummary m_ride;
};

/// Average heart rate on the last rides of the same workout, one bar each:
/// the lowest (best) is green with a star, today blue, the rest grey.
class HrTrendChart : public QWidget
{
public:
    HrTrendChart(const RideTrend &trend, QWidget *parent) : QWidget(parent), m_trend(trend)
    {
        setFixedHeight(104);
        setMinimumWidth(240);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const int count = int(m_trend.rides.size());
        int lowest = m_trend.rides.first().avgHrBpm;
        int highest = lowest;
        for (const RideTrend::Ride &ride : m_trend.rides) {
            lowest = qMin(lowest, ride.avgHrBpm);
            highest = qMax(highest, ride.avgHrBpm);
        }
        // Start the bars a little below the lowest value so differences show.
        const double floor = lowest - qMax(4, (highest - lowest) * 2 / 3);
        const double span = qMax(1.0, highest - floor);
        constexpr double kValueRow = 16.0;
        constexpr double kDateRow = 16.0;
        const double chartHeight = height() - kValueRow - kDateRow;
        const double slot = double(width()) / count;
        const double barWidth = qMin(36.0, slot - 10.0);

        QFont valueFont = font();
        valueFont.setPointSizeF(8.5);
        valueFont.setWeight(QFont::DemiBold);
        QFont dateFont = font();
        dateFont.setPointSizeF(7.5);
        const QColor best(0x3f, 0xb2, 0x7f);
        for (int index = 0; index < count; ++index) {
            const RideTrend::Ride &ride = m_trend.rides.at(index);
            const bool isBest = index == m_trend.bestIndex;
            const double barHeight = qMax(3.0, chartHeight * (ride.avgHrBpm - floor) / span);
            const double x = slot * index + (slot - barWidth) / 2;
            const QRectF bar(x, kValueRow + chartHeight - barHeight, barWidth, barHeight);
            const QColor color = isBest ? best : ride.isCurrent ? QColor(0x3f, 0x8f, 0xd8) : QColor(70, 70, 80);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRoundedRect(bar, 4, 4);

            painter.setFont(valueFont);
            painter.setPen(isBest ? best : QColor(0xe6, 0xe6, 0xea));
            const QString value = (isBest ? QStringLiteral("\u2605 ") : QString()) + QString::number(ride.avgHrBpm);
            painter.drawText(QRectF(slot * index, bar.top() - kValueRow, slot, kValueRow),
                             Qt::AlignHCenter | Qt::AlignBottom, value);
            painter.setFont(dateFont);
            painter.setPen(QColor(0x96, 0x96, 0xa0));
            const QString date = ride.isCurrent ? PostWorkoutPanel::tr("Today")
                                                : QLocale().toString(ride.date, QStringLiteral("d MMM"));
            painter.drawText(QRectF(slot * index, height() - kDateRow, slot, kDateRow), Qt::AlignCenter, date);
        }
    }

private:
    RideTrend m_trend;
};

QWidget *statTile(const QString &caption, const QString &value, QWidget *parent)
{
    auto *tile = new QFrame(parent);
    tile->setObjectName(QStringLiteral("statTile"));
    tile->setStyleSheet(QStringLiteral("QFrame#statTile { background-color: rgba(255,255,255,16); border-radius: 8px; }"));
    auto *tileLayout = new QVBoxLayout(tile);
    tileLayout->setContentsMargins(12, 8, 12, 8);
    tileLayout->setSpacing(0);
    auto *captionLabel = new QLabel(caption.toUpper(), tile);
    captionLabel->setStyleSheet(QStringLiteral("font-size: 8pt; color: #9a9aa6; letter-spacing: 1px;"));
    auto *valueLabel = new QLabel(value, tile);
    valueLabel->setStyleSheet(QStringLiteral("font-size: 16pt; font-weight: bold;"));
    tileLayout->addWidget(captionLabel);
    tileLayout->addWidget(valueLabel);
    return tile;
}

} // namespace

PostWorkoutPanel::PostWorkoutPanel(Account *account, const QList<Achievement> &earnedAchievements,
                                   const QString &fitFilePath, const QString &fitFileName,
                                   const QString &fitFileDescription, QWidget *parent)
    : QWidget(parent)
    , account(account)
    , fitFilePath(fitFilePath)
    , fitFileName(fitFileName)
    , fitFileDescription(fitFileDescription)
{
    setObjectName("widgetPostWorkout");
    // A plain QWidget only paints its style-sheet background with this set.
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(
        "QWidget#widgetPostWorkout { background-color: rgba(10,10,10,220); border-radius: 12px; }"
        "QLabel  { color: white; }"
        "QPushButton { background-color: #2a6099; color: white; border-radius: 6px; padding: 7px 16px; font-size: 10pt; }"
        "QPushButton:hover { background-color: #3275bd; }"
        "QPushButton:disabled { background-color: #1a3a59; color: #888; }");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(26, 18, 26, 20);

    auto *titleLabel = new QLabel(tr("Workout Complete!"), this);
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #80c0ff;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    if (!fitFileName.isEmpty()) {
        auto *nameLabel = new QLabel(fitFileName, this);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("font-size: 10pt; color: #9a9aa6;");
        layout->addWidget(nameLabel);
    }
    m_profileNote = new QLabel(this);
    m_profileNote->setWordWrap(true);
    m_profileNote->setAlignment(Qt::AlignCenter);
    m_profileNote->setStyleSheet("font-size: 10pt; color: #c8e6c9;");
    m_profileNote->hide();
    layout->addWidget(m_profileNote);

    // The ride on the left and what to do next (achievements, FTP, uploads) on
    // the right keeps the panel short enough to fit; narrow windows stack them.
    const bool wide = !parent || parent->width() >= 980;
    auto *rideColumn = new QVBoxLayout;
    rideColumn->setSpacing(8);
    addRideSummary(rideColumn);
    rideColumn->addStretch();
    auto *sidePanel = new QWidget(this);
    auto *side = new QVBoxLayout(sidePanel);
    side->setContentsMargins(0, 0, 0, 0);
    side->setSpacing(8);

    // — Achievements section —
    if (!earnedAchievements.isEmpty()) {
        auto *achHeader = new QLabel(tr("Achievements earned"), this);
        achHeader->setStyleSheet("font-size: 10pt; font-weight: bold; color: #f0c040;");
        side->addWidget(achHeader);
        for (const Achievement &a : earnedAchievements) {
            auto *row = new QLabel(QString("🏆  %1").arg(a.getName()), this);
            row->setStyleSheet("font-size: 10pt;");
            side->addWidget(row);
        }
    }

    addFtpSuggestion(side);

    // — Upload section —
    const bool hasStrava   = !account->strava_access_token.isEmpty();
    const bool hasIcu      = !account->intervals_icu_athlete_id.isEmpty() &&
                          (!account->intervals_icu_api_key.isEmpty() ||
                           !account->intervals_icu_access_token.isEmpty());
    const auto uploadMode = [](bool linked, bool autoUpload) {
        return !linked ? QStringLiteral("not linked")
                       : autoUpload ? QStringLiteral("auto-upload") : QStringLiteral("manual button");
    };
    LOG_INFO("PostWorkoutPanel", QStringLiteral("Uploads: Strava %1, Intervals.icu %2")
             .arg(uploadMode(hasStrava, account->strava_auto_upload),
                  uploadMode(hasIcu, account->intervals_icu_auto_upload)));

    if (hasStrava || hasIcu) {
        auto *upHeader = new QLabel(tr("Upload activity"), this);
        upHeader->setStyleSheet("font-size: 10pt; font-weight: bold; color: #80c0ff;");
        side->addSpacing(6);
        side->addWidget(upHeader);

        if (hasStrava) {
            // Manual mode (auto-upload off) shows a button to trigger the upload;
            // both modes share a word-wrapped status label that shows progress
            // and the full error text on failure.
            if (!account->strava_auto_upload) {
                auto *btn = new QPushButton(tr("Upload to Strava"), this);
                btn->setObjectName("btnStrava");
                connect(btn, &QPushButton::clicked, this, &PostWorkoutPanel::uploadToStrava);
                side->addWidget(btn);
            }
            auto *lbl = new QLabel(this);
            lbl->setObjectName("lblStrava");
            lbl->setWordWrap(true);
            lbl->setStyleSheet("font-size: 11pt;");
            // Allow the "View on Strava" link (shown on success) to open the
            // activity in the user's browser.
            lbl->setOpenExternalLinks(true);
            if (account->strava_auto_upload)
                lbl->setText(tr("Uploading to Strava…"));
            lbl->setVisible(!lbl->text().isEmpty());
            side->addWidget(lbl);
            if (account->strava_auto_upload)
                QTimer::singleShot(0, this, &PostWorkoutPanel::uploadToStrava);
        }
        if (hasIcu) {
            if (!account->intervals_icu_auto_upload) {
                auto *btn = new QPushButton(tr("Upload to Intervals.icu"), this);
                btn->setObjectName("btnIntervalsIcu");
                connect(btn, &QPushButton::clicked, this, &PostWorkoutPanel::uploadToIntervalsIcu);
                side->addWidget(btn);
            }
            auto *lbl = new QLabel(this);
            lbl->setObjectName("lblIntervalsIcu");
            lbl->setWordWrap(true);
            lbl->setStyleSheet("font-size: 11pt;");
            // Allow the "View on Intervals.icu" link (shown on success) to
            // open the activity in the user's browser.
            lbl->setOpenExternalLinks(true);
            if (account->intervals_icu_auto_upload)
                lbl->setText(tr("Uploading to Intervals.icu…"));
            lbl->setVisible(!lbl->text().isEmpty());
            side->addWidget(lbl);
            if (account->intervals_icu_auto_upload)
                QTimer::singleShot(0, this, &PostWorkoutPanel::uploadToIntervalsIcu);
        }
    }

    addRideTrend(side);

    side->addStretch();
    const bool hasSide = side->count() > 1;
    if (!hasSide)
        sidePanel->hide();
    auto *body = new QBoxLayout(wide ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    body->setSpacing(24);
    body->addLayout(rideColumn, 1);
    if (hasSide) {
        if (wide)
            sidePanel->setFixedWidth(340);
        body->addWidget(sidePanel, 0, Qt::AlignTop);
    }
    layout->addLayout(body);

    auto *keepRiding = new QPushButton(tr("Keep riding"), this);
    keepRiding->setObjectName("btnKeepRiding");
    keepRiding->setToolTip(tr("Fold this summary into a tab at the bottom and keep pedaling. "
                              "The workout is already saved; extra riding is not recorded."));
    connect(keepRiding, &QPushButton::clicked, this, &PostWorkoutPanel::keepRidingRequested);
    layout->addSpacing(4);
    layout->addWidget(keepRiding, 0, Qt::AlignHCenter);

    if (!wide || !hasSide)
        setMinimumWidth(520);

    fitToContents();
}

// Status labels change after the panel is shown (upload progress, the "View
// on ..." link). Their nested layout only relayouts on a posted event, so a
// bare adjustSize() measured the old one-line text and clipped the link.
void PostWorkoutPanel::fitToContents()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    if (QLayout *panelLayout = layout())
        panelLayout->activate();
    adjustSize();
    if (const QWidget *host = parentWidget()) {
        const int maxTop = qMax(0, host->height() - height());
        const int maxLeft = qMax(0, host->width() - width());
        move(qBound(0, x(), maxLeft), qBound(0, y(), maxTop));
    }
}

// The ride's numbers and time in power zones, read back from its FIT file the
// same way History does.
void PostWorkoutPanel::addRideSummary(QBoxLayout *layout)
{
    if (fitFilePath.isEmpty())
        return;
    const WorkoutHistorySummary current = FitActivityReader::readFile(fitFilePath);
    const RideSummary ride = RideSummary::build(current,
                                                FitActivityDetailReader::readFile(fitFilePath),
                                                account->FTP, PowerZones::boundsPct(account->power_zones_pct));
    if (!ride.valid)
        return;

    QList<QPair<QString, QString>> tiles;
    tiles.append({tr("Time"), clockText(ride.durationSec)});
    if (ride.hasPower) {
        tiles.append({tr("Avg power"), tr("%1 W").arg(ride.avgPowerW)});
        if (ride.normalizedPower > 0)
            tiles.append({tr("NP"), tr("%1 W").arg(ride.normalizedPower)});
        if (ride.intensityFactor > 0)
            tiles.append({tr("IF"), QString::number(ride.intensityFactor, 'f', 2)});
        if (ride.tss > 0)
            tiles.append({tr("TSS"), QString::number(qRound(ride.tss))});
    }
    if (ride.avgHrBpm > 0)
        tiles.append({tr("Avg HR"), tr("%1 bpm").arg(ride.avgHrBpm)});
    if (ride.avgCadence > 0)
        tiles.append({tr("Cadence"), tr("%1 rpm").arg(ride.avgCadence)});
    if (ride.calories > 0)
        tiles.append({tr("Calories"), tr("%1 kcal").arg(ride.calories)});

    auto *grid = new QGridLayout;
    grid->setSpacing(8);
    constexpr int kColumns = 4;
    for (int i = 0; i < tiles.size(); ++i)
        grid->addWidget(statTile(tiles.at(i).first, tiles.at(i).second, this), i / kColumns, i % kColumns);
    layout->addLayout(grid);

    // Progress on the same workout: the last ride of it side by side, and the
    // heart-rate trend over the last rides (shown in the side column).
    constexpr int kTrendRides = 6;
    QList<WorkoutHistorySummary> previousRides;
    for (const QString &file : RideComparison::previousRideFiles(Util::getSystemPathHistory(), fitFilePath,
                                                                 current.durationSec, kTrendRides - 1))
        previousRides.append(FitActivityReader::readFile(file));
    m_trend = RideTrend::build(current, previousRides);
    if (!previousRides.isEmpty()) {
        const RideComparison diff = RideComparison::compare(current, previousRides.first());
        auto signedText = [](double value, const QString &unit) {
            const int rounded = qRound(value);
            const QString number = rounded > 0 ? QStringLiteral("+%1").arg(rounded)
                                 : rounded < 0 ? QStringLiteral("\u2212%1").arg(-rounded)
                                               : QStringLiteral("=");
            return unit.isEmpty() || rounded == 0 ? number : number + QLatin1Char(' ') + unit;
        };
        auto coloured = [](const QString &text, int goodSign, int value) {
            const QString color = value == 0 ? QStringLiteral("#c8c8d0")
                                : (value > 0) == (goodSign > 0) ? QStringLiteral("#3fb27f") : QStringLiteral("#f2a33a");
            return QStringLiteral("<b style=\"color:%1\">%2</b>").arg(color, text);
        };
        // In ERG the watts follow the workout, so the body's answer is what
        // moves: heart rate and watts per beat. Without heart rate, fall back to power.
        QStringList parts;
        if (diff.hasHeartRate) {
            parts << tr("Avg HR %1").arg(coloured(signedText(diff.hrDeltaBpm, tr("bpm")), -1, diff.hrDeltaBpm));
            if (diff.hasEfficiency) {
                const int hundredths = qRound(diff.efficiencyDelta * 100);
                const QString text = hundredths == 0 ? QStringLiteral("=")
                                   : QStringLiteral("%1%2 %3").arg(hundredths > 0 ? QStringLiteral("+") : QStringLiteral("\u2212"))
                                         .arg(QString::number(qAbs(hundredths) / 100.0, 'f', 2), tr("W/bpm"));
                parts << tr("Efficiency %1").arg(coloured(text, +1, hundredths));
            }
            if (diff.currentFtpW > 0 && diff.previousFtpW > 0) {
                parts << (qAbs(diff.currentFtpW - diff.previousFtpW) <= 2
                              ? QStringLiteral("<span style=\"color:#9a9aa6\">%1</span>").arg(tr("same FTP"))
                              : QStringLiteral("<span style=\"color:#9a9aa6\">%1</span>")
                                    .arg(tr("FTP %1 \u2192 %2").arg(diff.previousFtpW).arg(diff.currentFtpW)));
            }
        } else {
            if (diff.hasPower)
                parts << tr("NP %1").arg(coloured(signedText(diff.npDeltaW, tr("W")), +1, diff.npDeltaW));
            if (diff.tssDelta != 0.0 || diff.hasPower)
                parts << tr("TSS %1").arg(coloured(signedText(diff.tssDelta, {}), 0, 0));
        }
        if (diff.valid && !parts.isEmpty()) {
            const QString when = QLocale().toString(diff.previousStart.date(), QStringLiteral("d MMM"));
            auto *row = new QLabel(QStringLiteral("<span style=\"color:#9a9aa6\">%1</span>&nbsp;&nbsp;&nbsp;%2")
                                       .arg(tr("vs last time (%1)").arg(when),
                                            parts.join(QStringLiteral("&nbsp;&nbsp;&nbsp;"))), this);
            row->setTextFormat(Qt::RichText);
            row->setStyleSheet(QStringLiteral("font-size: 10pt; color: #e8e8ee;"));
            layout->addWidget(row);
        }
    }

    if (ride.hasPower && ride.zoneTotalSeconds() > 0) {
        auto *zonesHeader = new QLabel(account->power_zones_pct.isEmpty() ? tr("Time in power zones")
                                                                     : tr("Time in power zones (Intervals.icu)"), this);
        zonesHeader->setStyleSheet(QStringLiteral("font-size: 9pt; color: #9a9aa6; margin-top: 6px;"));
        layout->addWidget(zonesHeader);
        layout->addWidget(new ZoneBar(ride, this));
        QStringList legend;
        for (int zone = 0; zone < ride.zoneSeconds.size(); ++zone) {
            if (ride.zoneSeconds.at(zone) <= 0)
                continue;
            legend.append(QStringLiteral("<span style=\"color:%1\">&#9632;</span> Z%2 %3")
                              .arg(PowerZones::color(zone).name()).arg(zone + 1).arg(clockText(ride.zoneSeconds.at(zone))));
        }
        auto *legendLabel = new QLabel(legend.join(QStringLiteral("&nbsp;&nbsp;&nbsp;")), this);
        legendLabel->setTextFormat(Qt::RichText);
        legendLabel->setWordWrap(true);
        legendLabel->setStyleSheet(QStringLiteral("font-size: 9pt; color: #c8c8d0;"));
        layout->addWidget(legendLabel);
    }
}

void PostWorkoutPanel::addRideTrend(QBoxLayout *layout)
{
    if (!m_trend.valid)
        return;
    auto *header = new QLabel(tr("Heart rate on this workout"), this);
    header->setStyleSheet(QStringLiteral("font-size: 10pt; font-weight: bold; color: #80c0ff;"));
    layout->addSpacing(6);
    layout->addWidget(header);
    layout->addWidget(new HrTrendChart(m_trend, this));

    const int rides = int(m_trend.rides.size());
    QString headline;
    if (m_trend.currentIsBest()) {
        headline = QStringLiteral("<b style=\"color:#3fb27f\">%1</b>").arg(tr("Lowest heart rate yet"));
        if (m_trend.currentPowerW > 0)
            headline += QStringLiteral("&nbsp;&nbsp;<span style=\"color:#9a9aa6\">%1</span>")
                            .arg(tr("at the same %1 W").arg(m_trend.currentPowerW));
    } else {
        const int bestHr = m_trend.rides.at(m_trend.bestIndex).avgHrBpm;
        headline = QStringLiteral("<b style=\"color:#f2a33a\">%1</b>")
                       .arg(tr("%1 bpm above your best").arg(m_trend.rides.last().avgHrBpm - bestHr));
    }
    const double efficiency = m_trend.rides.last().efficiency;
    QString detail;
    if (efficiency > 0.0) {
        const QString rank = m_trend.currentEfficiencyRank == 1
                                 ? tr("best of %1 rides").arg(rides)
                                 : tr("#%1 of %2 rides").arg(m_trend.currentEfficiencyRank).arg(rides);
        detail = tr("Efficiency %1 W/bpm").arg(QString::number(efficiency, 'f', 2)) + QStringLiteral(" \u00b7 ") + rank;
    }
    auto *summary = new QLabel(detail.isEmpty() ? headline : headline + QStringLiteral("<br>") + detail, this);
    summary->setTextFormat(Qt::RichText);
    summary->setWordWrap(true);
    summary->setStyleSheet(QStringLiteral("font-size: 10pt; color: #c8c8d0;"));
    layout->addWidget(summary);
}

void PostWorkoutPanel::setProfileNote(const QString &text)
{
    m_profileNote->setText(text);
    m_profileNote->setVisible(!text.isEmpty());
    fitToContents();
}

// Suggests a higher FTP when the recent rides show one (see FtpEstimator).
void PostWorkoutPanel::addFtpSuggestion(QBoxLayout *layout)
{
    if (account->enable_studio_mode || account->rowing_mode || account->FTP <= 0)
        return;
    const FtpEstimator::Estimate estimate = FtpEstimator::fromRides(
        FtpEstimator::recentRides(Util::getSystemPathHistory(), QDateTime::currentDateTime()));
    const int dismissed = AppSettings::read(SettingKeys::Training::FtpSuggestionDismissedW);
    if (!estimate.valid || !FtpEstimator::worthSuggesting(account->FTP, estimate.ftpW, dismissed))
        return;

    auto *header = new QLabel(tr("New FTP detected: %1 W").arg(estimate.ftpW), this);
    header->setStyleSheet("font-size: 10pt; font-weight: bold; color: #80c0ff;");
    layout->addSpacing(6);
    layout->addWidget(header);
    auto *detail = new QLabel(tr("Your best 20 minutes in the last %1 weeks was %2 W, so your FTP "
                                 "is about %3 W (currently %4 W).")
                                  .arg(FtpEstimator::kWindowDays / 7).arg(estimate.best20W)
                                  .arg(estimate.ftpW).arg(account->FTP), this);
    detail->setWordWrap(true);
    detail->setStyleSheet("font-size: 10pt;");
    layout->addWidget(detail);

    auto *buttons = new QHBoxLayout();
    auto *update = new QPushButton(tr("Update FTP"), this);
    auto *later  = new QPushButton(tr("Not now"), this);
    later->setStyleSheet("background-color: #444;");
    buttons->addWidget(update);
    buttons->addWidget(later);
    layout->addLayout(buttons);
    m_ftpStatus = new QLabel(this);
    m_ftpStatus->setWordWrap(true);
    m_ftpStatus->setStyleSheet("font-size: 10pt;");
    m_ftpStatus->hide();
    layout->addWidget(m_ftpStatus);

    const int ftp = estimate.ftpW;
    connect(update, &QPushButton::clicked, this, [this, ftp, update, later]() {
        update->setEnabled(false);
        later->setEnabled(false);
        acceptFtp(ftp);
    });
    connect(later, &QPushButton::clicked, this, [this, ftp, update, later]() {
        AppSettings::write(SettingKeys::Training::FtpSuggestionDismissedW, ftp);
        update->setEnabled(false);
        later->setEnabled(false);
        m_ftpStatus->setText(tr("FTP left at %1 W.").arg(account->FTP));
        m_ftpStatus->show();
        fitToContents();
    });
}

void PostWorkoutPanel::acceptFtp(int ftpWatts)
{
    if (auto *achievements = qApp->property("ManagerAchievement").value<ManagerAchievement*>();
        achievements && ftpWatts > account->FTP)
        achievements->recordEvent(AchievementCatalog::Events::FtpRaised);
    account->saveProfileFields(ftpWatts, account->LTHR, account->weight_kg);
    AppSettings::write(SettingKeys::Training::FtpSuggestionDismissedW, ftpWatts);
    emit ftpUpdated(ftpWatts);
    m_ftpStatus->show();

    // Login copies the FTP from Intervals.icu, so save it there too or the
    // next login would put the old value back.
    if (account->intervals_icu_access_token.isEmpty() || account->intervals_icu_athlete_id.isEmpty()) {
        m_ftpStatus->setText(tr("✓ FTP updated to %1 W.").arg(ftpWatts));
        fitToContents();
        return;
    }
    m_ftpStatus->setText(tr("FTP updated to %1 W. Saving to Intervals.icu…").arg(ftpWatts));
    fitToContents();
    auto *sync = new IntervalsIcuFtpSync(account->intervals_icu_athlete_id,
                                         account->intervals_icu_access_token, ftpWatts, this);
    connect(sync, &IntervalsIcuFtpSync::finished, this, [this, ftpWatts](bool ok, const QString &error) {
        if (ok)
            m_ftpStatus->setText(tr("✓ FTP updated to %1 W, here and on Intervals.icu.").arg(ftpWatts));
        else
            m_ftpStatus->setText(tr("FTP updated to %1 W here, but saving it to Intervals.icu failed (%2). "
                                    "Your next login may restore the old value.").arg(ftpWatts).arg(error));
        fitToContents();
    });
    sync->start();
}

// Updates the post-workout Strava status — the manual button (auto-upload off)
// or the status label (auto-upload on), whichever is present.
void PostWorkoutPanel::setStravaPostStatus(const QString &text, bool retryable)
{
    if (auto *lbl = findChild<QLabel*>("lblStrava")) {
        lbl->setText(text);
        lbl->setVisible(!text.isEmpty());
    }
    // The manual button (if any) is re-enabled only when a retry makes sense.
    if (auto *btn = findChild<QPushButton*>("btnStrava"))
        btn->setEnabled(retryable);
    fitToContents();
}

void PostWorkoutPanel::uploadToStrava()
{
    if (fitFilePath.isEmpty()) return;

    // Access tokens live only 6 h — refresh via the token Worker if expired,
    // then upload.
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (account->strava_token_expires_at > now + 60
        || account->strava_refresh_token.isEmpty()) {
        doStravaUpload();
        return;
    }
    setStravaPostStatus(tr("Refreshing Strava token…"), false);
    QNetworkReply *refresh = StravaService::refreshToken(account->strava_refresh_token);
    if (!refresh) { doStravaUpload(); return; }
    connect(refresh, &QNetworkReply::finished, this, [this, refresh]() {
        refresh->deleteLater();
        if (refresh->error() == QNetworkReply::NoError) {
            Util::parseJsonStravaObject(QString::fromUtf8(refresh->readAll()));
            account->saveStravaCredentials();
        } else {
            LOG_WARN("PostWorkoutPanel",
                     QStringLiteral("Strava token refresh failed: ") + refresh->errorString());
        }
        doStravaUpload();
    });
}

void PostWorkoutPanel::doStravaUpload()
{
    setStravaPostStatus(tr("Uploading to Strava…"), false);
    StravaService svc;
    svc.setAccessToken(account->strava_access_token);
    replyPostStravaUpload = svc.uploadActivity(fitFilePath, fitFileName, fitFileDescription,
                                               /*onTrainer*/ true);
    if (!replyPostStravaUpload) {
        setStravaPostStatus(tr("Upload to Strava (Failed — Retry)"), true);
        return;
    }
    connect(replyPostStravaUpload, &QNetworkReply::finished,
            this, &PostWorkoutPanel::slotPostStravaUploadDone);
}

void PostWorkoutPanel::slotPostStravaUploadDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostStravaUpload = nullptr;

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    LOG_INFO("PostWorkoutPanel",
             QStringLiteral("Strava upload response (HTTP %1): %2")
                 .arg(httpStatus).arg(QString::fromUtf8(body)));

    if (reply->error() != QNetworkReply::NoError) {
        // A 401 means the access token is stale or revoked even though the
        // local expiry bookkeeping thought otherwise. Force a refresh through
        // the token Worker and retry once before surfacing the failure.
        if (httpStatus == 401 && !m_stravaUpload401Retried
            && !account->strava_refresh_token.isEmpty()) {
            m_stravaUpload401Retried = true;
            account->strava_token_expires_at = 0;
            LOG_WARN("PostWorkoutPanel",
                     QStringLiteral("Strava upload got 401 – forcing token refresh and retrying"));
            uploadToStrava();
            return;
        }
        LOG_WARN("PostWorkoutPanel",
                 QStringLiteral("Strava upload failed: ") + reply->errorString());
        setStravaPostStatus(tr("✗ Strava upload failed (HTTP %1): %2")
                                .arg(httpStatus).arg(QString::fromUtf8(body).trimmed()), true);
        return;
    }

    stravaUploadID_post = Util::parseIdJsonStravaUploadObject(QString::fromUtf8(body));

    if (stravaUploadID_post <= 0) {
        LOG_WARN("PostWorkoutPanel",
                 QStringLiteral("Strava upload: no upload id in response — ")
                 + QString::fromUtf8(body));
        setStravaPostStatus(tr("✗ Strava did not accept the upload: %1")
                                .arg(QString::fromUtf8(body).trimmed()), true);
        return;
    }

    setStravaPostStatus(tr("Uploading to Strava…"), false);
    timerPostStravaStatus = new QTimer(this);
    connect(timerPostStravaStatus, &QTimer::timeout, this, &PostWorkoutPanel::slotPostStravaCheckStatus);
    timerPostStravaStatus->start(3000);
}

void PostWorkoutPanel::slotPostStravaCheckStatus()
{
    StravaService svc;
    svc.setAccessToken(account->strava_access_token);
    replyPostStravaStatus = svc.checkUploadStatus(stravaUploadID_post);
    if (replyPostStravaStatus)
        connect(replyPostStravaStatus, &QNetworkReply::finished,
                this, &PostWorkoutPanel::slotPostStravaStatusDone);
}

void PostWorkoutPanel::slotPostStravaStatusDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostStravaStatus = nullptr;

    const QByteArray statusBody = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        if (timerPostStravaStatus) timerPostStravaStatus->stop();
        LOG_WARN("PostWorkoutPanel", QStringLiteral("Strava status check failed: ") + reply->errorString());
        setStravaPostStatus(tr("✗ Strava status check failed: %1").arg(reply->errorString()), true);
        return;
    }

    const int code = Util::parseStravaUploadStatus(QString::fromUtf8(statusBody));
    // 0 = completed, 1 = in progress, 2 = error, -1 = unexpected
    if (code == 1) return;  // keep timer running

    if (timerPostStravaStatus) { timerPostStravaStatus->stop(); timerPostStravaStatus->deleteLater(); timerPostStravaStatus = nullptr; }

    if (code == 0) {
        // Per Strava brand guidelines, deep-link back to the activity with a
        // "View on Strava" link in Strava orange (#FC5200).
        const qint64 activityId = Util::parseStravaActivityId(QString::fromUtf8(statusBody));
        if (activityId > 0) {
            const QString link =
                QStringLiteral("<a style=\"color:#FC5200; text-decoration:underline;\" "
                               "href=\"https://www.strava.com/activities/%1\">%2</a>")
                    .arg(activityId)
                    .arg(tr("View on Strava"));
            // Link on its own line so it never gets clipped on a narrow panel.
            setStravaPostStatus(tr("✓ Uploaded to Strava") + "<br>" + link, false);
        } else {
            setStravaPostStatus(tr("✓ Uploaded to Strava"), false);
        }
        LOG_INFO("PostWorkoutPanel", activityId > 0
                 ? QStringLiteral("Strava upload succeeded: https://www.strava.com/activities/%1").arg(activityId)
                 : QStringLiteral("Strava upload succeeded (no activity id in status)"));
    } else {
        LOG_WARN("PostWorkoutPanel", QStringLiteral("Strava processing error: ") + QString::fromUtf8(statusBody));
        setStravaPostStatus(tr("✗ Strava could not process the activity: %1")
                                .arg(QString::fromUtf8(statusBody).trimmed()), true);
    }
}

// Updates the post-workout Intervals.icu status — the manual button
// (auto-upload off) or the status label (auto-upload on), whichever is present.
void PostWorkoutPanel::setIntervalsIcuPostStatus(const QString &text, bool retryable)
{
    if (auto *lbl = findChild<QLabel*>("lblIntervalsIcu")) {
        lbl->setText(text);
        lbl->setVisible(!text.isEmpty());
    }
    if (auto *btn = findChild<QPushButton*>("btnIntervalsIcu"))
        btn->setEnabled(retryable);
    fitToContents();
}

void PostWorkoutPanel::uploadToIntervalsIcu()
{
    if (fitFilePath.isEmpty()) return;

    auto *svc = new IntervalsIcuService(this);
    svc->setCredentials(account->intervals_icu_api_key, account->intervals_icu_athlete_id);
    svc->setAccessToken(account->intervals_icu_access_token);
    const QString externalId = QFileInfo(fitFilePath).baseName();
    LOG_INFO("PostWorkoutPanel", QStringLiteral("Intervals.icu upload: %1 (athlete %2, %3)")
             .arg(fitFilePath, account->intervals_icu_athlete_id,
                  account->intervals_icu_access_token.isEmpty() ? QStringLiteral("API key")
                                                                : QStringLiteral("OAuth")));
    replyPostIntervalsIcuUpload = svc->uploadActivity(fitFilePath, fitFileName, externalId);
    svc->deleteLater();

    if (!replyPostIntervalsIcuUpload) {
        LOG_WARN("PostWorkoutPanel", QStringLiteral("Intervals.icu upload could not start"));
        setIntervalsIcuPostStatus(tr("✗ Intervals.icu upload could not start — check your connection."), true);
        return;
    }
    setIntervalsIcuPostStatus(tr("Uploading to Intervals.icu…"), false);
    connect(replyPostIntervalsIcuUpload, &QNetworkReply::finished,
            this, &PostWorkoutPanel::slotPostIntervalsIcuUploadDone);
}

void PostWorkoutPanel::slotPostIntervalsIcuUploadDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostIntervalsIcuUpload = nullptr;

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        if (httpStatus == 409) {
            LOG_INFO("PostWorkoutPanel", QStringLiteral("Intervals.icu upload: activity already exists (HTTP 409)"));
            setIntervalsIcuPostStatus(tr("✓ Activity already on Intervals.icu"), false);
        } else {
            LOG_WARN("PostWorkoutPanel", QStringLiteral("Intervals.icu upload failed (HTTP %1): %2 %3")
                     .arg(httpStatus).arg(reply->errorString(), QString::fromUtf8(reply->readAll()).trimmed()));
            setIntervalsIcuPostStatus(tr("✗ Intervals.icu upload failed: %1").arg(reply->errorString()), true);
        }
        return;
    }

    // HTTP 201 returns the created activity object; deep-link to it like the
    // Strava panel does (link colour ≈ the intervals.icu logo red).
    const QJsonObject activity = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonValue idValue = activity.value(QStringLiteral("id"));
    const QString activityId = idValue.isString()
            ? idValue.toString()
            : (idValue.isDouble() ? QString::number(static_cast<qint64>(idValue.toDouble()))
                                  : QString());
    if (!activityId.isEmpty()) {
        const QString link =
            QStringLiteral("<a style=\"color:#e8485c; text-decoration:underline;\" "
                           "href=\"https://intervals.icu/activities/%1\">%2</a>")
                .arg(activityId)
                .arg(tr("View on Intervals.icu"));
        setIntervalsIcuPostStatus(tr("✓ Uploaded to Intervals.icu") + "<br>" + link, false);
    } else {
        setIntervalsIcuPostStatus(tr("✓ Uploaded to Intervals.icu"), false);
    }
    LOG_INFO("PostWorkoutPanel", activityId.isEmpty()
             ? QStringLiteral("Intervals.icu upload succeeded (HTTP %1, no activity id in response)").arg(httpStatus)
             : QStringLiteral("Intervals.icu upload succeeded (HTTP %1): https://intervals.icu/activities/%2")
                   .arg(httpStatus).arg(activityId));
}
