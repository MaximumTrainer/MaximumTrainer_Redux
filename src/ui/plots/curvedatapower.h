#ifndef CURVEDATAPOWER_H
#define CURVEDATAPOWER_H

#include "qwt_series_data.h"
#include <qpointer.h>

class DataPower;

class CurveDataPower: public QwtSeriesData<QPointF>
{
public:
    const DataPower &values() const;
    DataPower &values();

    QPointF sample( size_t i ) const override;
    size_t size() const override;

    QRectF boundingRect() const override;
};

#endif // CURVEDATAPOWER_H


