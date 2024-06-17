#ifndef INCLUDE_SFF_8636_UPPER_PAGE_02_H
#define INCLUDE_SFF_8636_UPPER_PAGE_02_H

#include <assert.h>
#include <stdint.h>

#include "sff-8636-types.h"
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_02 {
    char clei[10];       // 128-137
    uint8_t eeprom[118]; // 138-255
} __attribute__((packed));
static_assert(sizeof(struct sff_8636_upper_page_02) == SFF_8636_PAGE_SIZE,
              "SFF-8636 upper page 02 must be " STRINGIZE1(SFF_8636_PAGE_SIZE) " bytes.");

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_UPPER_PAGE_02_H
