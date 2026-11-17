#ifndef CURVEDATASPEED_H
#define CURVEDATASPEED_H

#include "qwt_series_data.h"
#include <qpointer.h>

class DataSpeed;

class CurveDataSpeed: public QwtSeriesData<QPointF>
{
public:
    const DataSpeed &values() const;
    DataSpeed &values();

    QPointF sample( size_t i ) const override;
    size_t size() const override;

    QRectF boundingRect() const override;
};

#endif // CURVEDATASPEED_H





