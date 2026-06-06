#ifndef DATAPOWER_H
#define DATAPOWER_H

#include "datametric.h"

class DataPower final : public DataMetricSingleton<DataPower>
{
    friend class DataMetricSingleton<DataPower>;
    DataPower() {}
};

#endif // DATAPOWER_H
