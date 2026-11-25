#include "sidebaricons.h"

#include "powerzones.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

constexpr int kSize = 128;
const QColor kOutline(0x30, 0x30, 0x30);

template <typename Draw>
QIcon drawn(Draw draw)
{
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    draw(painter);
    return QIcon(pixmap);
}

QColor zoneColor(int zone)
{
    QColor color = PowerZones::color(zone);
    color.setAlpha(255);
    return color;
}

void drawRider(QPainter &painter, double centreX, double headY, double scale, const QColor &color)
{
    painter.setPen(QPen(kOutline, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);
    const double bottom = 120.0;
    const double shoulder = headY + 20 * scale;
    QPainterPath body;
    body.moveTo(centreX - 27 * scale, bottom);
    body.lineTo(centreX - 27 * scale, shoulder + 24 * scale);
    body.quadTo(centreX - 27 * scale, shoulder, centreX, shoulder);
    body.quadTo(centreX + 27 * scale, shoulder, centreX + 27 * scale, shoulder + 24 * scale);
    body.lineTo(centreX + 27 * scale, bottom);
    body.closeSubpath();
    painter.drawPath(body);
    painter.drawEllipse(QPointF(centreX, headY), 18 * scale, 18 * scale);
}

} // namespace

QIcon SidebarIcons::workout()
{
    return drawn([](QPainter &painter) {
        // Warm-up, two hard intervals with recoveries, cool-down.
        struct Block { double x, width, height; int zone; };
        constexpr Block blocks[] = {{6, 18, 40, 1}, {24, 22, 104, 4}, {46, 18, 50, 2},
                                    {64, 22, 104, 4}, {86, 18, 70, 3}, {104, 18, 34, 1}};
        painter.setPen(QPen(kOutline, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (const Block &block : blocks) {
            painter.setBrush(zoneColor(block.zone));
            painter.drawRect(QRectF(block.x, 120 - block.height, block.width, block.height));
        }
    });
}

QIcon SidebarIcons::stats()
{
    return drawn([](QPainter &painter) {
        // A rising trend over its filled area, on a chart card.
        const QRectF card(8, 8, 112, 112);
        painter.setPen(QPen(kOutline, 5));
        painter.setBrush(QColor(0xf4, 0xf4, 0xf6));
        painter.drawRoundedRect(card, 16, 16);
        const QPolygonF trend({QPointF(24, 88), QPointF(48, 62), QPointF(72, 74), QPointF(104, 32)});
        QPolygonF area = trend;
        area << QPointF(104, 104) << QPointF(24, 104);
        QColor fill = zoneColor(6);
        fill.setAlpha(90);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPolygon(area);
        painter.setPen(QPen(zoneColor(6), 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(trend);
        painter.setPen(QPen(kOutline, 4));
        painter.setBrush(zoneColor(4));
        for (const QPointF &point : trend)
            painter.drawEllipse(point, 8, 8);
    });
}

QIcon SidebarIcons::devices()
{
    return drawn([](QPainter &painter) {
        painter.setPen(QPen(kOutline, 5));
        painter.setBrush(zoneColor(1));
        painter.drawRoundedRect(QRectF(26, 6, 76, 116), 36, 36);
        const QPointF rune[] = {{44, 44}, {84, 84}, {64, 104}, {64, 24}, {84, 44}, {44, 84}};
        painter.setPen(QPen(Qt::white, 9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(rune, 6);
    });
}

QIcon SidebarIcons::studio()
{
    return drawn([](QPainter &painter) {
        drawRider(painter, 30, 46, 0.85, zoneColor(1));
        drawRider(painter, 98, 46, 0.85, zoneColor(4));
        drawRider(painter, 64, 32, 1.1, zoneColor(2));
    });
}

QIcon SidebarIcons::intervals()
{
    return drawn([](QPainter &painter) {
        const QImage logo = QImage(QStringLiteral(":/image/icon/intervals")).convertToFormat(QImage::Format_ARGB32);
        QRect visible;
        for (int y = 0; y < logo.height(); ++y)
            for (int x = 0; x < logo.width(); ++x)
                if (qAlpha(logo.pixel(x, y)) > 40)
                    visible |= QRect(x, y, 1, 1);
        const QRectF area(10, 10, 108, 108);
        QPainterPath rounded;
        rounded.addRoundedRect(area, 16, 16);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setClipPath(rounded);
        painter.drawImage(area, logo, visible);
        painter.setClipping(false);
        painter.setPen(QPen(kOutline, 5));
        painter.drawPath(rounded);
    });
}

QIcon SidebarIcons::settings()
{
    return drawn([](QPainter &painter) {
        constexpr int kTeeth = 8;
        constexpr double kRoot = 44.0;
        constexpr double kTip = 58.0;
        const QPointF centre(64, 64);
        const auto at = [&](double radius, double degrees) {
            return centre + QPointF(radius * qCos(qDegreesToRadians(degrees)), radius * qSin(qDegreesToRadians(degrees)));
        };
        QPainterPath gear;
        for (int tooth = 0; tooth < kTeeth; ++tooth) {
            const double angle = tooth * 360.0 / kTeeth;
            tooth == 0 ? gear.moveTo(at(kRoot, angle - 14)) : gear.lineTo(at(kRoot, angle - 14));
            gear.lineTo(at(kTip, angle - 9));
            gear.lineTo(at(kTip, angle + 9));
            gear.lineTo(at(kRoot, angle + 14));
        }
        gear.closeSubpath();
        gear.addEllipse(centre, 19, 19);   // odd-even fill: the hole
        painter.setPen(QPen(kOutline, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor(0xc9, 0xd0, 0xd8));
        painter.drawPath(gear);
    });
}
