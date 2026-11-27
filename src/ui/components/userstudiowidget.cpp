#include "userstudiowidget.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QStyle>
#include <QVBoxLayout>

#include "account.h"
#include "faderlabel.h"
#include "powerzones.h"

namespace {
const QString kGrey   = QStringLiteral("#9a9aa6");
const QString kWhite  = QStringLiteral("#ececf0");
const QString kGreen  = QStringLiteral("#3fb27f");
const QString kAmber  = QStringLiteral("#f2a33a");
const QString kRed    = QStringLiteral("#e5534b");
const QString kYellow = QStringLiteral("#f2c230");

QString dotColor(SensorStatusStrip::State state)
{
    switch (state) {
    case SensorStatusStrip::State::Connected:  return kGreen;
    case SensorStatusStrip::State::Connecting: return kYellow;
    case SensorStatusStrip::State::NotFound:
    case SensorStatusStrip::State::Lost:       return kRed;
    case SensorStatusStrip::State::Searching:
    case SensorStatusStrip::State::Disconnected: break;
    }
    return kGrey;
}

bool needsRetry(SensorStatusStrip::State state)
{
    return state == SensorStatusStrip::State::NotFound || state == SensorStatusStrip::State::Lost;
}
} // namespace

UserStudioWidget::UserStudioWidget(int userID, QString displayName, double FTP, double LTHR, QWidget *parent)
    : QFrame(parent)
    , userID(userID)
    , displayName(std::move(displayName))
    , FTP(FTP)
    , LTHR(LTHR)
{
    setObjectName(QStringLiteral("studioCard"));
    setProperty("targetState", QStringLiteral("none"));
    setStyleSheet(QStringLiteral(
        "QFrame#studioCard { background-color: #26262d; border: 1px solid #34343d; border-radius: 12px; }"
        "QFrame#studioCard[targetState=\"above\"] { border-left: 5px solid %1; }"
        "QFrame#studioCard[targetState=\"below\"] { border-left: 5px solid %2; }"
        "QFrame#studioCard QLabel { background: transparent; border: none; }").arg(kAmber, kRed));
    setFixedHeight(kCardHeight);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 8, 12, 8);
    layout->setSpacing(2);

    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    auto *name = new QLabel(QStringLiteral("%1  %2").arg(userID).arg(this->displayName.toHtmlEscaped()), this);
    name->setStyleSheet(QStringLiteral("font: 600 10pt 'Inter'; color: %1;").arg(kWhite));
    header->addWidget(name);
    if (FTP > 0) {
        auto *ftp = new QLabel(tr("FTP %1").arg(qRound(FTP)), this);
        ftp->setStyleSheet(QStringLiteral("font: 9pt 'Inter'; color: %1;").arg(kGrey));
        header->addWidget(ftp);
    }
    m_ride = new QLabel(this);
    m_ride->setStyleSheet(QStringLiteral("font: 9pt 'Inter'; color: %1;").arg(kGrey));
    header->addWidget(m_ride);
    header->addStretch();
    m_dots = new QHBoxLayout;
    m_dots->setSpacing(4);
    header->addLayout(m_dots);
    layout->addLayout(header);

    auto *body = new QHBoxLayout;
    body->setSpacing(18);

    m_powerBlock = new QWidget(this);
    auto *powerLayout = new QVBoxLayout(m_powerBlock);
    powerLayout->setContentsMargins(0, 0, 0, 0);
    powerLayout->setSpacing(0);
    auto *powerRow = new QHBoxLayout;
    powerRow->setSpacing(4);
    m_power = new QLabel(QStringLiteral("-"), m_powerBlock);
    m_power->setStyleSheet(QStringLiteral("font: 700 26pt 'Inter'; color: %1;").arg(kWhite));
    m_powerUnit = new QLabel(tr("W"), m_powerBlock);
    m_powerUnit->setStyleSheet(QStringLiteral("font: 10pt 'Inter'; color: %1;").arg(kGrey));
    powerRow->addWidget(m_power, 0, Qt::AlignBottom);
    powerRow->addWidget(m_powerUnit, 0, Qt::AlignBottom);
    powerRow->addStretch();
    powerLayout->addLayout(powerRow);
    auto *targetRow = new QHBoxLayout;
    targetRow->setSpacing(8);
    m_target = new QLabel(m_powerBlock);
    m_target->setStyleSheet(QStringLiteral("font: 9pt 'Inter'; color: %1;").arg(kGrey));
    m_zone = new QLabel(m_powerBlock);
    m_zone->hide();
    targetRow->addWidget(m_target);
    targetRow->addWidget(m_zone);
    targetRow->addStretch();
    powerLayout->addLayout(targetRow);
    body->addWidget(m_powerBlock, 0, Qt::AlignVCenter);

    auto *right = new QVBoxLayout;
    right->setSpacing(2);
    auto *metrics = new QHBoxLayout;
    metrics->setSpacing(18);
    m_cadBlock = metricBlock(tr("CADENCE"), m_cad, tr("rpm"));
    m_hrBlock = metricBlock(tr("HEART RATE"), m_hr, tr("bpm"));
    metrics->addWidget(m_cadBlock);
    metrics->addWidget(m_hrBlock);
    metrics->addStretch();
    right->addLayout(metrics);
    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("font: 600 9pt 'Inter';"));
    m_status->installEventFilter(this);
    right->addWidget(m_status);
    m_resultTest = new FaderLabel(this);
    m_resultTest->setStyleSheet(QStringLiteral("font: 600 9pt 'Inter'; color: %1;").arg(kGreen));
    m_resultTest->hide();
    right->addWidget(m_resultTest);
    body->addLayout(right, 1);
    layout->addLayout(body, 1);

    updateStatus();
    updateTooltips();
}

QWidget *UserStudioWidget::metricBlock(const QString &caption, QLabel *&value, const QString &unit)
{
    auto *block = new QWidget(this);
    auto *column = new QVBoxLayout(block);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    auto *captionLabel = new QLabel(caption, block);
    captionLabel->setStyleSheet(QStringLiteral("font: 600 7.5pt 'Inter'; color: %1; letter-spacing: 1px;").arg(kGrey));
    column->addWidget(captionLabel);
    auto *row = new QHBoxLayout;
    row->setSpacing(4);
    value = new QLabel(QStringLiteral("-"), block);
    value->setStyleSheet(QStringLiteral("font: 700 17pt 'Inter'; color: %1;").arg(kWhite));
    auto *unitLabel = new QLabel(unit, block);
    unitLabel->setStyleSheet(QStringLiteral("font: 8.5pt 'Inter'; color: %1;").arg(kGrey));
    row->addWidget(value, 0, Qt::AlignBottom);
    row->addWidget(unitLabel, 0, Qt::AlignBottom);
    column->addLayout(row);
    return block;
}

void UserStudioWidget::setStopped(bool b)
{
    isStopped = b;
    updatePowerState();
}

void UserStudioWidget::setPowerSectionHidden() { m_powerBlock->hide(); }
void UserStudioWidget::setCadSectionHidden()   { m_cadBlock->hide(); }
void UserStudioWidget::setHrSectionHidden()    { m_hrBlock->hide(); }

void UserStudioWidget::setResultTest(QString text)
{
    m_resultTest->setText(text);
    m_resultTest->show();
    m_resultTest->fadeIn(1000);
}

void UserStudioWidget::addSensor(int key, const QString &name)
{
    auto *dot = new QLabel(QStringLiteral("●"), this);
    dot->installEventFilter(this);
    m_dots->addWidget(dot);
    m_sensorDots.insert(key, dot);
    m_sensorNames.insert(key, name);
    setSensorState(key, SensorStatusStrip::State::Searching);
}

void UserStudioWidget::setSensorState(int key, SensorStatusStrip::State state)
{
    QLabel *dot = m_sensorDots.value(key, nullptr);
    if (!dot)
        return;
    m_sensorStates.insert(key, state);
    dot->setStyleSheet(QStringLiteral("font-size: 11pt; color: %1;").arg(dotColor(state)));
    dot->setCursor(needsRetry(state) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QString status;
    switch (state) {
    case SensorStatusStrip::State::Searching:  status = tr("searching"); break;
    case SensorStatusStrip::State::Connecting: status = tr("connecting"); break;
    case SensorStatusStrip::State::Connected:  status = tr("connected"); break;
    case SensorStatusStrip::State::NotFound:   status = tr("not found, click to retry"); break;
    case SensorStatusStrip::State::Lost:       status = tr("lost, click to retry"); break;
    case SensorStatusStrip::State::Disconnected: status = tr("disconnected"); break;
    }
    dot->setToolTip(QStringLiteral("%1: %2").arg(m_sensorNames.value(key), status));
    updateStatus();
}

bool UserStudioWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        for (auto it = m_sensorStates.cbegin(); it != m_sensorStates.cend(); ++it) {
            if (!needsRetry(it.value()))
                continue;
            // A dot retries its own sensor; the status line retries the first one in trouble.
            if (watched == m_status || watched == m_sensorDots.value(it.key())) {
                emit sensorRetryRequested(it.key());
                return true;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void UserStudioWidget::setPowerValue(int value)
{
    powerValue = value;
    m_power->setText(value < 0 ? QStringLiteral("-") : QString::number(value));
    updatePowerState();
}

void UserStudioWidget::setCadenceValue(int value)
{
    m_cad->setText(value < 0 ? QStringLiteral("-") : QString::number(value));
}

void UserStudioWidget::setHrValue(int value)
{
    m_hr->setText(value < 0 ? QStringLiteral("-") : QString::number(value));
}

void UserStudioWidget::setTargetPower(double percentageFTP, int range)
{
    targetPowerRange = range;
    targetPowerPct = percentageFTP;
    if (percentageFTP <= 0 || FTP < 1) {
        targetPower = -1;
        m_target->setText(QString());
        m_zone->hide();
    } else {
        targetPower = qRound(percentageFTP * FTP);
        m_target->setText(tr("target %1 W").arg(targetPower));
        const Account *account = qApp->property("Account").value<Account*>();
        const QVector<int> bounds = PowerZones::boundsPct(account ? account->power_zones_pct : QList<int>{});
        const int zone = PowerZones::zoneForPct(percentageFTP * 100.0, bounds);
        m_zone->setText(QStringLiteral("Z%1").arg(zone + 1));
        m_zone->setStyleSheet(QStringLiteral("font: 700 8pt 'Inter'; color: #141418; background: %1;"
                                             " border-radius: 7px; padding: 1px 10px;")
                                  .arg(PowerZones::color(zone).name()));
        m_zone->show();
    }
    updatePowerState();
}

void UserStudioWidget::setTargetCadence(int value, int range)
{
    targetCad = value;
    targetCadRange = range;
    updateTooltips();
}

void UserStudioWidget::setTargetHr(double percentageLTHR, int range)
{
    targetHr = percentageLTHR < 0 || LTHR < 1 ? -1 : qRound(percentageLTHR * LTHR);
    targetHrRange = range;
    updateTooltips();
}

void UserStudioWidget::updatePowerState()
{
    Target state = Target::None;
    if (!isStopped && targetPower > 0 && powerValue >= 0) {
        const int diff = powerValue - targetPower;
        state = diff > targetPowerRange ? Target::Above : diff < -targetPowerRange ? Target::Below : Target::On;
    }
    if (state == powerState)
        return;
    powerState = state;
    const QString color = state == Target::Above ? kAmber : state == Target::Below ? kRed : kWhite;
    m_power->setStyleSheet(QStringLiteral("font: 700 26pt 'Inter'; color: %1;").arg(color));
    updateStatus();
}

void UserStudioWidget::updateStatus()
{
    for (auto it = m_sensorStates.cbegin(); it != m_sensorStates.cend(); ++it) {
        if (!needsRetry(it.value()))
            continue;
        const QString what = it.value() == SensorStatusStrip::State::Lost ? tr("%1 lost").arg(m_sensorNames.value(it.key()))
                                                                          : tr("%1 not found").arg(m_sensorNames.value(it.key()));
        m_status->setText(what + QStringLiteral(" - ") + tr("click to retry"));
        m_status->setStyleSheet(QStringLiteral("font: 600 9pt 'Inter'; color: %1;").arg(kRed));
        m_status->setCursor(Qt::PointingHandCursor);
        setEdge(QStringLiteral("below"));   // a sensor in trouble shows even when on target
        return;
    }
    setEdge(powerState == Target::Above ? QStringLiteral("above")
            : powerState == Target::Below ? QStringLiteral("below") : QStringLiteral("none"));
    m_status->setCursor(Qt::ArrowCursor);
    QString text;
    QString color = kGreen;
    if (powerState == Target::Above) {
        text = tr("%1 W above target").arg(powerValue - targetPower);
        color = kAmber;
    } else if (powerState == Target::Below) {
        text = tr("%1 W below target").arg(targetPower - powerValue);
        color = kRed;
    } else if (powerState == Target::On) {
        text = tr("on target");
    }
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral("font: 600 9pt 'Inter'; color: %1;").arg(color));
}

void UserStudioWidget::setEdge(const QString &state)
{
    if (property("targetState").toString() == state)
        return;
    setProperty("targetState", state);
    style()->unpolish(this);
    style()->polish(this);
}

void UserStudioWidget::updateRideLine()
{
    QStringList parts;
    if (normalizedPower > 0)
        parts << tr("NP %1").arg(qRound(normalizedPower));
    if (qRound(tss) > 0)
        parts << tr("TSS %1").arg(qRound(tss));
    m_ride->setText(parts.join(QStringLiteral(" · ")));
}

void UserStudioWidget::updateTooltips()
{
    const QLocale locale;
    auto line = [&locale](const QString &label, double interval, double workout) {
        return QStringLiteral("%1: %2 / %3").arg(label, locale.toString(qRound(interval)), locale.toString(qRound(workout)));
    };
    m_powerBlock->setToolTip(tr("Power (W), interval / workout") + QLatin1Char('\n')
                             + line(tr("Avg"), avgIntervalPower, avgWorkoutPower) + QLatin1Char('\n')
                             + line(tr("Max"), maxIntervalPower, maxWorkoutPower)
                             + (intensityFactor > 0 ? QStringLiteral("\nIF %1").arg(locale.toString(intensityFactor, 'f', 2)) : QString())
                             + (kcal > 0 ? QStringLiteral("\n%1 kcal").arg(qRound(kcal)) : QString()));
    m_cadBlock->setToolTip((targetCad > 0 ? tr("Target %1 rpm").arg(targetCad) + QLatin1Char('\n') : QString())
                           + line(tr("Avg"), avgIntervalCad, avgWorkoutCad) + QLatin1Char('\n')
                           + line(tr("Max"), maxIntervalCad, maxWorkoutCad));
    m_hrBlock->setToolTip((targetHr > 0 ? tr("Target %1 bpm").arg(targetHr) + QLatin1Char('\n') : QString())
                          + line(tr("Avg"), avgIntervalHr, avgWorkoutHr) + QLatin1Char('\n')
                          + line(tr("Max"), maxIntervalHr, maxWorkoutHr));
}

void UserStudioWidget::setMaxIntervalPower(double val) { maxIntervalPower = val; updateTooltips(); }
void UserStudioWidget::setMaxWorkoutPower(double val)  { maxWorkoutPower = val; updateTooltips(); }
void UserStudioWidget::setAvgIntervalPower(double val) { avgIntervalPower = val; updateTooltips(); }
void UserStudioWidget::setAvgWorkoutPower(double val)  { avgWorkoutPower = val; updateTooltips(); }
void UserStudioWidget::setMaxIntervalCad(double val)   { maxIntervalCad = val; updateTooltips(); }
void UserStudioWidget::setMaxWorkoutCad(double val)    { maxWorkoutCad = val; updateTooltips(); }
void UserStudioWidget::setAvgIntervalCad(double val)   { avgIntervalCad = val; updateTooltips(); }
void UserStudioWidget::setAvgWorkoutCad(double val)    { avgWorkoutCad = val; updateTooltips(); }
void UserStudioWidget::setMaxIntervalHr(double val)    { maxIntervalHr = val; updateTooltips(); }
void UserStudioWidget::setMaxWorkoutHr(double val)     { maxWorkoutHr = val; updateTooltips(); }
void UserStudioWidget::setAvgIntervalHr(double val)    { avgIntervalHr = val; updateTooltips(); }
void UserStudioWidget::setAvgWorkoutHr(double val)     { avgWorkoutHr = val; updateTooltips(); }

void UserStudioWidget::setNormalizedPower(double val)      { normalizedPower = val; updateRideLine(); }
void UserStudioWidget::setIntensityFactor(double val)      { intensityFactor = val; updateTooltips(); }
void UserStudioWidget::setTrainingStressScore(double val)  { tss = val; updateRideLine(); }
void UserStudioWidget::setCalories(double val)             { kcal = val; updateTooltips(); }
