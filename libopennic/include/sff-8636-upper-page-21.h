#ifndef INCLUDE_SFF_8636_UPPER_PAGE_21_H
#define INCLUDE_SFF_8636_UPPER_PAGE_21_H

#include <assert.h>
#include <stdint.h>

#include "sff-8636-types.h"
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_21_param_threshold {
    union sff_8636_u16 high;
    union sff_8636_u16 low;
} __attribute__((packed));

struct sff_8636_upper_page_21_param_thresholds {
    struct sff_8636_upper_page_21_param_threshold alarm;
    struct sff_8636_upper_page_21_param_threshold warning;
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_21 {
    struct sff_8636_upper_page_21_param_thresholds param_thresholds[16]; // 128-255
} __attribute__((packed));
static_assert(sizeof(struct sff_8636_upper_page_21) == SFF_8636_PAGE_SIZE,
              "SFF-8636 upper page 21 must be " STRINGIZE1(SFF_8636_PAGE_SIZE) " bytes.");

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_UPPER_PAGE_21_H
