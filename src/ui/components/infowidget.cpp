#include "infowidget.h"
#include "ui_infowidget.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include "account.h"
#include "myconstants.h"
#include "powerzones.h"
#include "util.h"


InfoWidget::~InfoWidget() {
    delete ui;
}


InfoWidget::InfoWidget(QWidget *parent) : QWidget(parent), ui(new Ui::InfoWidget) {
    ui->setupUi(this);

    // Typography scale (font rules only — the target-state frame colours and
    // the light/dark palette keep coming from the theme): hero live value,
    // medium secondary stats, small muted captions.
    setStyleSheet(QStringLiteral(R"(
        QLabel#label_currentValue { font: 700 28pt 'Inter'; }
        QLabel#label_range { font: 600 9pt 'Inter'; }
        QLabel#infoCaption { font: 600 8pt 'Inter'; color: #9a9aa6; letter-spacing: 1px; }
        QLabel#infoTargetPill { font: 8pt 'Inter'; border: 1px solid #44444e; border-radius: 8px; padding: 0px 6px; }
        QLabel#infoUnit { font: 10pt 'Inter'; color: #9a9aa6; }
        QLabel#label_avgInterval, QLabel#label_maxInterval,
        QLabel#label_avgWorkout,  QLabel#label_maxWorkout { font: 600 13pt 'Inter'; }
        QLabel#label_WorkoutTxt, QLabel#label_AvgIntervalTxt,
        QLabel#label_AverageTxt, QLabel#label_MaximumTxt,
        QLabel#label_trainerSpeedTxt { font: 600 8pt 'Inter'; color: #8d8d8d; }
    )"));

    useMile = false;

    target = -1;
    ui->label_currentValue->setText(QStringLiteral("0"));
    ui->label_range->setText("");

    ui->label_range->setVisible(false);
    ui->label_targetValue->setVisible(false);
    ui->label_imageTarget->setVisible(false);
    ui->label_trainerSpeedTxt->setVisible(false);


    isStopped = true;
    currentColor = 2;

    // Card layout: a title row (small icon, caption, target tag), then the
    // value with its unit and a "below / above target" hint, then Avg / Max.
    auto *grid = ui->gridLayout_2;
    grid->removeWidget(ui->label_image);
    auto *header = new QHBoxLayout();
    header->setSpacing(6);
    m_caption = new QLabel(this);
    m_caption->setObjectName(QStringLiteral("infoCaption"));
    m_targetPill = new QLabel(this);
    m_targetPill->setObjectName(QStringLiteral("infoTargetPill"));
    m_targetPill->hide();
    m_zonePill = new QLabel(this);
    m_zonePill->hide();
    ui->label_image->setFixedSize(18, 18);
    ui->label_image->setScaledContents(true);
    header->addWidget(ui->label_image);
    header->addWidget(m_caption);
    header->addStretch();
    header->addWidget(m_zonePill);
    header->addWidget(m_targetPill);
    ui->verticalLayout->insertLayout(0, header);

    ui->label_imageTarget->hide();
    ui->label_targetValue->hide();
    ui->label_spacerRow->hide();
    grid->removeWidget(ui->label_currentValue);
    grid->removeWidget(ui->label_range);
    m_unit = new QLabel(this);
    m_unit->setObjectName(QStringLiteral("infoUnit"));
    auto *valueRow = new QHBoxLayout();
    valueRow->setSpacing(4);
    valueRow->addWidget(ui->label_currentValue, 0, Qt::AlignBottom);
    valueRow->addWidget(m_unit, 0, Qt::AlignBottom);
    valueRow->addStretch();
    m_valueRow = valueRow;
    grid->addLayout(valueRow, 0, 0, 1, 3);
    grid->addWidget(ui->label_range, 1, 0, 1, 3);
}


//    ///Move Icon and value 1 column to the right
//    QLabel *labelIcon = ui->label_image;
//    QLabel *labelValue = ui->label_currentValue;
//    ui->gridLayout->addWidget(labelIcon,0,3,1,1, Qt::AlignLeft);
//    ui->gridLayout->addWidget(labelValue,0,4,1,1, Qt::AlignLeft);


void InfoWidget::setUserData(double FTP, double LTHR) {
    this->FTP = FTP;
    this->LTHR = LTHR;
}


///Only for speed widget
void InfoWidget::setValue(double v) {

    // Rowing (Beta): the speed channel carries the pace-equivalent km/h from
    // the rower; show it as time per 500 m instead of a speed.
    if (rowingMode && type == InfoWidget::SPEED) {
        if (v <= 0.5) {
            ui->label_currentValue->setText(QStringLiteral("-"));
            return;
        }
        const int paceSec = qRound(1800.0 / v);
        ui->label_currentValue->setText(QStringLiteral("%1:%2")
            .arg(paceSec / 60)
            .arg(paceSec % 60, 2, 10, QLatin1Char('0')));
        return;
    }

    if (v == 0) {
        ui->label_currentValue->setText(QStringLiteral("0"));
    }
    else if(v < 0) {
        ui->label_currentValue->setText(QStringLiteral("-"));
    }
    else {
        if (useMile)
            v = v* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

        ui->label_currentValue->setText(QString::number(v, 'f', 1));
    }
}

///Only for speed widget - trainer Speed in M/S
/// //////////////////////////////////////////////////////////////////////////////////////////////////
void InfoWidget::setTrainerSpeed(double v) {

    if (v == 0) {
        ui->label_trainerSpeed->setText("0");
    }
    else if(v < 0) {
        ui->label_trainerSpeed->setText("-");
    }
    else {
        if (useMile)
            v = v* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

        ui->label_trainerSpeed->setText("" + QString::number(v, 'f', 1));


    }

}


void InfoWidget::setValue( int value ) {


    if(value < 0) {
        ui->label_currentValue->setText(QStringLiteral("-"));
        return;
    }

    ui->label_currentValue->setText(QString::number(value));


    const int diff = value - target;
    if (target < 0 || isStopped)
        ui->label_range->setText(QString());
    else if (diff > targetRange)
        ui->label_range->setText(tr("%1 %2 above target").arg(diff).arg(unitText()));
    else if (diff < -targetRange)
        ui->label_range->setText(tr("%1 %2 below target").arg(-diff).arg(unitText()));
    else
        ui->label_range->setText(tr("on target"));


    // -1 below, 1 above, 0 on target, 2 no target / stopped. The card around
    // the box shows it; the value colour repeats it.
    int state = 0;
    if (isStopped || target < 0)
        state = 2;
    else if (diff < -targetRange)
        state = -1;
    else if (diff > targetRange)
        state = 1;
    if (state != currentColor) {
        currentColor = state;
        const QString color = state == -1 ? QStringLiteral("#e5534b")
                            : state == 1  ? QStringLiteral("#f2a33a")
                            : state == 0  ? QStringLiteral("#3fb27f") : QString();
        ui->label_currentValue->setStyleSheet(state == -1 || state == 1
                                              ? QStringLiteral("color: %1;").arg(color) : QString());
        ui->label_range->setStyleSheet(color.isEmpty() ? QString() : QStringLiteral("color: %1;").arg(color));
        emit targetStateChanged(state);
    }
}


void InfoWidget::targetChanged(double percentageValue, int range) {


    ui->label_range->setVisible(percentageValue >= 0);


    int realTarget = percentageValue;
    if (type == InfoWidget::POWER) {
        realTarget = qRound(percentageValue * FTP);
    }
    else if (type == InfoWidget::HEART_RATE) {
        realTarget = qRound(percentageValue * LTHR);
    }

    ui->label_targetValue->setText(QString::number(realTarget));
    if (percentageValue >= 0) {
        m_targetPill->setText(range > 0 ? tr("Target %1 %2 ±%3").arg(realTarget).arg(unitText()).arg(range)
                                        : tr("Target %1 %2").arg(realTarget).arg(unitText()));
    }
    m_targetPill->setVisible(percentageValue >= 0);
    updateZonePill(type == InfoWidget::POWER ? percentageValue : -1);


    this->target = realTarget;
    this->targetRange = range;
}


void InfoWidget::maxIntervalChanged(double avg) {

    if (type == InfoWidget::SPEED) {

        if (useMile) {
            avg = avg* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;
        }

        QString text =  locale.toString(avg, 'f', 1);
        ui->label_maxInterval->setText(text);
    }
    else {
        ui->label_maxInterval->setText(QString::number(qRound(avg)));
    }

}
void InfoWidget::maxWorkoutChanged(double avg) {

    if (type == InfoWidget::SPEED) {

        if (useMile)
            avg = avg* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

        QString text =  locale.toString(avg, 'f', 1);
        ui->label_maxWorkout->setText(text);
    }
    else {
        ui->label_maxWorkout->setText(QString::number(qRound(avg)));
    }
}
void InfoWidget::avgIntervalChanged(double avg) {

    if (useMile)
        avg = avg* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

    QString text =  locale.toString(avg, 'f', 1);
    ui->label_avgInterval->setText(text);

}
void InfoWidget::avgWorkoutChanged(double avg) {

    if (useMile)
        avg = avg* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

    QString text =  locale.toString(avg, 'f', 1);
    ui->label_avgWorkout->setText(text);
}


void InfoWidget::setTypeInfoBox(TypeInfoBox type) {


    this->type = type;

    int size = 18;
    const qreal dpr = devicePixelRatioF();
    QPixmap pixmapPower   = Util::loadIconForDpr(":/image/icon/power2", size, dpr);
    QPixmap pixmapCadence = Util::loadIconForDpr(":/image/icon/crank2", size, dpr);
    QPixmap pixmapSpeed   = Util::loadIconForDpr(":/image/icon/speed",  size, dpr);
    QPixmap pixmapHr      = Util::loadIconForDpr(":/image/icon/heart2", size, dpr);
    QPixmap pixmapTarget  = Util::loadIconForDpr(":/image/icon/target", size - 10, dpr);


    ui->label_imageTarget->setPixmap(pixmapTarget);
    ui->label_imageTarget->setToolTip(tr("Target"));
    //    ui->label_imageTarget->setP margin left

    m_caption->setText(type == InfoWidget::HEART_RATE ? tr("HEART RATE")
                       : type == InfoWidget::SPEED    ? tr("SPEED")
                       : type == InfoWidget::CADENCE  ? tr("CADENCE")
                                                      : tr("POWER"));
    refreshUnit();

    if (type == InfoWidget::HEART_RATE) {
        ui->label_currentValue->setToolTip(tr("Heart Rate - bpm"));
        ui->label_targetValue->setToolTip(tr("Target Heart Rate - bpm"));
        ui->label_range->setToolTip(tr("Difference from Target Heart Rate - bpm"));

        ui->label_image->setPixmap(pixmapHr);
        ui->label_image->setToolTip(tr("Heart Rate - bpm"));
    }
    else if (type == InfoWidget::SPEED) {
        ui->label_image->setStyleSheet("image: url(:/image/icon/speed)");
        ui->label_image->setToolTip(tr("Speed - km/h"));
        ui->label_currentValue->setToolTip(tr("Speed - km/h"));

        ui->label_image->setPixmap(pixmapSpeed);
        ui->label_image->setToolTip(tr("Speed - km/h"));

        ui->label_trainerSpeedTxt->setVisible(true);
    }
    else if (type == InfoWidget::CADENCE) {
        ui->label_currentValue->setToolTip(tr("Cadence - rpm"));
        ui->label_targetValue->setToolTip(tr("Target Cadence - rpm"));
        ui->label_range->setToolTip(tr("Difference from Target Cadence - rpm"));

        ui->label_image->setPixmap(pixmapCadence);
        ui->label_image->setToolTip(tr("Cadence - rpm"));
    }
    else if (type == InfoWidget::POWER) {
        ui->label_currentValue->setToolTip(tr("Power - watts"));
        ui->label_targetValue->setToolTip(tr("Target Power - watts"));
        ui->label_range->setToolTip(tr("Difference from Target Power - watts"));

        ui->label_image->setPixmap(pixmapPower);
        ui->label_image->setToolTip(tr("Power - watts"));
    }

}

void InfoWidget::setRowingMode(bool rowing) {
    rowingMode = rowing;
    if (m_caption && rowing && type == InfoWidget::CADENCE)
        m_caption->setText(tr("STROKE RATE"));
    if (m_caption && rowing && type == InfoWidget::SPEED)
        m_caption->setText(tr("PACE"));
    refreshUnit();
    if (!rowing)
        return;
    // Rowing (Beta): the cadence channel carries stroke rate and the speed
    // channel carries the pace equivalent — relabel the tooltips to match.
    if (type == InfoWidget::CADENCE) {
        ui->label_currentValue->setToolTip(tr("Stroke Rate - spm"));
        ui->label_targetValue->setToolTip(tr("Target Stroke Rate - spm"));
        ui->label_range->setToolTip(tr("Difference from Target Stroke Rate - spm"));
        ui->label_image->setToolTip(tr("Stroke Rate - spm"));
    }
    else if (type == InfoWidget::SPEED) {
        ui->label_currentValue->setToolTip(tr("Pace - per 500 m"));
        ui->label_image->setToolTip(tr("Pace - per 500 m"));
    }
}


void InfoWidget::setStopped(bool b) {
    isStopped = b;
}

void InfoWidget::setUseMiles(bool b) {
    useMile = b;
    refreshUnit();
    ui->label_image->setStyleSheet("image: url(:/image/icon/speed)");
    ui->label_image->setToolTip(tr("Speed - mph"));
    ui->label_currentValue->setToolTip(tr("Speed - mph"));
}

void InfoWidget::setTrainerSpeedVisible(bool b) {
    ui->label_trainerSpeed->setVisible(b);
    ui->label_trainerSpeedTxt->setVisible(b);
}


QString InfoWidget::unitText() const
{
    switch (type) {
    case InfoWidget::POWER:      return QStringLiteral("W");
    case InfoWidget::HEART_RATE: return QStringLiteral("bpm");
    case InfoWidget::CADENCE:    return rowingMode ? QStringLiteral("spm") : QStringLiteral("rpm");
    case InfoWidget::SPEED:      return rowingMode ? QStringLiteral("/500m")
                                      : useMile ? QStringLiteral("mph") : QStringLiteral("km/h");
    default:                     return QString();
    }
}

void InfoWidget::refreshUnit()
{
    if (m_unit)
        m_unit->setText(unitText());
}

// The power target's zone, in the zone's colour (same zones as the graphs).
void InfoWidget::updateZonePill(double targetFraction)
{
    if (targetFraction <= 0) {
        m_zonePill->hide();
        return;
    }
    const auto *account = qApp->property("Account").value<Account *>();
    const QVector<int> bounds = PowerZones::boundsPct(account ? account->power_zones_pct : QList<int>{});
    const int zone = PowerZones::zoneForPct(targetFraction * 100.0, bounds);
    m_zonePill->setText(PowerZones::label(zone, int(bounds.size()) + 1,
                                          account ? account->power_zone_names : QStringList{}));
    m_zonePill->setStyleSheet(QStringLiteral("QLabel { font: bold 8pt 'Inter'; color: #141418; background: %1;"
                                             " border-radius: 8px; padding: 0px 6px; }")
                                  .arg(PowerZones::color(zone).name()));
    m_zonePill->show();
}

void InfoWidget::setStatsVisible(bool visible) {

    for (QWidget *label : std::initializer_list<QWidget *>{
             ui->label_AverageTxt, ui->label_MaximumTxt,
             ui->label_AvgIntervalTxt, ui->label_avgInterval, ui->label_maxInterval,
             ui->label_WorkoutTxt, ui->label_avgWorkout, ui->label_maxWorkout})
        label->setVisible(visible);
    // The .ui minimum height makes room for the rows.
    setMinimumHeight(visible ? 140 : 0);
}


void InfoWidget::setSecondaryReading(const QString &text, const QColor &color)
{
    if (!m_secondaryReading) {
        m_secondaryReading = new QLabel(this);
        m_secondaryReading->setObjectName(QStringLiteral("infoSecondaryReading"));
        // Same row as the value, right of the unit: no extra height in a short pane.
        m_valueRow->insertSpacing(2, 14);
        m_valueRow->insertWidget(3, m_secondaryReading, 0, Qt::AlignBottom);
    }
    m_secondaryReading->setStyleSheet(QStringLiteral("font: 600 9pt 'Inter'; color: %1; background: transparent;")
                                          .arg(color.name()));
    m_secondaryReading->setText(text);
    m_secondaryReading->setVisible(!text.isEmpty());
}

QString InfoWidget::secondaryReading() const
{
    return m_secondaryReading ? m_secondaryReading->text() : QString();
}
