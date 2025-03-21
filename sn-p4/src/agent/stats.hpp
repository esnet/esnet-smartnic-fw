#ifndef STATS_HPP
#define STATS_HPP

#include "sn_p4_v2.grpc.pb.h"

using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
struct GetStatsContext {
    const StatsFilters& filters;
    Stats* stats;
};

extern "C" {
int get_stats_for_each_metric(const struct stats_for_each_spec* spec);
}

#endif // STATS_HPP
