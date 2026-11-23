#include "coursespage.h"

#include "racesimulation.h"

#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSettings>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>
#include <QTreeWidget>
#include <QPainter>
#include <QVBoxLayout>

#include "setting_keys.h"

#include <algorithm>

namespace {

constexpr QSize kProfileThumb(200, 48);

enum Column { ProfileColumn, NameColumn, PlaceColumn, LengthColumn, ClimbColumn, TimeColumn };
constexpr double kEstimateShareOfFtp = 0.70;
constexpr int kCourseIndexRole = Qt::UserRole;
constexpr int kSortValueRole = Qt::UserRole + 1;

/// Sorts the number columns by value, not by their text.
class CourseItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : NameColumn;
        if (column == LengthColumn || column == ClimbColumn || column == TimeColumn)
            return data(column, kSortValueRole).toDouble() < other.data(column, kSortValueRole).toDouble();
        return QTreeWidgetItem::operator<(other);
    }
};

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

    auto *intro = new QLabel(tr("Ride a real road against bots."), this);
    layout->addWidget(intro);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("coursesSearch"));
    m_search->setPlaceholderText(tr("Search courses"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_courseList = new QTreeWidget(this);
    m_courseList->setObjectName(QStringLiteral("coursesList"));
    m_courseList->setRootIsDecorated(false);
    m_courseList->setUniformRowHeights(true);
    m_courseList->setIconSize(kProfileThumb);
    m_courseList->setHeaderLabels({tr("Profile"), tr("Course"), tr("Where"), tr("Length (km)"), tr("Climbing (m)"),
                                   tr("Est. time")});
    for (int index = 0; index < m_courses.size(); ++index) {
        const RaceCourse &course = m_courses.at(index);
        auto *item = new CourseItem(m_courseList);
        item->setIcon(ProfileColumn, QIcon(profileThumbnail(course, devicePixelRatioF())));
        item->setData(ProfileColumn, kCourseIndexRole, index);
        QString name = course.name();
        if (course.isLoop())
            name += tr(" (%1 laps)").arg(course.laps());
        item->setText(NameColumn, name);
        item->setText(PlaceColumn, course.place());
        item->setText(LengthColumn, QString::number(course.totalDistanceM() / 1000.0, 'f', 1));
        item->setData(LengthColumn, kSortValueRole, course.totalDistanceM());
        item->setText(ClimbColumn, QString::number(qRound(climbM(course))));
        item->setData(ClimbColumn, kSortValueRole, climbM(course));
        item->setTextAlignment(LengthColumn, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(ClimbColumn, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(TimeColumn, Qt::AlignRight | Qt::AlignVCenter);
        item->setSizeHint(ProfileColumn, kProfileThumb + QSize(8, 8));
    }
    QHeaderView *header = m_courseList->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(ProfileColumn, QHeaderView::Fixed);
    header->resizeSection(ProfileColumn, kProfileThumb.width() + 12);
    header->setSectionResizeMode(NameColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(PlaceColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(LengthColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ClimbColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(TimeColumn, QHeaderView::ResizeToContents);
    m_courseList->setSortingEnabled(true);
    m_courseList->sortByColumn(ClimbColumn, Qt::AscendingOrder);
    m_courseList->setCurrentItem(m_courseList->topLevelItem(0));
    layout->addWidget(m_courseList, 1);

    auto *settingsForm = new QFormLayout();
    m_weight = new QLabel(this);
    m_weight->setObjectName(QStringLiteral("coursesWeight"));
    m_weight->setTextFormat(Qt::RichText);
    m_weight->setOpenExternalLinks(true);
    m_weight->setWordWrap(true);
    settingsForm->addRow(tr("Your weight"), m_weight);
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

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        const QString needle = text.trimmed();
        for (int row = 0; row < m_courseList->topLevelItemCount(); ++row) {
            QTreeWidgetItem *item = m_courseList->topLevelItem(row);
            const RaceCourse &course = m_courses.at(item->data(ProfileColumn, kCourseIndexRole).toInt());
            const bool match = needle.isEmpty()
                || course.name().contains(needle, Qt::CaseInsensitive)
                || course.place().contains(needle, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });
    // Double-click or Enter starts the course.
    connect(m_courseList, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const int row = item->data(ProfileColumn, kCourseIndexRole).toInt();
        if (row < 0 || row >= m_courses.size())
            return;
        const RaceBot::FieldSettings bots{m_botCount->value(), m_minWattsPerKg->value(), m_maxWattsPerKg->value()};
        QSettings settings;
        AppSettings::write(settings, SettingKeys::Courses::Bots, bots.count);
        AppSettings::write(settings, SettingKeys::Courses::MinWattsPerKg, bots.minWattsPerKg);
        AppSettings::write(settings, SettingKeys::Courses::MaxWattsPerKg, bots.maxWattsPerKg);
        emit rideRequested(m_courses.at(row), bots);
    });
}

double CoursesPage::estimatedSeconds(const RaceCourse &course, double watts, double riderKg, double bikeKg, double cda)
{
    constexpr double kStepSec = 1.0;
    constexpr double kGiveUpSec = 12 * 3600.0;
    RaceSimulation solo(course, QString(), riderKg, bikeKg, cda);
    solo.setYourPower(watts);
    while (!solo.youFinished() && solo.elapsedSec() < kGiveUpSec)
        solo.step(kStepSec);
    return solo.you().finishTimeSec.value_or(kGiveUpSec);
}

void CoursesPage::setProfile(int ftpWatts, double weightKg, double bikeKg, double cda)
{
    m_weight->setText(tr("%1 kg, from your Intervals.icu profile. To change it, update it in your "
                         "<a style=\"color:#6ca8ff\" href=\"https://intervals.icu/settings\">Intervals.icu settings</a> and restart the app.")
                          .arg(weightKg, 0, 'f', 1));
    const double watts = kEstimateShareOfFtp * ftpWatts;
    const QString how = tr("Your time riding alone at 70 % of your FTP (%1 W), with your weight (%2 kg) and "
                           "your bike. Bots and drafting are not counted.")
                            .arg(qRound(watts)).arg(weightKg, 0, 'f', 1);
    m_courseList->headerItem()->setToolTip(TimeColumn, how);
    for (int row = 0; row < m_courseList->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = m_courseList->topLevelItem(row);
        const RaceCourse &course = m_courses.at(item->data(ProfileColumn, kCourseIndexRole).toInt());
        if (ftpWatts <= 0 || weightKg <= 0.0) {
            item->setText(TimeColumn, QStringLiteral("-"));
            continue;
        }
        const int minutes = qRound(estimatedSeconds(course, watts, weightKg, bikeKg, cda) / 60.0);
        item->setText(TimeColumn, minutes >= 60 ? tr("%1h%2min").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'))
                                                : tr("%1 min").arg(minutes));
        item->setData(TimeColumn, kSortValueRole, minutes);
        item->setToolTip(TimeColumn, how);
    }
}
