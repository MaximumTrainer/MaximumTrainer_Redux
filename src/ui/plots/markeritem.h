#ifndef MARKERITEM_H
#define MARKERITEM_H

#include <QPainter>
#include "qwt_plot_item.h"

class MarkerItem : public QwtPlotItem
{
public:
    MarkerItem();
    void draw( QPainter *painter, const QwtScaleMap &, const QwtScaleMap &, const QRectF &canvasRect ) const override;

};



#endif // MARKERITEM_H
