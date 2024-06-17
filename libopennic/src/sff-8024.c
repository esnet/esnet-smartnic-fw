#include "sff-8024.h"

//--------------------------------------------------------------------------------------------------
const char* sff_8024_identifier_to_str(enum sff_8024_identifier identifier) {
    const char* str = "UNKNOWN";
    switch (identifier) {
#define CASE_ID(_name, _text) case sff_8024_identifier_##_name: str = _text; break
    CASE_ID(UNKNOWN, "Unknown or unspecified");
    CASE_ID(GBIC, "GBIC");
    CASE_ID(SOLDERED, "Module/connector soldered to motherboard (using SFF-8472)");
    CASE_ID(SFP_PLUS, "SFP/SFP+/SFP28 and later with SFF-8472 management interface");
    CASE_ID(300_PIN_XBI, "300 pin XBI");
    CASE_ID(XENPAK, "XENPAK";);
    CASE_ID(XFP, "XFP");
    CASE_ID(XFF, "XFF");
    CASE_ID(XFP_E, "XFP-E");
    CASE_ID(XPAK, "XPAK");
    CASE_ID(X2, "X2");
    CASE_ID(DWDM_SFP, "DWDM-SFP/SFP+ (not using SFF-8472)");
    CASE_ID(QSFP, "QSFP (INF-8438)");
    CASE_ID(QSFP_PLUS, "QSFP+ or later with SFF-8636 or SFF-8436 management interface");
    CASE_ID(CXP, "CXP or later");
    CASE_ID(HD_4X, "Shielded Mini Multilane HD 4X");
    CASE_ID(HD_8X, "Shielded Mini Multilane HD 8X");
    CASE_ID(QSFP28, "QSFP28 or later with SFF-8636 management interface");
    CASE_ID(CXP2, "CXP2 (aka CXP28) or later");
    CASE_ID(CDFP_1_OR_2, "CDFP (Style 1/Style2)");
    CASE_ID(HD_4X_FANOUT, "Shielded Mini Multilane HD 4X Fanout Cable");
    CASE_ID(HD_8X_FANOUT, "Shielded Mini Multilane HD 8X Fanout Cable");
    CASE_ID(CDFP_3, "CDFP (Style 3)");
    CASE_ID(MICRO_QSFP, "microQSFP");
    CASE_ID(QSFP_DD, "QSFP-DD Double Density 8X Pluggable Transceiver (INF-8628)");
    CASE_ID(OSFP_8X, "OSFP 8X Pluggable Transceiver");
    CASE_ID(SFP_DD,
            "SFP-DD Double Density 2X Pluggable Transceiver with SFP-DD Management "
            "Interface Specification");
    CASE_ID(DSFP, "DSFP Dual Small Form Factor Pluggable Transceiver");
    CASE_ID(MINILINK_4X, "x4 MiniLink/OcuLink");
    CASE_ID(MINILINK_8X, "x8 MiniLink");
    CASE_ID(QSFP_PLUS_CMIS,
            "QSFP+ or later with Common Management Interface Specification (CMIS)");
    CASE_ID(SFP_DD_CMIS,
            "SFP-DD Double Density 2X Pluggable Transceiver with Common Management Interface "
            "Specification (CMIS)";);
    CASE_ID(SFP_PLUS_CMIS, "SFP+ and later with Common Management Interface Specification (CMIS)");
    CASE_ID(OSFP_XD_CMIS, "OSFP-XD with Common Management Interface Specification (CMIS)");
    CASE_ID(OIF_ELSFP_CMIS, "OIF-ELSFP with Common Management Interface Specification (CMIS)");
    CASE_ID(CDFP_X4_CMIS,
            "CDFP (x4 PCIe) SFF-TA-1032 with Common Management Interface Specification (CMIS)");
    CASE_ID(CDFP_X8_CMIS,
            "CDFP (x8 PCIe) SFF-TA-1032 with Common Management Interface Specification (CMIS)");
    CASE_ID(CDFP_X16_CMIS,
            "CDFP (x16 PCIe) SFF-TA-1032 with Common Management Interface Specification (CMIS)");

    case sff_8024_identifier_VENDOR_FIRST ... sff_8024_identifier_VENDOR_LAST:
        str = "VENDOR";
        break;

    case sff_8024_identifier_RESERVED_FIRST ... sff_8024_identifier_RESERVED_LAST:
        str = "RESERVED";
        break;
#undef CASE_ID
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8024_extended_compliance_to_str(enum sff_8024_extended_compliance compliance) {
    const char* str = "UNKNOWN";
    switch (compliance) {
#define CASE_COMP(_name, _text) case sff_8024_extended_compliance_##_name: str = _text; break
    CASE_COMP(UNSPECIFIED, "Unspecified");
    CASE_COMP(100G_AOC_BER_5,
              "100G AOC (Active Optical Cable), retimed or 25GAUI C2M AOC. "
              "Providing a worst BER of 5 × 10-5");
    CASE_COMP(100GBASE_SR4, "100GBASE-SR4 or 25GBASE-SR");
    CASE_COMP(100GBASE_LR4, "100GBASE-LR4 or 25GBASE-LR");
    CASE_COMP(100GBASE_ER4, "100GBASE-ER4 or 25GBASE-ER");
    CASE_COMP(100GBASE_SR10, "100GBASE-SR10");
    CASE_COMP(100G_CWDM4, "100G CWDM4");
    CASE_COMP(100G_PSM4, "100G PSM4 Parallel SMF");
    CASE_COMP(100G_ACC_BER_5,
              "100G ACC (Active Copper Cable), retimed or 25GAUI C2M ACC. "
              "Providing a worst BER of 5 × 10-5");
    CASE_COMP(OBSOLETE0, "Obsolete (assigned before 100G CWDM4 MSA required FEC)");
    CASE_COMP(50GBASE_CR2_RS_FEC,
              "100GBASE-CR4, 25GBASE-CR CA-25G-L or 50GBASE-CR2 with RS (Clause91) FEC");
    CASE_COMP(50GBASE_CR2_BASE_R_FEC,
              "25GBASE-CR CA-25G-S or 50GBASE-CR2 with BASE-R (Clause 74 Fire code) FEC");
    CASE_COMP(50GBASE_CR2_NO_FEC, "25GBASE-CR CA-25G-N or 50GBASE-CR2 with no FEC");
    CASE_COMP(10M_ETH, "10 Mb/s Single Pair Ethernet (802.3cg, Clause 146/147, 1000 m copper)");
    CASE_COMP(40GBASE_ER4, "40GBASE-ER4");
    CASE_COMP(4X_10GBASE_SR, "4 x 10GBASE-SR");
    CASE_COMP(40G_PSM4, "40G PSM4 Parallel SMF");
    CASE_COMP(G959_1_P1I1_2D1, "G959.1 profile P1I1-2D1 (10709 MBd, 2km, 1310 nm SM)");
    CASE_COMP(G959_1_P1S1_2D2, "G959.1 profile P1S1-2D2 (10709 MBd, 40km, 1550 nm SM)");
    CASE_COMP(G959_1_P1L1_2D2, "G959.1 profile P1L1-2D2 (10709 MBd, 80km, 1550 nm SM)");
    CASE_COMP(10GBASE_T_SFI, "10GBASE-T with SFI electrical interfac");
    CASE_COMP(100G_CLR4, "100G CLR4");
    CASE_COMP(100G_AOC_BER_12,
              "100G AOC, retimed or 25GAUI C2M AOC. Providing a worst BER of 10-12 or below");
    CASE_COMP(100G_ACC_BER_12,
              "100G ACC, retimed or 25GAUI C2M ACC. Providing a worst BER of 10-12 or below");
    CASE_COMP(100GE_DWDM2,
              "100GE-DWDM2 (DWDM transceiver using 2 wavelengths on a 1550 nm DWDM "
              "grid with a reach up to 80 km)");
    CASE_COMP(100G_WDM, "100G 1550nm WDM (4 wavelengths)");
    CASE_COMP(10GBASE_T_SR, "10GBASE-T Short Reach (30 meters)");
    CASE_COMP(5GBASE_T, "5GBASE-T");
    CASE_COMP(2_5GBASE_T, "2.5GBASE-T");
    CASE_COMP(40G_SWDM4, "40G SWDM4");
    CASE_COMP(100G_SWDM4, "100G SWDM4");
    CASE_COMP(100G_PAM4, "100G PAM4 BiDi");
    CASE_COMP(4WDM_10_MSA,
              "4WDM-10 MSA (10km version of 100G CWDM4 with same RS(528,514) FEC in host system)");
    CASE_COMP(4WDM_20_MSA,
              "4WDM-20 MSA (20km version of 100GBASE-LR4 with RS(528,514) FEC in host system)");
    CASE_COMP(4WDM_40_MSA,
              "4WDM-40 MSA (40km reach with APD receiver and RS(528,514) FEC in host system)");
    CASE_COMP(100GBASE_DR_NO_FEC, "100GBASE-DR (Clause 140), CAUI-4 (no FEC)");
    CASE_COMP(100GBASE_FR1_NO_FEC,
              "100G-FR or 100GBASE-FR1 (Clause 140), CAUI-4 (no FEC on host interface)");
    CASE_COMP(100GBASE_LR1_NO_FEC,
              "100G-LR or 100GBASE-LR1 (Clause 140), CAUI-4 (no FEC on host interface)");
    CASE_COMP(100GBASE_SR1_NO_FEC,
              "100GBASE-SR1 (802.3, Clause 167), CAUI-4 (no FEC on host interface)");
    CASE_COMP(100GBASE_SR1, "100GBASE-SR1, 200GBASE-SR2 or 400GBASE-SR4 (802.3, Clause 167)");
    CASE_COMP(100GBASE_FR1,
              "100GBASE-FR1 (802.3, Clause 140) or 400GBASE-DR4-2 (P802.3df, Clause 124)");
    CASE_COMP(100GBASE_LR1, "100GBASE-LR1 (802.3, Clause 140)");
    CASE_COMP(100G_LR1_20_MSA_NO_FEC, "100G-LR1-20 MSA, CAUI-4 (no FEC on host interface)");
    CASE_COMP(100G_ER1_30_MSA_NO_FEC, "100G-ER1-30 MSA, CAUI-4 (no FEC on host interface)");
    CASE_COMP(100G_ER1_40_MSA_NO_FEC, "100G-ER1-40 MSA, CAUI-4 (no FEC on host interface)");
    CASE_COMP(100G_LR1_20_MSA, "100G-LR1-20 MSA");
    CASE_COMP(ACTIVE_COPPER_50GAUI_BER_6,
              "Active Copper Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. "
              "Providing a worst BER of 10-6 or below");
    CASE_COMP(ACTIVE_OPTICAL_50GAUI_BER_6,
              "Active Optical Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. "
              "Providing a worst BER of 10-6 or below");
    CASE_COMP(ACTIVE_COPPER_50GAUI_BER_5,
              "Active Copper Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. "
              "Providing a worst BER of 2.6x10-4 for ACC, 10-5 for AUI, or below");
    CASE_COMP(ACTIVE_OPTICAL_50GAUI_BER_5,
              "Active Optical Cable with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. "
              "Providing a worst BER of 2.6x10-4 for AOC, 10-5 for AUI, or below");
    CASE_COMP(100G_ER1_30_MSA, "100G-ER1-30 MSA");
    CASE_COMP(100G_ER1_40_MSA, "100G-ER1-40 MSA");
    CASE_COMP(100GBASE_VR1, "100GBASE-VR1, 200GBASE-VR2 or 400GBASE-VR4 (802.3, Clause 167)");
    CASE_COMP(10GBASE_BR, "10GBASE-BR (Clause 158)");
    CASE_COMP(25GBASE_BR, "25GBASE-BR (Clause 159)");
    CASE_COMP(50GBASE_BR, "50GBASE-BR (Clause 160)");
    CASE_COMP(100GBASE_VR1_NO_FEC,
              "100GBASE-VR1 (802.3, Clause 167), CAUI-4 (no FEC on host interface)");
    CASE_COMP(100GBASE_CR1, "100GBASE-CR1, 200GBASE-CR2 or 400GBASE-CR4 (P802.3ck, Clause 162)");
    CASE_COMP(100GBASE_CR2, "50GBASE-CR, 100GBASE-CR2, or 200GBASE-CR4");
    CASE_COMP(100GBASE_SR2, "50GBASE-SR, 100GBASE-SR2, or 200GBASE-SR4");
    CASE_COMP(200GBASE_DR4, "50GBASE-FR or 200GBASE-DR4");
    CASE_COMP(200GBASE_FR4, "200GBASE-FR4");
    CASE_COMP(200G_PSM4, "200G 1550 nm PSM4");
    CASE_COMP(50GBASE_LR, "50GBASE-LR");
    CASE_COMP(200GBASE_LR4, "200GBASE-LR4");
    CASE_COMP(400GBASE_DR4, "400GBASE-DR4 (802.3, Clause 124), 400GAUI-4 C2M (Annex 120G)");
    CASE_COMP(400GBASE_FR4, "400GBASE-FR4 (802.3, Clause 151)");
    CASE_COMP(400GBASE_LR4_6, "400GBASE-LR4-6 (802.3, Clause 151)");
    CASE_COMP(50GBASE_ER, "50GBASE-ER (IEEE 802.3, Clause 139)");
    CASE_COMP(400G_LR4_10, "400G-LR4-10");
    CASE_COMP(400GBASE_ZR, "400GBASE-ZR (P802.3cw, Clause 156)");
    CASE_COMP(256GFC_SW4, "256GFC-SW4 (FC-PI-7P)");
    CASE_COMP(64GFC, "64GFC (FC-PI-7)");
    CASE_COMP(128GFC, "128GFC (FC-PI-8)");

    case sff_8024_extended_compliance_RESERVED0:
    case sff_8024_extended_compliance_RESERVED1:
    case sff_8024_extended_compliance_RESERVED2_FIRST ...
         sff_8024_extended_compliance_RESERVED2_LAST:
    case sff_8024_extended_compliance_RESERVED3_FIRST ...
         sff_8024_extended_compliance_RESERVED3_LAST:
    case sff_8024_extended_compliance_RESERVED4_FIRST ...
         sff_8024_extended_compliance_RESERVED4_LAST:
        str = "RESERVED";
        break;
#undef CASE_COMP
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8024_connector_type_to_str(enum sff_8024_connector_type type) {
    const char* str = "UNKNOWN";
    switch (type) {
#define CASE_TYPE(_name, _text) case sff_8024_connector_type_##_name: str = _text; break
    CASE_TYPE(UNSPECIFIED, "Unspecified");
    CASE_TYPE(SC, "SC (Subscriber Connector)");
    CASE_TYPE(FIBRE_STYLE_1_COPPER, "Fibre Channel Style 1 copper connector");
    CASE_TYPE(FIBRE_STYLE_2_COPPER, "Fibre Channel Style 2 copper connector");
    CASE_TYPE(BNC_TNC, "BNC/TNC (Bayonet/Threaded Neill-Concelman)");
    CASE_TYPE(FIBRE_COAX_HEADERS, "Fibre Channel coax headers");
    CASE_TYPE(FIBRE_JACK, "Fiber Jack");
    CASE_TYPE(LC, "LC (Lucent Connector)");
    CASE_TYPE(MT_RJ, "MT-RJ (Mechanical Transfer – Registered Jack)");
    CASE_TYPE(MU, "MU (Multiple Optical)");
    CASE_TYPE(SG, "SG");
    CASE_TYPE(OPTICAL_PIGTAIL, "Optical Pigtail");
    CASE_TYPE(MPO_1X12, "MPO 1x12 (Multifiber Parallel Optic)");
    CASE_TYPE(MPO_2X16, "MPO 2x16 (Multifiber Parallel Optic)");
    CASE_TYPE(HSSDC_II, "HSSDC II (High Speed Serial Data Connector)");
    CASE_TYPE(COPPER_PIGTAIL, "Copper pigtail");
    CASE_TYPE(RJ45, "RJ45 (Registered Jack)");
    CASE_TYPE(NOT_SEPARABLE, "No separable connector");
    CASE_TYPE(MXC_2X16, "MXC 2x16");
    CASE_TYPE(CS, "CS optical connector");
    CASE_TYPE(SN, "SN (previously Mini CS) optical connector");
    CASE_TYPE(MPO_2X12, "MPO 2x12 (Multifiber Parallel Optic)");
    CASE_TYPE(MPO_1X16, "MPO 1x16 (Multifiber Parallel Optic)");

    case sff_8024_connector_type_VENDOR_FIRST ... sff_8024_connector_type_VENDOR_LAST:
        str = "VENDOR";
        break;

    case sff_8024_connector_type_RESERVED0_FIRST ... sff_8024_connector_type_RESERVED0_LAST:
    case sff_8024_connector_type_RESERVED1_FIRST ... sff_8024_connector_type_RESERVED1_LAST:
        str = "RESERVED";
        break;
#undef CASE_TYPE
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8024_encoding_to_str(enum sff_8024_encoding encoding) {
    const char* str = "UNKNOWN";
    switch (encoding) {
#define CASE_ENC(_name, _text) case sff_8024_encoding_##_name: str = _text; break
    CASE_ENC(UNSPECIFIED, "Unspecified");
    CASE_ENC(8B_10B, "8B/10B");
    CASE_ENC(4B_5B, "4B/5B");
    CASE_ENC(NRZ, "NRZ");
    CASE_ENC(SONET_SCRAMBLED, "SONET Scrambled");
    CASE_ENC(64B_66B, "64B/66B");
    CASE_ENC(MANCHESTER, "Manchester");
    CASE_ENC(256B_257B, "256B/257B (transcoded FEC-enabled data)");
    CASE_ENC(PAM4, "PAM4");

    case sff_8024_encoding_RESERVED_FIRST ... sff_8024_encoding_RESERVED_LAST:
        str = "RESERVED";
        break;
#undef CASE_ENC
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8024_xcvr_sub_type_to_str(enum sff_8024_xcvr_sub_type type) {
    const char* str = "UNKNOWN";
    switch (type) {
#define CASE_TYPE(_name, _text) case sff_8024_xcvr_sub_type_##_name: str = _text; break
    CASE_TYPE(UNSPECIFIED, "Unspecified");
    CASE_TYPE(TYPE_1, "Type 1");
    CASE_TYPE(TYPE_2, "Type 2");
    CASE_TYPE(TYPE_2A, "Type 2A");
    CASE_TYPE(TYPE_2B, "Type 2B");

    case sff_8024_xcvr_sub_type_RESERVED_FIRST ... sff_8024_xcvr_sub_type_RESERVED_LAST:
        str = "RESERVED";
        break;
#undef CASE_TYPE
    }

    return str;
}

//--------------------------------------------------------------------------------------------------
const char* sff_8024_xcvr_fiber_face_type_to_str(enum sff_8024_xcvr_fiber_face_type type) {
    const char* str = "UNKNOWN";
    switch (type) {
#define CASE_TYPE(_name, _text) case sff_8024_xcvr_fiber_face_type_##_name: str = _text; break
    CASE_TYPE(UNSPECIFIED, "Unspecified");
    CASE_TYPE(PC_UPC, "Physical/Ultra Physical Contact");
    CASE_TYPE(APC, "Angled Physical Contact");
#undef CASE_TYPE
    }

    return str;
}
