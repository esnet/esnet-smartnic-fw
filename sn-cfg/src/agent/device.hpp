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
    unsigned int napps;

    struct {
        struct stats_domain* domains[DeviceStatsDomain::NDOMAINS];
        DeviceStats* card;
        vector<DeviceStats*> hosts;
        vector<DeviceStats*> modules;
        vector<DeviceStats*> ports;
        DeviceStats* sw;
        vector<DeviceStats*> sysmons;
    } stats;
};

#endif // DEVICE_HPP
