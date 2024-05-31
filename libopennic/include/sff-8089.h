#ifndef INCLUDE_SFF_8089_H
#define INCLUDE_SFF_8089_H

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
enum sff_8089_application_category {
    sff_8089_application_category_CUSTOM,
    sff_8089_application_category_FIBRE_CHANNEL,
    sff_8089_application_category_ETHERNET,
    sff_8089_application_category_SONET_SDH,
    sff_8089_application_category_INFINIBAND,
    sff_8089_application_category_SBCON,
    sff_8089_application_category_COPPER,

    sff_8089_application_category_RESERVED_FIRST = 7,
    sff_8089_application_category_RESERVED_LAST = 20,
};
const char* sff_8089_application_category_to_str(enum sff_8089_application_category category);

/*
 * TODO: add per-category variant enums
 *
 * enum sff_8089_application_variant_fibre_channel
 * enum sff_8089_application_variant_ethernet
 * enum sff_8089_application_variant_sonet_sdh
 * enum sff_8089_application_variant_infiniband
 * enum sff_8089_application_variant_sbcon
 * enum sff_8089_application_variant_copper
*/

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8089_H
