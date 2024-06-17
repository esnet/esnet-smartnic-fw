#ifndef INCLUDE_SFF_8636_UPPER_PAGE_01_H
#define INCLUDE_SFF_8636_UPPER_PAGE_01_H

#include <assert.h>
#include <stdint.h>

#include "sff-8636-types.h"
#include "stringize.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
union sff_8636_upper_page_01_ast_header {
    struct {
        uint8_t length   : 6; // [5:0]
        uint8_t reserved : 2; // [7:6]
    };
    uint8_t _v;
} __attribute__((packed));

union sff_8636_upper_page_01_ast_code {
    struct {
        uint16_t category : 6; // [5:0] (enum sff_8089_application_category)
        uint16_t reserved : 2; // [7:6]
        uint16_t variant  : 8; // [15:8] (enum sff_8089_application_variant_<category>)
    };
    uint16_t _v;
} __attribute__((packed));

struct sff_8636_upper_page_01_ast {
    union sff_8636_upper_page_01_ast_header header;  // 129
    union sff_8636_upper_page_01_ast_code codes[63]; // 130-255
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct sff_8636_upper_page_01 {
    uint8_t cc_apps;                       // 128
    struct sff_8636_upper_page_01_ast ast; // 129-255
} __attribute__((packed));
static_assert(sizeof(struct sff_8636_upper_page_01) == SFF_8636_PAGE_SIZE,
              "SFF-8636 upper page 01 must be " STRINGIZE1(SFF_8636_PAGE_SIZE) " bytes.");

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8636_UPPER_PAGE_01_H
