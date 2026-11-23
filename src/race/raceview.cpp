#include "raceview.h"

#include "racesimulation.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace {

const QColor kYou(0xf0, 0x8a, 0x44);
const QColor kRider(0x7f, 0x9f, 0xd6);
const QColor kRidden(0x4c, 0xc0, 0x8f);
const QColor kDraft(0x4c, 0xc0, 0x8f);
const QColor kGradeEasy(0x7c, 0xc4, 0x7f);
const QColor kGradeMid(0xe0, 0xc3, 0x41);
const QColor kGradeHard(0xe8, 0xa2, 0x3a);
const QColor kGradeSteep(0xd6, 0x55, 0x3d);

constexpr double kTopBarHeight = 40.0;
constexpr double kStandingsWidth = 300.0;
constexpr double kRowHeight = 22.0;
constexpr double kNameTagRangeM = 250.0;
constexpr double kNowCardWidth = 420.0;
constexpr double kNowCardHeight = 92.0;
constexpr double kZoomHalfM = 130.0;   // the map shows 260 m around you
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

    const QRectF all = rect();
    const double profileHeight = std::clamp(all.height() * 0.28, 110.0, 180.0);
    const QRectF topBar(all.left(), all.top(), all.width(), kTopBarHeight);
    const QRectF profile(all.left(), all.bottom() - profileHeight, all.width(), profileHeight);
    const double middleTop = topBar.bottom();
    const double middleHeight = profile.top() - middleTop;
    const double standingsWidth = std::min(kStandingsWidth, all.width() * 0.38);
    const QRectF map(all.left(), middleTop, all.width() - standingsWidth, middleHeight);
    const QRectF standings(map.right(), middleTop, standingsWidth, middleHeight);

    paintTopBar(painter, topBar);
    paintRoute(painter, map);
    paintNowPanel(painter, map);
    paintStandings(painter, standings);
    paintProfile(painter, profile);
    if (m_race->youFinished())
        paintFinish(painter, map);
}

void RaceView::paintTopBar(QPainter &painter, const QRectF &area) const
{
    const RaceCourse &course = m_race->course();
    const RaceSimulation::Rider &you = m_race->you();
    const double done = std::clamp(you.distanceM, 0.0, course.totalDistanceM());
    const double share = done / course.totalDistanceM();

    painter.fillRect(area, palette().color(QPalette::Base));
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(font().pointSizeF() * 1.1);
    painter.setFont(titleFont);
    painter.setPen(palette().color(QPalette::Text));
    const QRectF titleArea = area.adjusted(14, 0, -area.width() * 0.45, 0);
    painter.drawText(titleArea, Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetrics(titleFont).elidedText(course.name() + QStringLiteral(" · ") + course.place(),
                                                        Qt::ElideRight, static_cast<int>(titleArea.width())));

    QString progress = QStringLiteral("%1 / %2 km").arg(done / 1000.0, 0, 'f', 1).arg(course.totalDistanceM() / 1000.0, 0, 'f', 1);
    if (course.isLoop()) {
        const int lap = std::min(course.laps(), static_cast<int>(done / course.lapDistanceM()) + 1);
        progress = tr("Lap %1/%2").arg(lap).arg(course.laps()) + QStringLiteral("   ") + progress;
    }
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::PlaceholderText));
    const double barWidth = std::min(220.0, area.width() * 0.2);
    const QRectF bar(area.right() - barWidth - 64, area.center().y() - 5, barWidth, 10);
    painter.drawText(QRectF(area.left(), area.top(), bar.left() - 14 - area.left(), area.height()),
                     Qt::AlignVCenter | Qt::AlignRight, progress);

    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Mid));
    painter.drawRoundedRect(bar, 5, 5);
    painter.setBrush(kRidden);
    painter.drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * share, bar.height()), 5, 5);
    QFont percentFont = font();
    percentFont.setBold(true);
    painter.setFont(percentFont);
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRectF(bar.right() + 8, area.top(), 50, area.height()), Qt::AlignVCenter | Qt::AlignRight,
                     QStringLiteral("%1 %").arg(qRound(share * 100)));
}

void RaceView::paintRoute(QPainter &painter, const QRectF &area) const
{
    const RaceCourse &course = m_race->course();
    const RaceSimulation::Rider &you = m_race->you();
    const QRectF bounds = zoomBounds();
    painter.save();
    painter.setClipRect(area);
    // Centre the road in the space above the card, so it never hides you.
    const QRectF mapArea = area;
    const QRectF roadArea(mapArea.left(), mapArea.top(), mapArea.width(),
                       std::max(120.0, nowCardRect(mapArea).top() - mapArea.top()));

    QPainterPath road;
    for (int index = 0; index < course.points().size(); ++index) {
        const QPointF point = routePoint(course.points().at(index).distanceM, roadArea, bounds);
        index == 0 ? road.moveTo(point) : road.lineTo(point);
    }
    QPen roadPen(palette().color(QPalette::Mid), 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(roadPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(road);

    // The part of the current lap you have ridden.
    const double lapStart = course.isLoop() ? std::floor(std::max(0.0, you.distanceM) / course.lapDistanceM()) * course.lapDistanceM() : 0.0;
    QPainterPath ridden;
    bool started = false;
    const int samples = static_cast<int>((you.distanceM - lapStart) / 20.0);
    for (int sample = 0; sample <= samples; ++sample) {
        const QPointF point = routePoint(lapStart + sample * 20.0, roadArea, bounds);
        started ? ridden.lineTo(point) : ridden.moveTo(point);
        started = true;
    }
    if (started) {
        ridden.lineTo(routePoint(you.distanceM, roadArea, bounds));
        painter.setPen(QPen(kRidden, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(ridden);
    }

    const QPointF start = routePoint(0.0, roadArea, bounds);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Text));
    painter.drawEllipse(start, 5, 5);
    if (!course.isLoop()) {
        const QPointF finish = routePoint(course.totalDistanceM(), roadArea, bounds);
        painter.drawRect(QRectF(finish.x() - 6, finish.y() - 6, 12, 12));
    }

    // Riders: furthest back first, so the leaders and you are drawn on top.
    const QVector<int> order = m_race->standings();
    QVector<int> positionOf(m_race->riders().size());
    for (int place = 0; place < order.size(); ++place)
        positionOf[order.at(place)] = place + 1;

    QFont dotFont = font();
    dotFont.setBold(true);
    dotFont.setPointSizeF(font().pointSizeF() * 0.78);
    const QFontMetricsF dotMetrics(dotFont);
    QVector<QRectF> placedTags;

    auto dotCentre = [&](int index) { return routePoint(m_race->riders().at(index).distanceM, roadArea, bounds); };
    auto drawDot = [&](int index) {
        const RaceSimulation::Rider &rider = m_race->riders().at(index);
        const QPointF centre = dotCentre(index);
        const double radius = rider.isYou ? 10.0 : 8.5;
        painter.setPen(QPen(palette().color(QPalette::Window), 2));
        painter.setBrush(rider.isYou ? kYou : kRider);
        painter.drawEllipse(centre, radius, radius);
        painter.setFont(dotFont);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(centre.x() - radius, centre.y() - radius, 2 * radius, 2 * radius),
                         Qt::AlignCenter, QString::number(positionOf.at(index)));
    };
    auto drawTag = [&](int index) {
        const RaceSimulation::Rider &rider = m_race->riders().at(index);
        const bool nearYou = std::abs(rider.distanceM - you.distanceM) <= kNameTagRangeM;
        if (!rider.isYou && !nearYou && positionOf.at(index) > 1)
            return;
        const QPointF centre = dotCentre(index);
        const double radius = rider.isYou ? 10.0 : 8.5;
        const QColor colour = rider.isYou ? kYou : kRider;
        const QString label = rider.isYou ? tr("You") : rider.name;
        const double width = dotMetrics.horizontalAdvance(label) + 10;
        const QRectF candidates[] = {
            {centre.x() + radius + 3, centre.y() - 9, width, 17},
            {centre.x() - radius - 3 - width, centre.y() - 9, width, 17},
            {centre.x() - width / 2, centre.y() - radius - 20, width, 17},
            {centre.x() - width / 2, centre.y() + radius + 3, width, 17},
        };
        for (const QRectF &tag : candidates) {
            const bool clear = std::none_of(placedTags.cbegin(), placedTags.cend(),
                                            [&tag](const QRectF &other) { return other.intersects(tag); });
            // Yours is always drawn: in the first free spot, or the first spot.
            const bool lastChance = rider.isYou && &tag == &candidates[std::size(candidates) - 1];
            if (!clear && !lastChance)
                continue;
            const QRectF &placed = clear ? tag : candidates[0];
            placedTags.append(placed);
            painter.setPen(rider.isYou ? Qt::NoPen : QPen(colour, 1));
            painter.setBrush(rider.isYou ? colour : palette().color(QPalette::Base));
            painter.drawRoundedRect(placed, 4, 4);
            painter.setFont(dotFont);
            painter.setPen(rider.isYou ? Qt::white : colour);
            painter.drawText(placed, Qt::AlignCenter, label);
            return;
        }
    };

    // Dots back to front with you on top; then tags, yours first so it always
    // gets a spot, then the leader and the riders nearest you.
    for (int place = static_cast<int>(order.size()) - 1; place >= 0; --place)
        if (order.at(place) != 0)
            drawDot(order.at(place));
    drawDot(0);
    for (int index = 0; index < m_race->riders().size(); ++index)
        placedTags.append(QRectF(dotCentre(index) - QPointF(8, 8), QSizeF(16, 16)));
    drawTag(0);
    QVector<int> byCloseness;
    for (int index = 1; index < m_race->riders().size(); ++index)
        byCloseness.append(index);
    std::sort(byCloseness.begin(), byCloseness.end(), [&](int a, int b) {
        if ((positionOf.at(a) == 1) != (positionOf.at(b) == 1))
            return positionOf.at(a) == 1;
        return std::abs(m_race->riders().at(a).distanceM - you.distanceM)
             < std::abs(m_race->riders().at(b).distanceM - you.distanceM);
    });
    for (const int index : std::as_const(byCloseness))
        drawTag(index);
    painter.restore();
    paintOverview(painter, area);
}

void RaceView::paintOverview(QPainter &painter, const QRectF &mapArea) const
{
    const RaceCourse &course = m_race->course();
    const QRectF box(mapArea.left() + 12, mapArea.top() + 12, 170, 120);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(palette().color(QPalette::Base));
    painter.drawRoundedRect(box, 6, 6);

    QPainterPath road;
    for (int index = 0; index < course.points().size(); ++index) {
        const QPointF point = routePoint(course.points().at(index).distanceM, box, m_projectedBounds, 10.0);
        index == 0 ? road.moveTo(point) : road.lineTo(point);
    }
    painter.setPen(QPen(palette().color(QPalette::Mid), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
    const double width = std::min(kNowCardWidth, mapArea.width() - 24);
    return {mapArea.center().x() - width / 2, mapArea.bottom() - kNowCardHeight - 10, width, kNowCardHeight};
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

    // Only what you follow second by second: speed, W/kg and the draft.
    const QRectF card = nowCardRect(mapArea);
    QColor fill = palette().color(QPalette::Base);
    fill.setAlpha(235);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(card, 10, 10);

    QFont big = font();
    big.setBold(true);
    big.setPointSizeF(font().pointSizeF() * 2.3);
    QFont unit = font();
    unit.setPointSizeF(font().pointSizeF() * 1.1);
    const QRectF left(card.left() + 16, card.top() + 8, card.width() / 2 - 16, 46);
    const QRectF right(card.center().x(), card.top() + 8, card.width() / 2 - 16, 46);
    auto drawValue = [&](const QRectF &cell, const QString &value, const QString &suffix, Qt::Alignment side) {
        const double valueWidth = QFontMetricsF(big).horizontalAdvance(value);
        const double suffixWidth = QFontMetricsF(unit).horizontalAdvance(suffix) + 6;
        const double start = side == Qt::AlignLeft ? cell.left() : cell.right() - valueWidth - suffixWidth;
        painter.setFont(big);
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(QRectF(start, cell.top(), valueWidth + 2, cell.height()), Qt::AlignLeft | Qt::AlignVCenter, value);
        painter.setFont(unit);
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(QRectF(start + valueWidth + 6, cell.top() + 6, suffixWidth, cell.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, suffix);
    };
    drawValue(left, QString::number(you.speedMps * 3.6, 'f', 1), tr("km/h"), Qt::AlignLeft);
    drawValue(right, QString::number(you.powerW / you.riderKg, 'f', 1), tr("W/kg"), Qt::AlignRight);

    QFont draftFont = font();
    draftFont.setPointSizeF(font().pointSizeF() * 1.15);
    draftFont.setBold(you.draftingBehind >= 0);
    painter.setFont(draftFont);
    const QRectF draftCell(card.left() + 16, card.bottom() - 34, card.width() - 32, 26);
    if (you.draftingBehind >= 0) {
        painter.setPen(kDraft);
        const QString wheel = m_race->riders().at(you.draftingBehind).name;
        painter.drawText(draftCell, Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(draftFont).elidedText(
                             tr("On %1's wheel \u00b7 %2 \u00b7 saving %3 %")
                                 .arg(wheel)
                                 .arg(gapM(you.draftingBehind) < 0.5
                                          ? tr("under 0.5 m")
                                          : tr("%1 m").arg(gapM(you.draftingBehind), 0, 'f', 1))
                                 .arg(qRound(you.powerSaving * 100)),
                             Qt::ElideMiddle, static_cast<int>(draftCell.width())));
    } else {
        // The nearest rider ahead, so you know how far there is to close.
        constexpr double kShowAheadM = 60.0;
        int nearest = -1;
        for (int index = 1; index < m_race->riders().size(); ++index) {
            const double gap = gapM(index);
            if (gap > 0.0 && gap <= kShowAheadM && !m_race->riders().at(index).finishTimeSec
                && (nearest < 0 || gap < gapM(nearest)))
                nearest = index;
        }
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(draftCell, Qt::AlignLeft | Qt::AlignVCenter,
                         nearest < 0 ? tr("No draft")
                                     : tr("%1 is %2 m ahead \u00b7 close the gap to draft")
                                           .arg(m_race->riders().at(nearest).name)
                                           .arg(qRound(gapM(nearest))));
    }
}

void RaceView::paintStandings(QPainter &painter, const QRectF &area) const
{
    painter.fillRect(area, palette().color(QPalette::AlternateBase));
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(area.topLeft(), area.bottomLeft());

    const QVector<int> order = m_race->standings();
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
        painter.setPen(muted);
        if (!rider.finishTimeSec)
            painter.drawText(wkgCell, Qt::AlignVCenter | Qt::AlignRight,
                             QStringLiteral("%1 W/kg").arg(rider.powerW / rider.riderKg, 0, 'f', 1));
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

    QFont label = font();
    label.setPointSizeF(font().pointSizeF() * 0.8);
    painter.setFont(label);
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(area.adjusted(14, 6, 0, 0), Qt::AlignLeft | Qt::AlignTop, tr("WHOLE COURSE · GRADIENT"));

    double low = 1e9;
    double high = -1e9;
    for (const RaceCourse::Point &point : course.points()) {
        low = std::min(low, point.elevationM);
        high = std::max(high, point.elevationM);
    }
    // Keep gentle courses looking gentle: at least 60 m of vertical range.
    const double span = std::max(60.0, high - low);
    const QRectF plot = area.adjusted(14, 26, -14, -10);
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
    const QRectF tag(QPointF(tagLeft, plot.top() + 2), tagSize);
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
