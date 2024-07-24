#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>
#include <vector>

#include "esnet_smartnic_toplevel.h"
#include "snp4.h"
#include "stats.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
enum DeviceStatsDomain {
    COUNTERS,
    NDOMAINS
};

struct DeviceStats {
    string zone_name;
    vector<string*> strings;
    vector<void*> io_data;
    struct stats_zone* zone;
};

//--------------------------------------------------------------------------------------------------
struct DevicePipeline {
    unsigned int id;
    void* handle;
    struct snp4_info_pipeline info;

    struct {
        DeviceStats* counters;
        DeviceStats* table_ecc;
    } stats;
};

//--------------------------------------------------------------------------------------------------
struct Device {
    string bus_id;
    volatile struct esnet_smartnic_bar2* bar2;
    vector<DevicePipeline*> pipelines;

    struct {
        struct stats_domain* domains[DeviceStatsDomain::NDOMAINS];
    } stats;
};

#endif // DEVICE_HPP
