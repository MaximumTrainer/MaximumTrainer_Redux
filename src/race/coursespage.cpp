#include "coursespage.h"

#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSettings>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "setting_keys.h"

#include <algorithm>

namespace {

constexpr QSize kProfileThumb(200, 48);

QColor gradeColor(double grade)
{
    if (grade < 0.03) return {0x7c, 0xc4, 0x7f};
    if (grade < 0.06) return {0xe0, 0xc3, 0x41};
    if (grade < 0.09) return {0xe8, 0xa2, 0x3a};
    return {0xd6, 0x55, 0x3d};
}

/// One lap's elevation profile, coloured by gradient, for the course list.
QPixmap profileThumbnail(const RaceCourse &course, qreal devicePixelRatio)
{
    QPixmap pixmap(kProfileThumb * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);

    double low = 1e9;
    double high = -1e9;
    for (const RaceCourse::Point &point : course.points()) {
        low = std::min(low, point.elevationM);
        high = std::max(high, point.elevationM);
    }
    const double span = std::max(60.0, high - low);
    const double lap = course.lapDistanceM();
    constexpr int kBands = 50;
    for (int band = 0; band < kBands; ++band) {
        const double from = lap * band / kBands;
        const double to = lap * (band + 1) / kBands;
        const double fromElevation = course.elevationAt(from);
        const double toElevation = course.elevationAt(to);
        const double w = kProfileThumb.width();
        const double h = kProfileThumb.height() - 2;
        QPolygonF shape;
        shape << QPointF(w * band / kBands, h)
              << QPointF(w * band / kBands, h - h * (fromElevation - low) / span)
              << QPointF(w * (band + 1) / kBands, h - h * (toElevation - low) / span)
              << QPointF(w * (band + 1) / kBands, h);
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradeColor((toElevation - fromElevation) / (to - from)));
        painter.drawPolygon(shape);
    }
    return pixmap;
}

} // namespace

QIcon CoursesPage::tabIcon()
{
    constexpr int kSize = 128;
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    // A climb that steepens towards the top, one colour per section.
    const QPointF ridge[] = {{4, 112}, {30, 98}, {54, 80}, {78, 56}, {100, 30}, {124, 14}};
    const QColor colours[] = {gradeColor(0.02), gradeColor(0.04), gradeColor(0.07), gradeColor(0.10), gradeColor(0.10)};
    for (int section = 0; section < 5; ++section) {
        QPolygonF shape;
        shape << QPointF(ridge[section].x(), 124) << ridge[section] << ridge[section + 1]
              << QPointF(ridge[section + 1].x(), 124);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colours[section]);
        painter.drawPolygon(shape);
    }
    painter.setPen(QPen(QColor(0x30, 0x30, 0x30), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(ridge, 6);
    // The flag at the top.
    painter.setPen(QPen(QColor(0x30, 0x30, 0x30), 4));
    painter.drawLine(QPointF(118, 16), QPointF(118, 0 + 2));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xf0, 0x8a, 0x44));
    painter.drawPolygon(QPolygonF({QPointF(118, 2), QPointF(98, 8), QPointF(118, 14)}));
    return QIcon(pixmap);
}

double CoursesPage::climbM(const RaceCourse &course)
{
    double climb = 0.0;
    const QVector<RaceCourse::Point> &points = course.points();
    for (qsizetype index = 1; index < points.size(); ++index)
        climb += std::max(0.0, points.at(index).elevationM - points.at(index - 1).elevationM);
    return climb * course.laps();
}

QVector<RaceCourse> CoursesPage::bundledCourses()
{
    QVector<RaceCourse> courses;
    const QDir dir(QStringLiteral(":/courses"));
    for (const QString &file : dir.entryList({QStringLiteral("*.json")}, QDir::Files))
        if (auto course = RaceCourse::fromFile(dir.filePath(file)))
            courses.append(*course);
    std::sort(courses.begin(), courses.end(), [](const RaceCourse &a, const RaceCourse &b) {
        return climbM(a) / a.totalDistanceM() < climbM(b) / b.totalDistanceM();
    });
    return courses;
}

CoursesPage::CoursesPage(QWidget *parent)
    : QWidget(parent), m_courses(bundledCourses())
{
    setObjectName(QStringLiteral("coursesPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto *title = new QLabel(tr("Courses"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *intro = new QLabel(tr("Ride a real road against bots. About a third of them, the \"Pace\" bots, "
                                "ride together at 3.0 W/kg: sit on their wheels to draft. Your trainer "
                                "follows the road at 50 % difficulty."), this);
    intro->setWordWrap(true);
    intro->setMaximumWidth(640);
    layout->addWidget(intro);

    m_courseList = new QListWidget(this);
    m_courseList->setObjectName(QStringLiteral("coursesList"));
    m_courseList->setIconSize(kProfileThumb);
    m_courseList->setSpacing(4);
    m_courseList->setMaximumWidth(640);
    QFont itemFont = m_courseList->font();
    itemFont.setPointSizeF(itemFont.pointSizeF() + 0.5);
    for (const RaceCourse &course : std::as_const(m_courses)) {
        QString text = QStringLiteral("%1\n%2\n").arg(course.name(), course.place());
        text += tr("%1 km · %2 m of climbing").arg(course.totalDistanceM() / 1000.0, 0, 'f', 1)
                                                .arg(qRound(climbM(course)));
        if (course.isLoop())
            text += tr(" · %1 laps of %2 km").arg(course.laps()).arg(course.lapDistanceM() / 1000.0, 0, 'f', 1);
        auto *item = new QListWidgetItem(QIcon(profileThumbnail(course, devicePixelRatioF())), text, m_courseList);
        item->setFont(itemFont);
    }
    m_courseList->setCurrentRow(0);
    layout->addWidget(m_courseList, 1);

    auto *settingsForm = new QFormLayout();
    auto *weightRow = new QHBoxLayout();
    m_weight = new QDoubleSpinBox(this);
    m_weight->setObjectName(QStringLiteral("coursesWeight"));
    m_weight->setRange(30.0, 200.0);
    m_weight->setDecimals(1);
    m_weight->setSuffix(tr(" kg"));
    m_weight->setMinimumWidth(100);
    auto *weightNote = new QLabel(tr("From your profile. Correct it if it has changed: it sets your speed on the climbs."), this);
    weightNote->setWordWrap(true);
    weightRow->addWidget(m_weight);
    weightRow->addWidget(weightNote, 1);
    settingsForm->addRow(tr("Your weight"), weightRow);

    namespace Keys = SettingKeys::Courses;
    const QSettings settings;
    m_botCount = new QSpinBox(this);
    m_botCount->setObjectName(QStringLiteral("coursesBotCount"));
    m_botCount->setRange(1, RaceBot::kMaxBots);
    m_botCount->setValue(AppSettings::read(settings, Keys::Bots));
    m_botCount->setMinimumWidth(100);
    auto wattsPerKgBox = [this](const QString &objectName, double value) {
        auto *box = new QDoubleSpinBox(this);
        box->setObjectName(objectName);
        box->setRange(1.0, 7.0);
        box->setDecimals(1);
        box->setSingleStep(0.1);
        box->setSuffix(tr(" W/kg"));
        box->setValue(value);
        box->setMinimumWidth(100);
        return box;
    };
    m_minWattsPerKg = wattsPerKgBox(QStringLiteral("coursesMinWkg"), AppSettings::read(settings, Keys::MinWattsPerKg));
    m_maxWattsPerKg = wattsPerKgBox(QStringLiteral("coursesMaxWkg"), AppSettings::read(settings, Keys::MaxWattsPerKg));
    auto *rangeRow = new QHBoxLayout();
    rangeRow->addWidget(m_minWattsPerKg);
    rangeRow->addWidget(new QLabel(tr("to"), this));
    rangeRow->addWidget(m_maxWattsPerKg);
    rangeRow->addStretch(1);
    auto *countRow = new QHBoxLayout();
    countRow->addWidget(m_botCount);
    countRow->addStretch(1);
    settingsForm->addRow(tr("Bots"), countRow);
    settingsForm->addRow(tr("Bot strength"), rangeRow);
    layout->addLayout(settingsForm);
    // Keep the range the right way round as either end is edited.
    connect(m_minWattsPerKg, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (m_maxWattsPerKg->value() < value) m_maxWattsPerKg->setValue(value);
    });
    connect(m_maxWattsPerKg, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (m_minWattsPerKg->value() > value) m_minWattsPerKg->setValue(value);
    });

    m_ride = new QPushButton(tr("Ride with bots"), this);
    m_ride->setObjectName(QStringLiteral("coursesRide"));
    m_ride->setEnabled(!m_courses.isEmpty());
    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_ride);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    connect(m_ride, &QPushButton::clicked, this, [this]() {
        const int row = m_courseList->currentRow();
        if (row < 0 || row >= m_courses.size())
            return;
        const RaceBot::FieldSettings bots{m_botCount->value(), m_minWattsPerKg->value(), m_maxWattsPerKg->value()};
        QSettings settings;
        AppSettings::write(settings, SettingKeys::Courses::Bots, bots.count);
        AppSettings::write(settings, SettingKeys::Courses::MinWattsPerKg, bots.minWattsPerKg);
        AppSettings::write(settings, SettingKeys::Courses::MaxWattsPerKg, bots.maxWattsPerKg);
        emit rideRequested(m_courses.at(row), m_weight->value(), bots);
    });
    connect(m_courseList, &QListWidget::itemDoubleClicked, m_ride, &QPushButton::click);
}

void CoursesPage::setProfileWeightKg(double weightKg)
{
    m_weight->setValue(weightKg);
}
