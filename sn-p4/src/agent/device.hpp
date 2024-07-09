#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>

#include "esnet_smartnic_toplevel.h"

using namespace std;

//--------------------------------------------------------------------------------------------------
struct Device {
    string bus_id;
    volatile struct esnet_smartnic_bar2* bar2;
};

#endif // DEVICE_HPP
