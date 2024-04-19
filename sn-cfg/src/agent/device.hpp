#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>
#include <vector>

#include "esnet_smartnic_toplevel.h"
#include "stats.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
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
        struct stats_domain* domain;
        vector<DeviceStats*> hosts;
    } stats;
};

#endif // DEVICE_HPP
