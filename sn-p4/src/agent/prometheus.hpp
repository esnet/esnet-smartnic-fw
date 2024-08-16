#ifndef PROMETHEUS_HPP
#define PROMETHEUS_HPP

// The prometheus C client headers are not C++ friendly.
extern "C" {
#include <prom.h>
#include <promhttp.h>
}

#endif // PROMETHEUS_HPP
