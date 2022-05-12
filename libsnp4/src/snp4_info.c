#include <stdio.h>
#include "sdnet_0_defs.h"	/* XilVitisNetP4TargetConfig_sdnet_0 */

static const char * tabs(unsigned int indent)
{
  switch (indent) {
  case 0:  return "";
  case 1:  return "\t";
  case 2:  return "\t\t";
  case 3:  return "\t\t\t";
  case 4:  return "\t\t\t\t";
  case 5:  return "\t\t\t\t\t";
  case 6:  return "\t\t\t\t\t\t";
  default: return "\t\t\t\t\t\t\t";
  }
}

static const char * vitisnetp4_endian_str(XilVitisNetP4Endian endian)
{
  switch (endian) {
  case XIL_VITIS_NET_P4_LITTLE_ENDIAN: return "Little Endian";
  case XIL_VITIS_NET_P4_BIG_ENDIAN:    return "Big Endian";
  default: return "??";
  }
}

static const char * vitisnetp4_table_mode_str(XilVitisNetP4TableMode mode)
{
  switch (mode) {
  case XIL_VITIS_NET_P4_TABLE_MODE_BCAM:      return "BCAM";
  case XIL_VITIS_NET_P4_TABLE_MODE_STCAM:     return "STCAM";
  case XIL_VITIS_NET_P4_TABLE_MODE_TCAM:      return "TCAM";
  case XIL_VITIS_NET_P4_TABLE_MODE_DCAM:      return "DCAM";
  case XIL_VITIS_NET_P4_TABLE_MODE_TINY_BCAM: return "Tiny BCAM";
  case XIL_VITIS_NET_P4_TABLE_MODE_TINY_TCAM: return "Tiny TCAM";
  default: return "??";
  }
}

static const char * vitisnetp4_cam_optimization_type(XilVitisNetP4CamOptimizationType opt_type)
{
  switch (opt_type) {
  case XIL_VITIS_NET_P4_CAM_OPTIMIZE_NONE:    return "None";
  case XIL_VITIS_NET_P4_CAM_OPTIMIZE_RAM:     return "RAM";
  case XIL_VITIS_NET_P4_CAM_OPTIMIZE_LOGIC:   return "Logic";
  case XIL_VITIS_NET_P4_CAM_OPTIMIZE_ENTRIES: return "Entries";
  case XIL_VITIS_NET_P4_CAM_OPTIMIZE_MASKS:   return "Masks";
  default: return "??";
  }
}

static const char * vitisnetp4_cam_mem_type(XilVitisNetP4CamMemType mem_type)
{
  switch (mem_type) {
  case XIL_VITIS_NET_P4_CAM_MEM_AUTO: return "Auto";
  case XIL_VITIS_NET_P4_CAM_MEM_BRAM: return "BRAM";
  case XIL_VITIS_NET_P4_CAM_MEM_URAM: return "URAM";
  case XIL_VITIS_NET_P4_CAM_MEM_HBM:  return "HBM";
  case XIL_VITIS_NET_P4_CAM_MEM_RAM:  return "RAM";
  default: return "??";
  }
}

static void vitisnetp4_print_cam_config(XilVitisNetP4CamConfig *cc, unsigned int indent)
{
  printf("%sBaseAddr: 0x%08lx\n", tabs(indent), cc->BaseAddr);
  printf("%sFormatString: %s\n", tabs(indent), cc->FormatStringPtr);
  printf("%sNumEntries: %u\n", tabs(indent), cc->NumEntries);
  printf("%sRamFrequencyHz: %u\n", tabs(indent), cc->RamFrequencyHz);
  printf("%sLookupFrequencyHz: %u\n", tabs(indent), cc->LookupFrequencyHz);
  printf("%sLookupsPerSec: %u\n", tabs(indent), cc->LookupsPerSec);
  printf("%sResponseSizeBits: %u\n", tabs(indent), cc->ResponseSizeBits);
  printf("%sPrioritySizeBits: %u\n", tabs(indent), cc->PrioritySizeBits);
  printf("%sNumMasks: %u\n", tabs(indent), cc->NumMasks);
  printf("%sEndian: %s\n", tabs(indent), vitisnetp4_endian_str(cc->Endian));
  printf("%sMemType: %s\n", tabs(indent), vitisnetp4_cam_mem_type(cc->MemType));
  printf("%sRamSizeKbytes: %u\n", tabs(indent), cc->RamSizeKbytes);
  printf("%sOptimizationType: %s\n", tabs(indent), vitisnetp4_cam_optimization_type(cc->OptimizationType));
}

static void vitisnetp4_print_param(XilVitisNetP4Attribute *attr, unsigned int indent)
{
  printf("%s%3u %s\n", tabs(indent), attr->Value, attr->NameStringPtr);
}

static void vitisnetp4_print_action(XilVitisNetP4Action *a, unsigned int indent)
{
  printf("%sName: %s\n", tabs(indent), a->NameStringPtr);
  printf("%sParameters: [n=%u]\n", tabs(indent), a->ParamListSize);
  for (unsigned int pidx = 0; pidx < a->ParamListSize; pidx++) {
    XilVitisNetP4Attribute *attr = &a->ParamListPtr[pidx];
    vitisnetp4_print_param(attr, indent+1);
  }
}

static void vitisnetp4_print_table_config(XilVitisNetP4TableConfig *tc, unsigned int indent)
{
  printf("%sEndian: %s\n", tabs(indent), vitisnetp4_endian_str(tc->Endian));
  printf("%sMode: %s\n", tabs(indent), vitisnetp4_table_mode_str(tc->Mode));
  printf("%sKeySizeBits: %u\n", tabs(indent), tc->KeySizeBits);
  printf("%sCamConfig:\n", tabs(indent));
  vitisnetp4_print_cam_config(&tc->CamConfig, indent+1);
  printf("%sActionIdWidthBits: %u\n", tabs(indent), tc->ActionIdWidthBits);

  printf("%sActions: [n=%u]\n", tabs(indent), tc->ActionListSize);
  for (unsigned int aidx = 0; aidx < tc->ActionListSize; aidx++) {
    XilVitisNetP4Action *a = tc->ActionListPtr[aidx];
    vitisnetp4_print_action(a, indent+1);
  }
}

static void vitisnetp4_print_target_table_config(XilVitisNetP4TargetTableConfig *ttc, unsigned int indent)
{
  printf("%sName: %s\n", tabs(indent), ttc->NameStringPtr);

  printf("%sConfig:\n", tabs(indent));
  vitisnetp4_print_table_config(&ttc->Config, indent+1);
}

void snp4_print_target_config (void)
{
  struct XilVitisNetP4TargetConfig *tcfg = &XilVitisNetP4TargetConfig_sdnet_0;

  printf("Endian: %s\n", vitisnetp4_endian_str(tcfg->Endian));
  printf("Tables: [n=%u]\n", tcfg->TableListSize);
  for (unsigned int tidx = 0; tidx < tcfg->TableListSize; tidx++) {
    XilVitisNetP4TargetTableConfig *t = tcfg->TableListPtr[tidx];
    vitisnetp4_print_target_table_config(t, 0);
  }
}
