#ifndef ICONITEM_H
#define ICONITEM_H

#include <QPainter>
#include <QFont>
#include <QString>
#include "qwt_plot_item.h"

class IconItem : public QwtPlotItem
{
public:
    IconItem();
    void draw( QPainter *painter, const QwtScaleMap &, const QwtScaleMap &, const QRectF &canvasRect ) const;
    QString iconType;

    // The live value drawn beside the icon (the big centred number) and its
    // font, so the icon can sit immediately to the LEFT of that number and be
    // vertically centred on it — matching the detailed InfoWidget layout.
    QString valueText = "0";
    QFont   valueFont;
};

#endif // ICONITEM_H
