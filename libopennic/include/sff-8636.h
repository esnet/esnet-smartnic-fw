#ifndef INCLUDE_SFF_8636_H
#define INCLUDE_SFF_8636_H

#include <assert.h>
#include <stdint.h>

#include "sff-8024.h"
#include "sff-8636-lower-page.h"
#include "sff-8636-upper-page-00.h"
#include "sff-8636-upper-page-01.h"
#include "sff-8636-upper-page-02.h"
#include "sff-8636-upper-page-03.h"
#include "sff-8636-upper-page-20.h"
#include "sff-8636-upper-page-21.h"
#include "sff-8636-types.h"
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
union sff_8636_page_words {
    uint8_t u8[SFF_8636_PAGE_SIZE];
    uint16_t u16[SFF_8636_PAGE_SIZE / sizeof(uint16_t)];
    uint32_t u32[SFF_8636_PAGE_SIZE / sizeof(uint32_t)];
};

//--------------------------------------------------------------------------------------------------
union sff_8636_page {
    union sff_8636_page_words words;
    union {
        struct sff_8636_lower_page page;
    } lower;
    union {
        struct sff_8636_upper_page_00 page_00;
        struct sff_8636_upper_page_01 page_01;
        struct sff_8636_upper_page_02 page_02;
        struct sff_8636_upper_page_03 page_03;
        struct sff_8636_upper_page_20 page_20;
        struct sff_8636_upper_page_21 page_21;
    } upper;
} __attribute__((packed));
static_assert(sizeof(union sff_8636_page) == SFF_8636_PAGE_SIZE,
              "SFF-8636 pages must be " STRINGIZE1(SFF_8636_PAGE_SIZE) " bytes.");

//--------------------------------------------------------------------------------------------------
struct sff_8636_memory {
    union {
        union sff_8636_page_words words;
        struct sff_8636_lower_page page;
    } lower;

    union {
        union sff_8636_page_words words;
        struct sff_8636_upper_page_00 page_00;
        struct sff_8636_upper_page_01 page_01;
        struct sff_8636_upper_page_02 page_02;
        struct sff_8636_upper_page_03 page_03;
        struct sff_8636_upper_page_20 page_20;
        struct sff_8636_upper_page_21 page_21;
    } upper;
} __attribute__((packed));
static_assert(sizeof(struct sff_8636_memory) == SFF_8636_MEMORY_SIZE,
              "SFF-8636 memory must be " STRINGIZE1(SFF_8636_MEMORY_SIZE) " bytes.");

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_H
