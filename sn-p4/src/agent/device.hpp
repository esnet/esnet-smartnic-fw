#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>
#include <vector>

#include "esnet_smartnic_toplevel.h"
#include "snp4.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
struct DevicePipeline {
    unsigned int id;
    void* handle;
    struct snp4_info_pipeline info;
};

//--------------------------------------------------------------------------------------------------
struct Device {
    string bus_id;
    volatile struct esnet_smartnic_bar2* bar2;
    vector<DevicePipeline*> pipelines;
};

#endif // DEVICE_HPP
