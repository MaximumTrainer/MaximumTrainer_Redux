#include "raceview.h"

#include "powerzones.h"
#include "racesimulation.h"

#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace {

const QColor kYou(0xf0, 0x8a, 0x44);
const QColor kRider(0x7f, 0x9f, 0xd6);
const QColor kDraft(0x4c, 0xc0, 0x8f);
const QColor kProgress(0x4c, 0xc0, 0x8f);
// Fixed, not from the palette: the workout window's theme makes "mid" nearly
// the background colour, and the road vanished.
const QColor kRoad(0x5b, 0x62, 0x6c);
const QColor kSprint(0xef, 0x4b, 0x3f);
const QColor kGradeEasy(0x7c, 0xc4, 0x7f);
const QColor kGradeMid(0xe0, 0xc3, 0x41);
const QColor kGradeHard(0xe8, 0xa2, 0x3a);
const QColor kGradeSteep(0xd6, 0x55, 0x3d);

constexpr double kStandingsWidth = 300.0;
constexpr double kRowHeight = 30.0;
constexpr double kNowCardWidth = 600.0;
constexpr double kNowCardHeight = 172.0;
constexpr double kRoadWidthPx = 80.0;   // four riders abreast
constexpr double kNameRangeM = 30.0;   // riders this close get a name tag, where it fits
constexpr double kZoomHalfM = 90.0;    // the map shows 180 m around you
constexpr double kProfileMinSegmentM = 100.0;
constexpr double kProfileBands = 80.0;
constexpr double kMetresPerDegreeLat = 110540.0;
constexpr double kMetresPerDegreeLonAtEquator = 111320.0;

QString clock(double seconds)
{
    const int total = static_cast<int>(std::lround(std::abs(seconds)));
    const int hours = total / 3600;
    const int minutes = (total / 60) % 60;
    const int secs = total % 60;
    if (hours > 0)
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(minutes).arg(secs, 2, 10, QLatin1Char('0'));
}

QString gapText(double seconds)
{
    if (std::abs(seconds) < 0.5)
        return QStringLiteral("0:00");
    return (seconds < 0 ? QStringLiteral("−") : QStringLiteral("+")) + clock(seconds);
}

QString ordinal(int position)
{
    const int lastTwo = position % 100;
    if (lastTwo >= 11 && lastTwo <= 13)
        return QStringLiteral("%1th").arg(position);
    switch (position % 10) {
    case 1: return QStringLiteral("%1st").arg(position);
    case 2: return QStringLiteral("%1nd").arg(position);
    case 3: return QStringLiteral("%1rd").arg(position);
    default: return QStringLiteral("%1th").arg(position);
    }
}

} // namespace

RaceView::RaceView(const RaceSimulation *race, QWidget *parent)
    : QWidget(parent), m_race(race)
{
    setObjectName(QStringLiteral("raceView"));
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(560, 320);

    const QVector<RaceCourse::Point> &points = m_race->course().points();
    const double originLat = points.constFirst().latitude;
    const double originLon = points.constFirst().longitude;
    const double lonScale = kMetresPerDegreeLonAtEquator * std::cos(qDegreesToRadians(originLat));
    m_projected.reserve(points.size());
    for (const RaceCourse::Point &point : points) {
        const QPointF projected((point.longitude - originLon) * lonScale,
                                (point.latitude - originLat) * kMetresPerDegreeLat);
        m_projected.append(projected);
        m_projectedBounds = m_projectedBounds.isNull() ? QRectF(projected, QSizeF(0.001, 0.001))
                                                       : m_projectedBounds.united(QRectF(projected, QSizeF(0.001, 0.001)));
    }
}

void RaceView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

QColor RaceView::gradeColor(double grade) const
{
    if (grade < 0.03)
        return kGradeEasy;
    if (grade < 0.06)
        return kGradeMid;
    if (grade < 0.09)
        return kGradeHard;
    return kGradeSteep;
}

QPointF RaceView::project(double raceDistanceM) const
{
    const RaceCourse &course = m_race->course();
    const RaceCourse::Point point = course.pointAt(std::max(0.0, raceDistanceM));
    const RaceCourse::Point &origin = course.points().constFirst();
    const double lonScale = kMetresPerDegreeLonAtEquator * std::cos(qDegreesToRadians(origin.latitude));
    return {(point.longitude - origin.longitude) * lonScale, (point.latitude - origin.latitude) * kMetresPerDegreeLat};
}

QRectF RaceView::zoomBounds() const
{
    // A fixed window centred on you: your dot stays still while the road and
    // the riders around you move under it.
    const QPointF centre = project(std::max(0.0, m_race->you().distanceM));
    return {centre.x() - kZoomHalfM, centre.y() - kZoomHalfM, 2 * kZoomHalfM, 2 * kZoomHalfM};
}

QPointF RaceView::routePoint(double raceDistanceM, const QRectF &area, const QRectF &bounds, double margin) const
{
    const QPointF projected = project(raceDistanceM);
    const double scale = std::min((area.width() - 2 * margin) / bounds.width(),
                                  (area.height() - 2 * margin) / bounds.height());
    const QPointF centre = area.center();
    const QPointF boundsCentre = bounds.center();
    return {centre.x() + (projected.x() - boundsCentre.x()) * scale,
            centre.y() - (projected.y() - boundsCentre.y()) * scale};
}

void RaceView::paintEvent(QPaintEvent *)
{

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().color(QPalette::Window));

    updateDisplayOrder();

    const QRectF all = rect();
    const double profileHeight = std::clamp(all.height() * 0.28, 110.0, 180.0);
    const QRectF profile(all.left(), all.bottom() - profileHeight, all.width(), profileHeight);
    const double middleTop = all.top();
    const double middleHeight = profile.top() - middleTop;
    const double standingsWidth = std::min(kStandingsWidth, all.width() * 0.38);
    const QRectF map(all.left(), middleTop, all.width() - standingsWidth, middleHeight);
    const QRectF standings(map.right(), middleTop, standingsWidth, middleHeight);

    paintRoute(painter, map);
    paintProgressPanel(painter, map);
    paintNowPanel(painter, map);
    paintZonePanel(painter, map);
    paintGearPanel(painter, map);
    paintStandings(painter, standings);
    paintProfile(painter, profile);
    if (m_race->youFinished())
        paintFinish(painter, map);
}

void RaceView::paintProgressPanel(QPainter &painter, const QRectF &mapArea) const
{
    // Lap, distance and how much of the course is done, top left above the
    // small map.
    const RaceCourse &course = m_race->course();
    const double done = std::clamp(m_race->you().distanceM, 0.0, course.totalDistanceM());
    const double share = done / course.totalDistanceM();
    const QRectF panel = progressPanelRect(mapArea);
    QColor fill = palette().color(QPalette::Base);
    fill.setAlpha(240);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(panel, 8, 8);

    QFont nameFont = font();
    nameFont.setBold(true);
    painter.setFont(nameFont);
    painter.setPen(palette().color(QPalette::Text));
    const QRectF nameRow(panel.left() + 12, panel.top() + 6, panel.width() - 24, 20);
    painter.drawText(nameRow, Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(nameFont).elidedText(course.name(), Qt::ElideRight, static_cast<int>(nameRow.width())));

    QFont big = font();
    big.setBold(true);
    big.setPointSizeF(font().pointSizeF() * 1.35);
    painter.setFont(big);
    const QRectF top(panel.left() + 12, nameRow.bottom() + 2, panel.width() - 24, 26);
    if (course.isLoop()) {
        const int lap = std::min(course.laps(), static_cast<int>(done / course.lapDistanceM()) + 1);
        painter.drawText(top, Qt::AlignLeft | Qt::AlignVCenter, tr("Lap %1/%2").arg(lap).arg(course.laps()));
    }
    painter.drawText(top, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1 %").arg(qRound(share * 100)));

    const QRectF bar(panel.left() + 12, top.bottom() + 6, panel.width() - 24, 9);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Mid));
    painter.drawRoundedRect(bar, 4.5, 4.5);
    painter.setBrush(kProgress);
    painter.drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * share, bar.height()), 4.5, 4.5);

    painter.setFont(font());
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(QRectF(panel.left() + 12, bar.bottom() + 4, panel.width() - 24, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     tr("%1 / %2 km").arg(done / 1000.0, 0, 'f', 1).arg(course.totalDistanceM() / 1000.0, 0, 'f', 1));
}

QRectF RaceView::progressPanelRect(const QRectF &mapArea) const
{
    return {mapArea.left() + 12, mapArea.top() + 12, 260, 100};
}

void RaceView::updateDisplayOrder()
{
    // Bots in a bunch are centimetres apart and would swap places every frame,
    // so a bot only passes another bot once clearly ahead (kPassMarginM). Your
    // own place is always the real one: the wheel you follow ranks ahead of you.
    constexpr double kPassMarginM = 2.0;
    const QVector<RaceSimulation::Rider> &riders = m_race->riders();
    if (m_displayOrder.size() != riders.size())
        m_displayOrder = m_race->standings();
    auto passes = [&](int behind, int ahead) {
        const RaceSimulation::Rider &b = riders.at(behind);
        const RaceSimulation::Rider &a = riders.at(ahead);
        if (b.finishTimeSec && a.finishTimeSec)
            return *b.finishTimeSec < *a.finishTimeSec;
        if (b.finishTimeSec || a.finishTimeSec)
            return b.finishTimeSec.has_value();
        return b.distanceM > a.distanceM + kPassMarginM;
    };
    m_displayOrder.removeOne(0);
    for (int pass = 0; pass < m_displayOrder.size(); ++pass) {
        bool swapped = false;
        for (int place = 0; place + 1 < m_displayOrder.size(); ++place)
            if (passes(m_displayOrder.at(place + 1), m_displayOrder.at(place))) {
                std::swap(m_displayOrder[place], m_displayOrder[place + 1]);
                swapped = true;
            }
        if (!swapped)
            break;
    }

    const QVector<int> standings = m_race->standings();
    const qsizetype yourPlace = standings.indexOf(0);
    std::stable_partition(m_displayOrder.begin(), m_displayOrder.end(),
                          [&](int bot) { return standings.indexOf(bot) < yourPlace; });
    m_displayOrder.insert(yourPlace, 0);
}

void RaceView::paintRoute(QPainter &painter, const QRectF &area) const
{
    const RaceCourse &course = m_race->course();
    const RaceSimulation::Rider &you = m_race->you();
    const QRectF bounds = zoomBounds();
    painter.save();
    painter.setClipRect(area);
    const QRectF &roadArea = area;   // you are in the middle of the map

    QPainterPath road;
    for (int index = 0; index < course.points().size(); ++index) {
        const QPointF point = routePoint(course.points().at(index).distanceM, roadArea, bounds);
        index == 0 ? road.moveTo(point) : road.lineTo(point);
    }
    // Wide enough for three riders side by side.
    QPen roadPen(kRoad, kRoadWidthPx, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(roadPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(road);

    const QPointF start = routePoint(0.0, roadArea, bounds);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Text));
    painter.drawEllipse(start, 5, 5);
    if (!course.isLoop()) {
        const QPointF finish = routePoint(course.totalDistanceM(), roadArea, bounds);
        painter.drawRect(QRectF(finish.x() - 6, finish.y() - 6, 12, 12));
    }

    // Riders: furthest back first, so the leaders and you are drawn on top.
    const QVector<int> &order = m_displayOrder;
    QVector<int> positionOf(m_race->riders().size());
    for (int place = 0; place < order.size(); ++place)
        positionOf[order.at(place)] = place + 1;

    QFont dotFont = font();
    dotFont.setBold(true);
    dotFont.setPointSizeF(font().pointSizeF() * 0.78);

    // Lay the riders out like a bunch, four abreast. Each rider keeps a lane
    // across the road from frame to frame and only moves over when the
    // lane is taken by a rider just ahead, then glides across instead of
    // jumping. Placed leader first, so the riders behind make way.
    constexpr double kRadius = 8.5;
    constexpr double kYourRadius = 10.0;
    constexpr double kSlot = 2 * kRadius + 2;
    constexpr double kLaneGlide = 0.12;   // share of the way to the new lane per frame
    constexpr int kLanes = 4;
    // Lane 0..3 across the road, centred on the road's middle line.
    auto laneOffset = [](double lane) { return lane - (kLanes - 1) / 2.0; };
    const int riderCount = static_cast<int>(m_race->riders().size());
    if (m_lanes.size() != riderCount) {
        m_lanes.resize(riderCount);
        m_laneOffsets.resize(riderCount);
        for (int index = 0; index < riderCount; ++index) {
            m_lanes[index] = (index + 1) % kLanes;   // you start in an inside lane
            m_laneOffsets[index] = m_lanes.at(index);
        }
    }
    const double pixelsPerMetre = std::min(roadArea.width() - 56.0, roadArea.height() - 56.0) / (2.0 * kZoomHalfM);
    const int wheel = you.draftingBehind;
    QVector<QPointF> centres(riderCount);
    QVector<QPointF> laneTargets;   // where the riders already placed are heading
    for (const int index : order) {
        const double distance = m_race->riders().at(index).distanceM;
        const QPointF onRoad = routePoint(distance, roadArea, bounds);
        const QPointF direction = routePoint(distance + 4.0, roadArea, bounds) - routePoint(distance - 4.0, roadArea, bounds);
        const double length = std::hypot(direction.x(), direction.y());
        const QPointF across = length > 0.01 ? QPointF(-direction.y() / length, direction.x() / length) : QPointF(0, 1);
        auto clearOf = [&](int lane) {
            const QPointF spot = onRoad + across * (laneOffset(lane) * kSlot);
            return std::all_of(laneTargets.cbegin(), laneTargets.cend(), [&](const QPointF &other) {
                return std::hypot(spot.x() - other.x(), spot.y() - other.y()) >= kSlot - 1.0;
            });
        };
        // Stay put if you can; otherwise move one lane over if it is free; if
        // neither neighbour is free, stay and overlap a little rather than hop.
        int lane = m_lanes.at(index);
        if (index == 0 && wheel >= 0) {
            lane = m_lanes.at(wheel);
        } else if (!clearOf(lane)) {
            for (const int option : {lane - 1, lane + 1})
                if (option >= 0 && option < kLanes && clearOf(option)) {
                    lane = option;
                    break;
                }
        }
        m_lanes[index] = lane;
        m_laneOffsets[index] += (lane - m_laneOffsets.at(index)) * kLaneGlide;
        laneTargets.append(onRoad + across * (laneOffset(lane) * kSlot));
        centres[index] = onRoad + across * (laneOffset(m_laneOffsets.at(index)) * kSlot);
        if (index == 0) {
            // Drafting: sit behind the wheel you follow instead of beside it. At
            // map scale a wheel is centimetres away, so the dot slides back one slot.
            const double wanted = wheel >= 0 ? std::max(0.0, kSlot - gapM(wheel) * pixelsPerMetre) : 0.0;
            m_yourSlideBack += (wanted - m_yourSlideBack) * kLaneGlide;
            if (length > 0.01)
                centres[0] -= direction / length * m_yourSlideBack;
            laneTargets.last() = centres[0];
        }
    }
    // Anyone sprinting (1.5 x their own FTP or more), you too: the border flashes red.
    const bool flashOn = (QDateTime::currentMSecsSinceEpoch() / 250) % 2 == 0;
    auto drawDot = [&](int index) {
        const RaceSimulation::Rider &rider = m_race->riders().at(index);
        const QPointF centre = centres.at(index);
        const double radius = rider.isYou ? kYourRadius : kRadius;
        painter.setPen(sprinting(index) && flashOn ? QPen(kSprint, 3.5) : QPen(palette().color(QPalette::Window), 2));
        painter.setBrush(rider.isYou ? kYou : kRider);
        painter.drawEllipse(centre, radius, radius);
        if (rider.isYou && draftState() == DraftState::Drafting) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(kDraft, 3));
            painter.drawEllipse(centre, radius + 5, radius + 5);
        }
        painter.setFont(dotFont);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(centre.x() - radius, centre.y() - radius, 2 * radius, 2 * radius),
                         Qt::AlignCenter, QString::number(positionOf.at(index)));
    };

    // Dots back to front, you on top; numbers match the standings.
    for (int place = static_cast<int>(order.size()) - 1; place >= 0; --place)
        if (order.at(place) != 0)
            drawDot(order.at(place));
    drawDot(0);

    // Name tags for the riders within kNameRangeM of you, where a tag fits
    // without covering a dot or another tag (you are the orange dot, no tag).
    // A tag keeps the side it had last frame while that side is free, so names
    // don't hop around a rider in a moving bunch.
    QVector<QRectF> taken;
    for (int index = 0; index < centres.size(); ++index) {
        const double radius = index == 0 ? kYourRadius : kRadius;
        taken.append(QRectF(centres.at(index) - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius)));
    }
    taken.append(nowCardRect(area));   // drawn over the map
    const QFontMetricsF tagMetrics(dotFont);
    auto placeTag = [&](int index, bool mustShow) {
        const RaceSimulation::Rider &rider = m_race->riders().at(index);
        const QPointF centre = centres.at(index);
        const double radius = rider.isYou ? kYourRadius + 6 : kRadius;   // clear of your draft ring
        const QString label = rider.isYou
            ? tr("%1 m \u00b7 -%2 %").arg(std::max(0.0, gapM(wheel)), 0, 'f', 1).arg(qRound(m_shownSaving * 100))
            : rider.name;
        const double width = tagMetrics.horizontalAdvance(label) + 12;
        const QRectF candidates[] = {
            {centre.x() + radius + 3, centre.y() - 9, width, 18},
            {centre.x() - radius - 3 - width, centre.y() - 9, width, 18},
            {centre.x() - width / 2, centre.y() - radius - 21, width, 18},
            {centre.x() - width / 2, centre.y() + radius + 3, width, 18},
        };
        if (m_tagSides.size() != m_race->riders().size()) {
            m_tagSides = QVector<int>(m_race->riders().size(), 0);
            m_tagSides[0] = 1;   // your card is on the right
        }
        int chosen = -1;
        for (int attempt = 0; attempt < 4; ++attempt) {
            const int side = (m_tagSides.at(index) + attempt) % 4;
            const QRectF &candidate = candidates[side];
            if (std::none_of(taken.cbegin(), taken.cend(), [&](const QRectF &other) { return other.intersects(candidate); })) {
                chosen = side;
                break;
            }
        }
        if (chosen < 0 && !mustShow)
            return;
        if (chosen >= 0)
            m_tagSides[index] = chosen;
        const QRectF tag = candidates[chosen >= 0 ? chosen : 2];
        taken.append(tag);
        const QColor colour = rider.isYou ? kDraft : kRider;
        painter.setPen(rider.isYou ? Qt::NoPen : QPen(colour, 1));
        painter.setBrush(rider.isYou ? colour : palette().color(QPalette::Base));
        painter.drawRoundedRect(tag, 4, 4);
        painter.setFont(dotFont);
        painter.setPen(rider.isYou ? Qt::black : colour);
        painter.drawText(tag, Qt::AlignCenter, label);
    };
    // How far back you are and the power you save, beside your dot while its ring is green.
    if (draftState() == DraftState::Drafting) {
        m_shownSaving = m_shownSaving <= 0.0 ? you.powerSaving : m_shownSaving + (you.powerSaving - m_shownSaving) * 0.1;
        placeTag(0, true);
    } else {
        m_shownSaving = 0.0;
    }
    QVector<int> nearby;
    for (int index = 1; index < m_race->riders().size(); ++index)
        if (std::abs(gapM(index)) <= kNameRangeM)
            nearby.append(index);
    std::sort(nearby.begin(), nearby.end(), [&](int a, int b) { return std::abs(gapM(a)) < std::abs(gapM(b)); });
    for (const int index : std::as_const(nearby))
        placeTag(index, false);
    painter.restore();
    paintOverview(painter, area);
}

void RaceView::paintOverview(QPainter &painter, const QRectF &mapArea) const
{
    const RaceCourse &course = m_race->course();
    const QRectF box(mapArea.left() + 12, progressPanelRect(mapArea).bottom() + 10, 260, 190);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(palette().color(QPalette::Base));
    painter.drawRoundedRect(box, 6, 6);

    QPainterPath road;
    for (int index = 0; index < course.points().size(); ++index) {
        const QPointF point = routePoint(course.points().at(index).distanceM, box, m_projectedBounds, 10.0);
        index == 0 ? road.moveTo(point) : road.lineTo(point);
    }
    painter.setPen(QPen(kRoad.lighter(130), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(road);

    // The window the big map is showing.
    const QRectF zoom = zoomBounds();
    const double scale = std::min((box.width() - 20) / m_projectedBounds.width(),
                                  (box.height() - 20) / m_projectedBounds.height());
    const QPointF centre = box.center() + QPointF((zoom.center().x() - m_projectedBounds.center().x()) * scale,
                                                  -(zoom.center().y() - m_projectedBounds.center().y()) * scale);
    const QRectF window(centre.x() - zoom.width() * scale / 2, centre.y() - zoom.height() * scale / 2,
                        zoom.width() * scale, zoom.height() * scale);
    painter.setPen(QPen(palette().color(QPalette::Text), 1, Qt::DashLine));
    painter.drawRect(window.intersected(box.adjusted(2, 2, -2, -2)));

    painter.setPen(Qt::NoPen);
    for (int index = static_cast<int>(m_race->riders().size()) - 1; index >= 0; --index) {
        const RaceSimulation::Rider &rider = m_race->riders().at(index);
        painter.setBrush(rider.isYou ? kYou : kRider);
        const double radius = rider.isYou ? 4.5 : 2.5;
        painter.drawEllipse(routePoint(rider.distanceM, box, m_projectedBounds, 10.0), radius, radius);
    }
}

QRectF RaceView::nowCardRect(const QRectF &mapArea) const
{
    // Almost centred, just right of your dot (which sits in the middle of the
    // map), so a glance from your dot to your numbers is short. See-through,
    // so riders passing under it stay visible. Kept below the top-right panels.
    constexpr double kRightOfYourDot = 60.0;
    const double left = mapArea.center().x() + kRightOfYourDot;
    const double width = std::min(kNowCardWidth, mapArea.right() - 12 - left);
    const double belowPanels = gearPanelRect(mapArea).bottom() + 10;
    const double top = std::max(mapArea.center().y() - kNowCardHeight / 2, belowPanels);
    return {left, std::min(top, mapArea.bottom() - kNowCardHeight - 8), width, kNowCardHeight};
}

void RaceView::setFtpWattsPerKg(double wattsPerKg)
{
    m_ftpWattsPerKg = wattsPerKg;
    update();
}

bool RaceView::sprinting(int riderIndex) const
{
    const RaceSimulation::Rider &rider = m_race->riders().at(riderIndex);
    constexpr double kSprintShareOfFtp = 1.5;
    const double ftpWattsPerKg = rider.isYou ? m_ftpWattsPerKg : rider.ftpWattsPerKg;
    return !rider.finishTimeSec && ftpWattsPerKg > 0.0
        && rider.powerW / rider.riderKg >= kSprintShareOfFtp * ftpWattsPerKg;
}

RaceView::DraftState RaceView::draftState() const
{
    return !m_race->youFinished() && m_race->you().draftingBehind >= 0 ? DraftState::Drafting : DraftState::None;
}

double RaceView::gapM(int riderIndex) const
{
    return m_race->riders().at(riderIndex).distanceM - m_race->you().distanceM;
}

void RaceView::paintNowPanel(QPainter &painter, const QRectF &mapArea) const
{
    const RaceSimulation::Rider &you = m_race->you();
    if (m_race->youFinished())
        return;

    // What you follow second by second, each value in its own fixed cell so a
    // changing number never pushes the others around.
    const QRectF card = nowCardRect(mapArea);
    QColor fill = palette().color(QPalette::Base);
    fill.setAlpha(165);   // see-through: riders under the card stay visible
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(card, 10, 10);

    const double wattsPerKg = you.powerW / you.riderKg;
    struct Cell { QString label; QString value; QString unit; QColor colour; QString sub; QColor subColour; };
    const QColor text = palette().color(QPalette::Text);
    auto orDash = [](int value) { return value > 0 ? QString::number(value) : QStringLiteral("-"); };
    // W/kg sits under the power, in the power cell.
    const Cell cells[] = {
        {tr("POWER"), QString::number(qRound(you.powerW)), tr("W"), text,
         tr("%1 W/kg").arg(wattsPerKg, 0, 'f', 1), sprinting(0) ? kSprint : text},
        {tr("CADENCE"), orDash(m_cadenceRpm), tr("rpm"), text, QString(), text},
        {tr("HEART RATE"), orDash(m_heartRateBpm), tr("bpm"), text, QString(), text},
        {tr("SPEED"), QString::number(you.speedMps * 3.6, 'f', 1), tr("km/h"), text, QString(), text},
    };
    QFont labelFont = font();
    labelFont.setPointSizeF(font().pointSizeF() * 0.95);
    labelFont.setLetterSpacing(QFont::PercentageSpacing, 108);
    QFont valueFont = font();
    valueFont.setBold(true);
    valueFont.setPointSizeF(font().pointSizeF() * 2.5);
    QFont unitFont = font();
    unitFont.setPointSizeF(font().pointSizeF() * 1.1);
    QFont subFont = font();
    subFont.setBold(true);
    subFont.setPointSizeF(font().pointSizeF() * 1.4);
    const int cellCount = static_cast<int>(std::size(cells));
    const double cellWidth = (card.width() - 24) / cellCount;
    for (int index = 0; index < cellCount; ++index) {
        const Cell &cell = cells[index];
        const QRectF box(card.left() + 12 + index * cellWidth, card.top() + 10, cellWidth, 112);
        if (index > 0) {
            painter.setPen(QPen(palette().color(QPalette::Mid), 1));
            painter.drawLine(QPointF(box.left(), box.top() + 6), QPointF(box.left(), box.bottom() - 4));
        }
        painter.setFont(labelFont);
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(QRectF(box.left(), box.top(), box.width(), 16), Qt::AlignHCenter | Qt::AlignTop, cell.label);
        // Your power zone, as a coloured pill beside "POWER".
        if (index == 0 && currentZone() >= 0) {
            const int zone = currentZone();
            const double labelWidth = QFontMetricsF(labelFont).horizontalAdvance(cell.label);
            const QRectF pill(box.center().x() + labelWidth / 2 + 6, box.top() - 1, 30, 18);
            QColor colour = PowerZones::color(zone);
            colour.setAlpha(255);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colour);
            painter.drawRoundedRect(pill, 4, 4);
            QFont pillFont = labelFont;
            pillFont.setBold(true);
            painter.setFont(pillFont);
            painter.setPen(QColor(0x1d, 0x1d, 0x1d));
            painter.drawText(pill, Qt::AlignCenter, tr("Z%1").arg(zone + 1));
        }
        const QFontMetricsF valueMetrics(valueFont);
        const QFontMetricsF unitMetrics(unitFont);
        const double valueWidth = valueMetrics.horizontalAdvance(cell.value);
        const double unitWidth = cell.unit.isEmpty() ? 0.0 : unitMetrics.horizontalAdvance(cell.unit) + 4;
        const double start = box.center().x() - (valueWidth + unitWidth) / 2;
        const QRectF valueBox(start, box.top() + 20, valueWidth + 2, 58);
        painter.setFont(valueFont);
        painter.setPen(cell.colour);
        painter.drawText(valueBox, Qt::AlignLeft | Qt::AlignVCenter, cell.value);
        if (!cell.unit.isEmpty()) {
            painter.setFont(unitFont);
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(QRectF(valueBox.right() + 4, valueBox.top() + 8, unitWidth, valueBox.height()),
                             Qt::AlignLeft | Qt::AlignVCenter, cell.unit);
        }
        if (!cell.sub.isEmpty()) {
            painter.setFont(subFont);
            painter.setPen(cell.subColour);
            painter.drawText(QRectF(box.left(), valueBox.bottom(), box.width(), 28), Qt::AlignHCenter | Qt::AlignVCenter, cell.sub);
        }
    }

    QFont draftFont = font();
    draftFont.setPointSizeF(font().pointSizeF() * 1.3);
    draftFont.setBold(you.draftingBehind >= 0);
    painter.setFont(draftFont);
    const QRectF draftCell(card.left() + 18, card.bottom() - 38, card.width() - 36, 30);
    if (you.draftingBehind >= 0) {
        painter.setPen(kDraft);
        const QString wheel = m_race->riders().at(you.draftingBehind).name;
        painter.drawText(draftCell, Qt::AlignHCenter | Qt::AlignVCenter,
                         QFontMetrics(draftFont).elidedText(
                             tr("On %1's wheel%2")
                                 .arg(wheel)
                                 .arg(you.ridersSheltering > 1
                                          ? tr(" \u00b7 %1 riders").arg(you.ridersSheltering)
                                          : QString()),
                             Qt::ElideMiddle, static_cast<int>(draftCell.width())));
    } else {
        constexpr double kShowAheadM = 60.0;
        int nearest = -1;
        for (int index = 1; index < m_race->riders().size(); ++index) {
            const double gap = gapM(index);
            if (gap > 0.0 && gap <= kShowAheadM && !m_race->riders().at(index).finishTimeSec
                && (nearest < 0 || gap < gapM(nearest)))
                nearest = index;
        }
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(draftCell, Qt::AlignHCenter | Qt::AlignVCenter,
                         nearest < 0 ? tr("No draft")
                                     : tr("%1 is %2 m ahead \u00b7 close the gap to draft")
                                           .arg(m_race->riders().at(nearest).name)
                                           .arg(qRound(gapM(nearest))));
    }
}

QRectF RaceView::gearPanelRect(const QRectF &mapArea) const
{
    // Under Time in zone, top right.
    constexpr double kWidth = 190.0;
    return {mapArea.right() - kWidth - 12, mapArea.top() + 12 + 112 + 10, kWidth, m_gearVisible ? 46.0 : 0.0};
}

void RaceView::paintGearPanel(QPainter &painter, const QRectF &mapArea) const
{
    if (!m_gearVisible)
        return;
    const QRectF panel = gearPanelRect(mapArea);
    QColor fill = palette().color(QPalette::Base);
    fill.setAlpha(240);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(panel, 8, 8);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kFlashMs = 350;
    m_gearDownRect = QRectF(panel.left() + 6, panel.top() + 6, 34, panel.height() - 12);
    m_gearUpRect = QRectF(panel.right() - 40, panel.top() + 6, 34, panel.height() - 12);
    auto drawButton = [&](const QRectF &button, const QString &arrow, int direction) {
        const bool lit = m_shiftDirection == direction && now - m_shiftAtMs < kFlashMs;
        painter.setPen(Qt::NoPen);
        painter.setBrush(lit ? kProgress : palette().color(QPalette::Mid));
        painter.drawRoundedRect(button, 6, 6);
        QFont arrowFont = font();
        arrowFont.setBold(true);
        arrowFont.setPointSizeF(font().pointSizeF() * 1.2);
        painter.setFont(arrowFont);
        painter.setPen(lit ? QColor(0x1d, 0x1d, 0x1d) : palette().color(QPalette::Text));
        painter.drawText(button, Qt::AlignCenter, arrow);
    };
    drawButton(m_gearDownRect, QStringLiteral("\u25bc"), -1);
    drawButton(m_gearUpRect, QStringLiteral("\u25b2"), 1);

    QFont gearFont = font();
    gearFont.setBold(true);
    gearFont.setPointSizeF(font().pointSizeF() * 1.25);
    painter.setFont(gearFont);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRectF(m_gearDownRect.right(), panel.top(), m_gearUpRect.left() - m_gearDownRect.right(), panel.height()),
                     Qt::AlignCenter, tr("Gear %1/%2").arg(m_gear).arg(m_gearCount));
}

void RaceView::setGear(int gear, int gearCount, bool visible)
{
    m_gear = gear;
    m_gearCount = gearCount;
    m_gearVisible = visible;
    update();
}

void RaceView::flashShift(int direction)
{
    m_shiftDirection = direction;
    m_shiftAtMs = QDateTime::currentMSecsSinceEpoch();
    update();
}

void RaceView::setZones(int ftpWatts, const QVector<int> &zoneBoundsPct)
{
    m_ftpWatts = ftpWatts;
    m_zoneBoundsPct = zoneBoundsPct;
    m_zoneSeconds = QVector<double>(zoneBoundsPct.size() + 1, 0.0);
    update();
}

int RaceView::currentZone() const
{
    if (m_ftpWatts <= 0 || m_zoneSeconds.isEmpty())
        return -1;
    return PowerZones::zoneForPct(100.0 * m_race->you().powerW / m_ftpWatts, m_zoneBoundsPct);
}

void RaceView::addRideTime(double seconds)
{
    const int zone = currentZone();
    if (zone >= 0 && zone < m_zoneSeconds.size())
        m_zoneSeconds[zone] += seconds;
}

void RaceView::paintZonePanel(QPainter &painter, const QRectF &mapArea) const
{
    if (m_zoneSeconds.isEmpty())
        return;
    // Time in each zone so far, Zwift style: one bar per zone, tallest = most time.
    const int zones = static_cast<int>(m_zoneSeconds.size());
    constexpr double kBarWidth = 18.0;
    constexpr double kGap = 5.0;
    const double width = zones * kBarWidth + (zones - 1) * kGap + 24;
    const QRectF panel(mapArea.right() - width - 12, mapArea.top() + 12, width, 112);
    QColor fill = palette().color(QPalette::Base);
    fill.setAlpha(240);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(panel, 8, 8);

    QFont small = font();
    small.setPointSizeF(font().pointSizeF() * 0.75);
    painter.setFont(small);
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(QRectF(panel.left(), panel.top() + 5, panel.width(), 14), Qt::AlignHCenter, tr("TIME IN ZONE"));

    const double most = *std::max_element(m_zoneSeconds.cbegin(), m_zoneSeconds.cend());
    const QRectF bars(panel.left() + 12, panel.top() + 24, panel.width() - 24, 64);
    const int now = currentZone();
    for (int zone = 0; zone < zones; ++zone) {
        const double x = bars.left() + zone * (kBarWidth + kGap);
        const double height = most > 0.0 ? std::max(2.0, bars.height() * m_zoneSeconds.at(zone) / most) : 2.0;
        QColor colour = PowerZones::color(zone);
        colour.setAlpha(255);
        painter.setPen(zone == now ? QPen(palette().color(QPalette::Text), 2) : Qt::NoPen);
        painter.setBrush(colour);
        painter.drawRoundedRect(QRectF(x, bars.bottom() - height, kBarWidth, height), 3, 3);
        painter.setPen(zone == now ? palette().color(QPalette::Text) : palette().color(QPalette::PlaceholderText));
        painter.drawText(QRectF(x - 4, bars.bottom() + 3, kBarWidth + 8, 14), Qt::AlignHCenter, tr("Z%1").arg(zone + 1));
    }
}

void RaceView::setLiveMetrics(int cadenceRpm, int heartRateBpm)
{
    m_cadenceRpm = cadenceRpm;
    m_heartRateBpm = heartRateBpm;
}

void RaceView::mousePressEvent(QMouseEvent *event)
{
    if (m_gearVisible && m_gearUpRect.contains(event->position()))
        emit gearUpClicked();
    else if (m_gearVisible && m_gearDownRect.contains(event->position()))
        emit gearDownClicked();
    else
        QWidget::mousePressEvent(event);
}

void RaceView::paintStandings(QPainter &painter, const QRectF &area) const
{
    painter.fillRect(area, palette().color(QPalette::AlternateBase));
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(area.topLeft(), area.bottomLeft());

    const QVector<int> &order = m_displayOrder;
    QFont label = font();
    label.setPointSizeF(font().pointSizeF() * 0.8);
    painter.setFont(label);
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(area.adjusted(14, 8, -12, 0), Qt::AlignLeft | Qt::AlignTop,
                     tr("STANDINGS · %1 RIDERS").arg(order.size()));

    const double top = area.top() + 30;
    const int rowsThatFit = std::max(1, static_cast<int>((area.bottom() - top) / kRowHeight));
    const int yourPlace = static_cast<int>(order.indexOf(0));

    // Everyone if it fits; otherwise the top three and the riders around you.
    QVector<int> places;
    if (order.size() <= rowsThatFit) {
        for (int place = 0; place < order.size(); ++place)
            places.append(place);
    } else {
        const int head = std::min(3, rowsThatFit / 3);
        for (int place = 0; place < head; ++place)
            places.append(place);
        const int window = rowsThatFit - head;
        int from = std::max(head, yourPlace - window / 2);
        from = std::min(from, static_cast<int>(order.size()) - window);
        for (int place = from; place < from + window; ++place)
            places.append(place);
    }

    const RaceSimulation::Rider &you = m_race->you();
    double y = top;
    for (const int place : std::as_const(places)) {
        const int index = order.at(place);
        const RaceSimulation::Rider &rider = m_race->riders().at(index);
        const QRectF row(area.left() + 6, y, area.width() - 12, kRowHeight - 2);
        if (rider.isYou) {
            painter.setPen(QPen(kYou, 1));
            painter.setBrush(palette().color(QPalette::Base));
            painter.drawRoundedRect(row, 5, 5);
        }
        QFont rowFont = font();
        rowFont.setPointSizeF(font().pointSizeF() * 1.15);
        rowFont.setBold(rider.isYou);
        const QColor text = rider.isYou ? kYou : palette().color(QPalette::Text);
        const QColor muted = rider.isYou ? palette().color(QPalette::Text) : palette().color(QPalette::PlaceholderText);
        constexpr double kGapWidth = 54.0;
        constexpr double kWkgWidth = 62.0;
        const QRectF gapCell(row.right() - 8 - kGapWidth, row.top(), kGapWidth, row.height());
        const QRectF wkgCell(gapCell.left() - 6 - kWkgWidth, row.top(), kWkgWidth, row.height());
        const QRectF nameCell(row.left() + 32, row.top(), wkgCell.left() - 16 - (row.left() + 32), row.height());

        painter.setFont(rowFont);
        painter.setPen(text);
        painter.drawText(row.adjusted(8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, QString::number(place + 1));
        const QString name = rider.isYou ? tr("You") : rider.name;
        const QString shown = QFontMetrics(rowFont).elidedText(name, Qt::ElideRight, static_cast<int>(nameCell.width()));
        painter.drawText(nameCell, Qt::AlignVCenter | Qt::AlignLeft, shown);
        if (rider.draftingBehind >= 0 && !rider.finishTimeSec) {
            const double nameWidth = QFontMetricsF(rowFont).horizontalAdvance(shown);
            painter.setPen(Qt::NoPen);
            painter.setBrush(kDraft);
            painter.drawEllipse(QPointF(nameCell.left() + nameWidth + 8, row.center().y()), 3.5, 3.5);
        }

        QFont numbers = rowFont;
        numbers.setPointSizeF(font().pointSizeF() * 0.92);
        painter.setFont(numbers);
        if (!rider.finishTimeSec) {
            const double wattsPerKg = rider.powerW / rider.riderKg;
            painter.setPen(sprinting(index) ? kSprint : muted);
            painter.drawText(wkgCell, Qt::AlignVCenter | Qt::AlignRight,
                             QStringLiteral("%1 W/kg").arg(wattsPerKg, 0, 'f', 1));
        }
        painter.setPen(muted);
        QString right;
        if (rider.isYou)
            right = rider.finishTimeSec ? clock(*rider.finishTimeSec) : QString();
        else if (rider.finishTimeSec && !you.finishTimeSec)
            right = clock(*rider.finishTimeSec);
        else
            right = gapText(m_race->gapToYouSec(index));
        painter.drawText(gapCell, Qt::AlignVCenter | Qt::AlignRight, right);
        y += kRowHeight;
    }
}

void RaceView::paintProfile(QPainter &painter, const QRectF &area) const
{
    const RaceCourse &course = m_race->course();
    const RaceSimulation::Rider &you = m_race->you();
    painter.fillRect(area, palette().color(QPalette::Base));
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(area.topLeft(), area.topRight());

    double low = 1e9;
    double high = -1e9;
    for (const RaceCourse::Point &point : course.points()) {
        low = std::min(low, point.elevationM);
        high = std::max(high, point.elevationM);
    }
    // Keep gentle courses looking gentle: at least 60 m of vertical range.
    const double span = std::max(60.0, high - low);
    // The top strip is kept for the grade label, so it never covers the hill.
    const QRectF plot = area.adjusted(14, 36, -14, -10);
    const double total = course.totalDistanceM();
    auto toX = [&](double distance) { return plot.left() + plot.width() * std::clamp(distance / total, 0.0, 1.0); };
    auto toY = [&](double elevation) { return plot.bottom() - plot.height() * (elevation - low) / span; };

    // Bands scale with the course so a long climb reads as sections, not noise.
    const double segmentM = std::max(kProfileMinSegmentM, course.lapDistanceM() / kProfileBands);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing, false);   // no seams between bands
    const int bands = static_cast<int>(std::ceil(total / segmentM));
    for (int band = 0; band < bands; ++band) {
        const double from = band * segmentM;
        const double to = std::min(total, from + segmentM);
        const double grade = (course.elevationAt(to) - course.elevationAt(from)) / (to - from);
        QPolygonF shape;
        shape << QPointF(toX(from), plot.bottom()) << QPointF(toX(from), toY(course.elevationAt(from)))
              << QPointF(toX(to), toY(course.elevationAt(to))) << QPointF(toX(to), plot.bottom());
        painter.setBrush(gradeColor(grade));
        painter.drawPolygon(shape);
    }
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Where each new lap starts: a dotted line and its number.
    if (course.isLoop() && course.laps() > 1) {
        QFont lapFont = font();
        lapFont.setPointSizeF(font().pointSizeF() * 0.8);
        painter.setFont(lapFont);
        for (int lap = 1; lap < course.laps(); ++lap) {
            const double x = toX(lap * course.lapDistanceM());
            painter.setPen(QPen(palette().color(QPalette::Text), 1, Qt::DotLine));
            painter.drawLine(QPointF(x, plot.top() - 4), QPointF(x, plot.bottom()));
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(QRectF(x + 4, plot.top() - 4, 60, 14), Qt::AlignLeft | Qt::AlignTop, tr("Lap %1").arg(lap + 1));
        }
    }

    for (int index = static_cast<int>(m_race->riders().size()) - 1; index >= 1; --index) {
        const RaceSimulation::Rider &rider = m_race->riders().at(index);
        const double distance = std::max(0.0, rider.distanceM);
        painter.setBrush(kRider);
        painter.setPen(QPen(palette().color(QPalette::Base), 1.5));
        painter.drawEllipse(QPointF(toX(distance), toY(course.elevationAt(distance)) - 3), 4.5, 4.5);
    }
    const double yourDistance = std::max(0.0, you.distanceM);
    painter.setPen(QPen(kYou, 2));
    painter.drawLine(QPointF(toX(yourDistance), plot.top()), QPointF(toX(yourDistance), plot.bottom()));
    painter.setBrush(kYou);
    painter.setPen(QPen(palette().color(QPalette::Base), 1.5));
    painter.drawEllipse(QPointF(toX(yourDistance), toY(course.elevationAt(yourDistance)) - 3), 6.5, 6.5);

    // Grade and altitude where you are, riding along your line.
    const double grade = course.gradeAt(yourDistance);
    QFont tagFont = font();
    tagFont.setBold(true);
    tagFont.setPointSizeF(font().pointSizeF() * 1.1);
    const QString tagText = tr("Grade %1 %").arg(grade * 100.0, 0, 'f', 1);
    const QSizeF tagSize(QFontMetricsF(tagFont).horizontalAdvance(tagText) + 18, 24);
    const double lineX = toX(yourDistance);
    // Beside the line, switching sides near the right edge.
    const double tagLeft = lineX + 8 + tagSize.width() <= plot.right() ? lineX + 8 : lineX - 8 - tagSize.width();
    const QRectF tag(QPointF(tagLeft, area.top() + 6), tagSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradeColor(grade));
    painter.drawRoundedRect(tag, 5, 5);
    painter.setFont(tagFont);
    painter.setPen(QColor(0x1d, 0x1d, 0x1d));
    painter.drawText(tag, Qt::AlignCenter, tagText);
}

void RaceView::paintFinish(QPainter &painter, const QRectF &area) const
{
    const RaceSimulation::Rider &you = m_race->you();
    const QRectF box(area.center().x() - 170, area.center().y() - 55, 340, 110);
    painter.setPen(QPen(kYou, 2));
    painter.setBrush(palette().color(QPalette::Base));
    painter.drawRoundedRect(box, 10, 10);

    QFont big = font();
    big.setBold(true);
    big.setPointSizeF(font().pointSizeF() * 1.8);
    painter.setFont(big);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(box.adjusted(0, 14, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                     tr("Finished %1").arg(ordinal(m_race->yourPosition())));
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(box.adjusted(0, 0, 0, -18), Qt::AlignHCenter | Qt::AlignBottom,
                     tr("%1 of %2 riders · %3").arg(ordinal(m_race->yourPosition()))
                         .arg(m_race->riders().size())
                         .arg(clock(you.finishTimeSec.value_or(0.0))));
}
