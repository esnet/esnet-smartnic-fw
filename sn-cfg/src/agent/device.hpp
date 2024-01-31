#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>

using namespace std;

//--------------------------------------------------------------------------------------------------
struct Device {
    const string bus_id;
    unsigned int nhosts;
    unsigned int nports;
    void* base;
};

#endif // DEVICE_HPP
