#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>
#include <vector>

#include "cms.h"
#include "esnet_smartnic_toplevel.h"
#include "stats.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
enum DeviceStatsDomain {
    COUNTERS,
    MONITORS,
    MODULES,
    NDOMAINS
};
const char* device_stats_domain_name(DeviceStatsDomain dom);

enum DeviceStatsZone {
    CARD_MONITORS,
    SYSMON_MONITORS,
    HOST_COUNTERS,
    PORT_COUNTERS,
    SWITCH_COUNTERS,
    MODULE_MONITORS,
    NZONES
};

struct DeviceStats {
    string name;
    struct stats_zone* zone;
};

//--------------------------------------------------------------------------------------------------
struct Device {
    string bus_id;
    volatile struct esnet_smartnic_bar2* bar2;
    struct cms cms;

    unsigned int nhosts;
    unsigned int nports;

    struct {
        struct stats_domain* domains[DeviceStatsDomain::NDOMAINS];
        vector<DeviceStats*> zones[DeviceStatsZone::NZONES];
    } stats;
};

#endif // DEVICE_HPP
