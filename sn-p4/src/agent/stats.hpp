#ifndef STATS_HPP
#define STATS_HPP

#include "sn_p4_v2.grpc.pb.h"

using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
ErrorCode get_stats_zone(struct stats_zone* zone, const StatsFilters& filters, Stats* stats);
ErrorCode clear_stats_zone(struct stats_zone* zone, const StatsFilters& filters);

#endif // STATS_HPP
