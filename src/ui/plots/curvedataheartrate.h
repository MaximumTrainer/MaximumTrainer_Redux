#ifndef CURVEDATAHEARTRATE_H
#define CURVEDATAHEARTRATE_H

#include "qwt_series_data.h"
#include <qpointer.h>

class DataHeartRate;

class CurveDataHeartRate: public QwtSeriesData<QPointF>
{
public:
    const DataHeartRate &values() const;
    DataHeartRate &values();

    QPointF sample( size_t i ) const override;
    size_t size() const override;

    QRectF boundingRect() const override;
};


#endif // CURVEDATAHEARTRATE_H



