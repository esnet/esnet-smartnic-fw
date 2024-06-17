#ifndef INCLUDE_SFF_8024_H
#define INCLUDE_SFF_8024_H

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------------------------------------
enum sff_8024_identifier {
    sff_8024_identifier_UNKNOWN,
    sff_8024_identifier_GBIC,
    sff_8024_identifier_SOLDERED,
    sff_8024_identifier_SFP_PLUS,
    sff_8024_identifier_300_PIN_XBI,
    sff_8024_identifier_XENPAK,
    sff_8024_identifier_XFP,
    sff_8024_identifier_XFF,
    sff_8024_identifier_XFP_E,
    sff_8024_identifier_XPAK,
    sff_8024_identifier_X2,
    sff_8024_identifier_DWDM_SFP,
    sff_8024_identifier_QSFP,
    sff_8024_identifier_QSFP_PLUS,
    sff_8024_identifier_CXP,
    sff_8024_identifier_HD_4X,
    sff_8024_identifier_HD_8X,
    sff_8024_identifier_QSFP28,
    sff_8024_identifier_CXP2,
    sff_8024_identifier_CDFP_1_OR_2,
    sff_8024_identifier_HD_4X_FANOUT,
    sff_8024_identifier_HD_8X_FANOUT,
    sff_8024_identifier_CDFP_3,
    sff_8024_identifier_MICRO_QSFP,
    sff_8024_identifier_QSFP_DD,
    sff_8024_identifier_OSFP_8X,
    sff_8024_identifier_SFP_DD,
    sff_8024_identifier_DSFP,
    sff_8024_identifier_MINILINK_4X,
    sff_8024_identifier_MINILINK_8X,
    sff_8024_identifier_QSFP_PLUS_CMIS,
    sff_8024_identifier_SFP_DD_CMIS,
    sff_8024_identifier_SFP_PLUS_CMIS,
    sff_8024_identifier_OSFP_XD_CMIS,
    sff_8024_identifier_OIF_ELSFP_CMIS,
    sff_8024_identifier_CDFP_X4_CMIS,
    sff_8024_identifier_CDFP_X8_CMIS,
    sff_8024_identifier_CDFP_X16_CMIS,

    sff_8024_identifier_RESERVED_FIRST = 0x26,
    sff_8024_identifier_RESERVED_LAST = 0x7f,

    sff_8024_identifier_VENDOR_FIRST = 0x80,
    sff_8024_identifier_VENDOR_LAST = 0xff
};
const char* sff_8024_identifier_to_str(enum sff_8024_identifier identifier);

//--------------------------------------------------------------------------------------------------
enum sff_8024_extended_compliance {
    sff_8024_extended_compliance_UNSPECIFIED,
    sff_8024_extended_compliance_100G_AOC_BER_5,
    sff_8024_extended_compliance_100GBASE_SR4,
    sff_8024_extended_compliance_100GBASE_LR4,
    sff_8024_extended_compliance_100GBASE_ER4,
    sff_8024_extended_compliance_100GBASE_SR10,
    sff_8024_extended_compliance_100G_CWDM4,
    sff_8024_extended_compliance_100G_PSM4,
    sff_8024_extended_compliance_100G_ACC_BER_5,

    sff_8024_extended_compliance_OBSOLETE0,
    sff_8024_extended_compliance_RESERVED0,

    sff_8024_extended_compliance_50GBASE_CR2_RS_FEC,
    sff_8024_extended_compliance_50GBASE_CR2_BASE_R_FEC,
    sff_8024_extended_compliance_50GBASE_CR2_NO_FEC,
    sff_8024_extended_compliance_10M_ETH,

    sff_8024_extended_compliance_RESERVED1,

    // 0x10
    sff_8024_extended_compliance_40GBASE_ER4,
    sff_8024_extended_compliance_4X_10GBASE_SR,
    sff_8024_extended_compliance_40G_PSM4,
    sff_8024_extended_compliance_G959_1_P1I1_2D1,
    sff_8024_extended_compliance_G959_1_P1S1_2D2,
    sff_8024_extended_compliance_G959_1_P1L1_2D2,
    sff_8024_extended_compliance_10GBASE_T_SFI,
    sff_8024_extended_compliance_100G_CLR4,
    sff_8024_extended_compliance_100G_AOC_BER_12,
    sff_8024_extended_compliance_100G_ACC_BER_12,
    sff_8024_extended_compliance_100GE_DWDM2,
    sff_8024_extended_compliance_100G_WDM,
    sff_8024_extended_compliance_10GBASE_T_SR,
    sff_8024_extended_compliance_5GBASE_T,
    sff_8024_extended_compliance_2_5GBASE_T,
    sff_8024_extended_compliance_40G_SWDM4,
    // 0x20
    sff_8024_extended_compliance_100G_SWDM4,
    sff_8024_extended_compliance_100G_PAM4,
    sff_8024_extended_compliance_4WDM_10_MSA,
    sff_8024_extended_compliance_4WDM_20_MSA,
    sff_8024_extended_compliance_4WDM_40_MSA,
    sff_8024_extended_compliance_100GBASE_DR_NO_FEC,
    sff_8024_extended_compliance_100GBASE_FR1_NO_FEC,
    sff_8024_extended_compliance_100GBASE_LR1_NO_FEC,
    sff_8024_extended_compliance_100GBASE_SR1_NO_FEC,
    sff_8024_extended_compliance_100GBASE_SR1,
    sff_8024_extended_compliance_100GBASE_FR1,
    sff_8024_extended_compliance_100GBASE_LR1,
    sff_8024_extended_compliance_100G_LR1_20_MSA_NO_FEC,
    sff_8024_extended_compliance_100G_ER1_30_MSA_NO_FEC,
    sff_8024_extended_compliance_100G_ER1_40_MSA_NO_FEC,
    sff_8024_extended_compliance_100G_LR1_20_MSA,
    // 0x30
    sff_8024_extended_compliance_ACTIVE_COPPER_50GAUI_BER_6,
    sff_8024_extended_compliance_ACTIVE_OPTICAL_50GAUI_BER_6,
    sff_8024_extended_compliance_ACTIVE_COPPER_50GAUI_BER_5,
    sff_8024_extended_compliance_ACTIVE_OPTICAL_50GAUI_BER_5,
    sff_8024_extended_compliance_100G_ER1_30_MSA,
    sff_8024_extended_compliance_100G_ER1_40_MSA,
    sff_8024_extended_compliance_100GBASE_VR1,
    sff_8024_extended_compliance_10GBASE_BR,
    sff_8024_extended_compliance_25GBASE_BR,
    sff_8024_extended_compliance_50GBASE_BR,
    sff_8024_extended_compliance_100GBASE_VR1_NO_FEC,

    sff_8024_extended_compliance_RESERVED2_FIRST = 0x3b,
    sff_8024_extended_compliance_RESERVED2_LAST = 0x3e,

    sff_8024_extended_compliance_100GBASE_CR1,
    // 0x40
    sff_8024_extended_compliance_100GBASE_CR2,
    sff_8024_extended_compliance_100GBASE_SR2,
    sff_8024_extended_compliance_200GBASE_DR4,
    sff_8024_extended_compliance_200GBASE_FR4,
    sff_8024_extended_compliance_200G_PSM4,
    sff_8024_extended_compliance_50GBASE_LR,
    sff_8024_extended_compliance_200GBASE_LR4,
    sff_8024_extended_compliance_400GBASE_DR4,
    sff_8024_extended_compliance_400GBASE_FR4,
    sff_8024_extended_compliance_400GBASE_LR4_6,
    sff_8024_extended_compliance_50GBASE_ER,
    sff_8024_extended_compliance_400G_LR4_10,
    sff_8024_extended_compliance_400GBASE_ZR,

    sff_8024_extended_compliance_RESERVED3_FIRST = 0x4d,
    sff_8024_extended_compliance_RESERVED3_LAST = 0x7e,

    sff_8024_extended_compliance_256GFC_SW4,
    // 0x80
    sff_8024_extended_compliance_64GFC,
    sff_8024_extended_compliance_128GFC,

    sff_8024_extended_compliance_RESERVED4_FIRST = 0x82,
    sff_8024_extended_compliance_RESERVED4_LAST = 0xff,
};
const char* sff_8024_extended_compliance_to_str(enum sff_8024_extended_compliance compliance);

//--------------------------------------------------------------------------------------------------
enum sff_8024_connector_type {
    sff_8024_connector_type_UNSPECIFIED,
    sff_8024_connector_type_SC,
    sff_8024_connector_type_FIBRE_STYLE_1_COPPER,
    sff_8024_connector_type_FIBRE_STYLE_2_COPPER,
    sff_8024_connector_type_BNC_TNC,
    sff_8024_connector_type_FIBRE_COAX_HEADERS,
    sff_8024_connector_type_FIBRE_JACK,
    sff_8024_connector_type_LC,
    sff_8024_connector_type_MT_RJ,
    sff_8024_connector_type_MU,
    sff_8024_connector_type_SG,
    sff_8024_connector_type_OPTICAL_PIGTAIL,
    sff_8024_connector_type_MPO_1X12,
    sff_8024_connector_type_MPO_2X16,

    sff_8024_connector_type_RESERVED0_FIRST = 0x0e,
    sff_8024_connector_type_RESERVED0_LAST = 0x1f,

    sff_8024_connector_type_HSSDC_II,
    sff_8024_connector_type_COPPER_PIGTAIL,
    sff_8024_connector_type_RJ45,
    sff_8024_connector_type_NOT_SEPARABLE,
    sff_8024_connector_type_MXC_2X16,
    sff_8024_connector_type_CS,
    sff_8024_connector_type_SN,
    sff_8024_connector_type_MPO_2X12,
    sff_8024_connector_type_MPO_1X16,

    sff_8024_connector_type_RESERVED1_FIRST = 0x29,
    sff_8024_connector_type_RESERVED1_LAST = 0x7f,

    sff_8024_connector_type_VENDOR_FIRST = 0x80,
    sff_8024_connector_type_VENDOR_LAST = 0xff,
};
const char* sff_8024_connector_type_to_str(enum sff_8024_connector_type type);

//--------------------------------------------------------------------------------------------------
enum sff_8024_encoding {
    sff_8024_encoding_UNSPECIFIED,
    sff_8024_encoding_8B_10B,
    sff_8024_encoding_4B_5B,
    sff_8024_encoding_NRZ,
    sff_8024_encoding_SONET_SCRAMBLED,
    sff_8024_encoding_64B_66B,
    sff_8024_encoding_MANCHESTER,
    sff_8024_encoding_256B_257B,
    sff_8024_encoding_PAM4,

    sff_8024_encoding_RESERVED_FIRST = 0x09,
    sff_8024_encoding_RESERVED_LAST = 0xff,
};
const char* sff_8024_encoding_to_str(enum sff_8024_encoding encoding);

//--------------------------------------------------------------------------------------------------
enum sff_8024_xcvr_sub_type {
    sff_8024_xcvr_sub_type_UNSPECIFIED,
    sff_8024_xcvr_sub_type_TYPE_1,
    sff_8024_xcvr_sub_type_TYPE_2,
    sff_8024_xcvr_sub_type_TYPE_2A,
    sff_8024_xcvr_sub_type_TYPE_2B,

    sff_8024_xcvr_sub_type_RESERVED_FIRST = 0x05,
    sff_8024_xcvr_sub_type_RESERVED_LAST = 0xff
};
const char* sff_8024_xcvr_sub_type_to_str(enum sff_8024_xcvr_sub_type type);

//--------------------------------------------------------------------------------------------------
enum sff_8024_xcvr_fiber_face_type {
    sff_8024_xcvr_fiber_face_type_UNSPECIFIED,
    sff_8024_xcvr_fiber_face_type_PC_UPC,
    sff_8024_xcvr_fiber_face_type_APC,
};
const char* sff_8024_xcvr_fiber_face_type_to_str(enum sff_8024_xcvr_fiber_face_type type);

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_SFF_8024_H
