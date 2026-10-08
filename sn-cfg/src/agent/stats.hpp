#ifndef STATS_HPP
#define STATS_HPP

#include "sn_cfg_v2.grpc.pb.h"

using namespace sn_cfg::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
ErrorCode get_stats_zone(struct stats_zone* zone, const StatsFilters& filters, Stats* stats);
ErrorCode clear_stats_zone(struct stats_zone* zone, const StatsFilters& filters);

#endif // STATS_HPP
