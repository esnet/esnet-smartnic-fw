#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>

#include "esnet_smartnic_toplevel.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
struct Device {
    const string bus_id;
    unsigned int nhosts;
    unsigned int nports;
    unsigned int napps;
    volatile struct esnet_smartnic_bar2* bar2;
};

#endif // DEVICE_HPP
