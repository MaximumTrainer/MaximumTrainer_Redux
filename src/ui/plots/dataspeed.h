#ifndef DATASPEED_H
#define DATASPEED_H

#include "datametric.h"

class DataSpeed final : public DataMetricSingleton<DataSpeed>
{
    friend class DataMetricSingleton<DataSpeed>;
    DataSpeed() {}
};

#endif // DATASPEED_H
