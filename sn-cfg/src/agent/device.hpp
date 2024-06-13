#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>
#include <vector>

#include "esnet_smartnic_toplevel.h"
#include "stats.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
enum DeviceStatsDomain {
    COUNTERS,
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

    unsigned int nhosts;
    unsigned int nports;
    unsigned int napps;

    struct {
        struct stats_domain* domains[DeviceStatsDomain::NDOMAINS];
        vector<DeviceStats*> hosts;
        vector<DeviceStats*> ports;
        DeviceStats* sw;
    } stats;
};

#endif // DEVICE_HPP
