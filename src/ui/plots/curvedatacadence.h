#ifndef CURVEDATACADENCE_H
#define CURVEDATACADENCE_H

#include "qwt_series_data.h"
#include <qpointer.h>


class DataCadence;

class CurveDataCadence: public QwtSeriesData<QPointF>
{
public:
    const DataCadence &values() const;
    DataCadence &values();

    QPointF sample( size_t i ) const override;
    size_t size() const override;

    QRectF boundingRect() const override;
};

#endif // CURVEDATACADENCE_H








