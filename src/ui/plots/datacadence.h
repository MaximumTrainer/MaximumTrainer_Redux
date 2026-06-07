#ifndef DATACADENCE_H
#define DATACADENCE_H

#include "datametric.h"

class DataCadence final : public DataMetricSingleton<DataCadence>
{
    friend class DataMetricSingleton<DataCadence>;
    DataCadence() {}
};

#endif // DATACADENCE_H
