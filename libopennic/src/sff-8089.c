#include "sff-8089.h"

//--------------------------------------------------------------------------------------------------
const char* sff_8089_application_category_to_str(enum sff_8089_application_category category) {
    const char* str = "UNKNOWN";
    switch (category) {
#define CASE_CAT(_name, _text) case sff_8089_application_category_##_name: str = _text; break
    CASE_CAT(CUSTOM, "");
    CASE_CAT(FIBRE_CHANNEL, "");
    CASE_CAT(ETHERNET, "");
    CASE_CAT(SONET_SDH, "");
    CASE_CAT(INFINIBAND, "");
    CASE_CAT(SBCON, "");
    CASE_CAT(COPPER, "");

    case sff_8089_application_category_RESERVED_FIRST ... sff_8089_application_category_RESERVED_LAST:
        str = "RESERVED";
        break;
#undef CASE_CAT
    }

    return str;
}
