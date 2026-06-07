#ifndef DATAHEARTRATE_H
#define DATAHEARTRATE_H

#include "datametric.h"

class DataHeartRate final : public DataMetricSingleton<DataHeartRate>
{
    friend class DataMetricSingleton<DataHeartRate>;
    DataHeartRate() {}
};

#endif // DATAHEARTRATE_H
