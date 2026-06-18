#include "iconitem.h"
#include "util.h"

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

    QPointF p(canvasRect.center().x()+30, canvasRect.top());
    painter->drawPixmap(p, pixmap);

}
