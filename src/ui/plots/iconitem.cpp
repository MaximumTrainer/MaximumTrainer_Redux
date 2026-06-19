#include "iconitem.h"
#include "util.h"

#include <QFontMetricsF>

IconItem::IconItem() {}




void IconItem::draw( QPainter *painter,
                               const QwtScaleMap &, const QwtScaleMap &,
                               const QRectF &canvasRect ) const

{

    QString path;
    if (iconType == "POWER")           path = ":/image/icon/power2";
    else if (iconType == "CADENCE")    path = ":/image/icon/crank2";
    else if (iconType == "HEART_RATE") path = ":/image/icon/heart2";
    else if (iconType == "SPEED")      path = ":/image/icon/speed";
    if (path.isEmpty())
        return;

    // Render crisp for the canvas' device pixel ratio (drawPixmap honours the
    // pixmap's DPR, so the icon stays a 35px logical box but isn't upscaled blurry).
    const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    const QPixmap pixmap = Util::loadIconForDpr(path, 35, dpr);
    const QSizeF iconSize = pixmap.deviceIndependentSize();

    // Sit immediately to the LEFT of the centred value number and vertically
    // centre on it (matching the detailed InfoWidget), rather than floating at a
    // fixed offset above. QwtPlotTextLabel draws the number centred horizontally,
    // inset from the top by its 5px margin.
    const QFontMetricsF fm(valueFont);
    const qreal textWidth  = fm.horizontalAdvance(valueText);
    const qreal textHeight = fm.height();
    const qreal margin     = 5.0;   // QwtPlotTextLabel default margin
    const qreal gap        = 6.0;   // space between icon and number

    const qreal textLeft = canvasRect.center().x() - textWidth / 2.0;
    const qreal textTop  = canvasRect.top() + margin;

    QPointF p(textLeft - gap - iconSize.width(),
              textTop + textHeight / 2.0 - iconSize.height() / 2.0);
    painter->drawPixmap(p, pixmap);

}
