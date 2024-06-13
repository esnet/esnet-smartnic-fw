#ifndef STATS_HPP
#define STATS_HPP

#include <bitset>

#include "sn_cfg_v1.grpc.pb.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
struct GetStatsContext {
    Stats* stats;
    bitset<StatsMetricType_MAX + 1> metric_types;
};

extern "C" {
int get_stats_for_each_metric(const struct stats_for_each_spec* spec);
}

#endif // STATS_HPP
