#include <stdio.h>
#include "sdnet_0_defs.h"	/* XilSdnetTargetConfig_sdnet_0 */

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

static const char * sdnet_endian_str(XilSdnetEndian endian)
{
  switch (endian) {
  case XIL_SDNET_LITTLE_ENDIAN: return "Little Endian";
  case XIL_SDNET_BIG_ENDIAN:    return "Big Endian";
  default: return "??";
  }
}

static const char * sdnet_table_mode_str(XilSdnetTableMode mode)
{
  switch (mode) {
  case XIL_SDNET_TABLE_MODE_BCAM:      return "BCAM";
  case XIL_SDNET_TABLE_MODE_STCAM:     return "STCAM";
  case XIL_SDNET_TABLE_MODE_TCAM:      return "TCAM";
  case XIL_SDNET_TABLE_MODE_DCAM:      return "DCAM";
  case XIL_SDNET_TABLE_MODE_TINY_BCAM: return "Tiny BCAM";
  case XIL_SDNET_TABLE_MODE_TINY_TCAM: return "Tiny TCAM";
  default: return "??";
  }
}

static const char * sdnet_cam_optimization_type(XilSdnetCamOptimizationType opt_type)
{
  switch (opt_type) {
  case XIL_SDNET_CAM_OPTIMIZE_NONE:    return "None";
  case XIL_SDNET_CAM_OPTIMIZE_RAM:     return "RAM";
  case XIL_SDNET_CAM_OPTIMIZE_LOGIC:   return "Logic";
  case XIL_SDNET_CAM_OPTIMIZE_ENTRIES: return "Entries";
  case XIL_SDNET_CAM_OPTIMIZE_MASKS:   return "Masks";
  default: return "??";
  }
}

static const char * sdnet_cam_mem_type(XilSdnetCamMemType mem_type)
{
  switch (mem_type) {
  case XIL_SDNET_CAM_MEM_AUTO: return "Auto";
  case XIL_SDNET_CAM_MEM_BRAM: return "BRAM";
  case XIL_SDNET_CAM_MEM_URAM: return "URAM";
  case XIL_SDNET_CAM_MEM_HBM:  return "HBM";
  case XIL_SDNET_CAM_MEM_RAM:  return "RAM";
  default: return "??";
  }
}

static void print_cam_config(XilSdnetCamConfig *cc, unsigned int indent)
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
  printf("%sEndian: %s\n", tabs(indent), sdnet_endian_str(cc->Endian));
  printf("%sMemType: %s\n", tabs(indent), sdnet_cam_mem_type(cc->MemType));
  printf("%sRamSizeKbytes: %u\n", tabs(indent), cc->RamSizeKbytes);
  printf("%sOptimizationType: %s\n", tabs(indent), sdnet_cam_optimization_type(cc->OptimizationType));
}

static void print_param(XilSdnetAttribute *attr, unsigned int indent)
{
  printf("%s%3u %s\n", tabs(indent), attr->Value, attr->NameStringPtr);
}

static void print_action(XilSdnetAction *a, unsigned int indent)
{
  printf("%sName: %s\n", tabs(indent), a->NameStringPtr);
  printf("%sParameters: [n=%u]\n", tabs(indent), a->ParamListSize);
  for (unsigned int pidx = 0; pidx < a->ParamListSize; pidx++) {
    XilSdnetAttribute *attr = &a->ParamListPtr[pidx];
    print_param(attr, indent+1);
  }
}

static void print_table_config(XilSdnetTableConfig *tc, unsigned int indent)
{
  printf("%sEndian: %s\n", tabs(indent), sdnet_endian_str(tc->Endian));
  printf("%sMode: %s\n", tabs(indent), sdnet_table_mode_str(tc->Mode));
  printf("%sKeySizeBits: %u\n", tabs(indent), tc->KeySizeBits);
  printf("%sCamConfig:\n", tabs(indent));
  print_cam_config(&tc->CamConfig, indent+1);
  printf("%sActionIdWidthBits: %u\n", tabs(indent), tc->ActionIdWidthBits);

  printf("%sActions: [n=%u]\n", tabs(indent), tc->ActionListSize);
  for (unsigned int aidx = 0; aidx < tc->ActionListSize; aidx++) {
    XilSdnetAction *a = tc->ActionListPtr[aidx];
    print_action(a, indent+1);
  }
}

static void print_target_table_config(XilSdnetTargetTableConfig *ttc, unsigned int indent)
{
  printf("%sName: %s\n", tabs(indent), ttc->NameStringPtr);

  printf("%sConfig:\n", tabs(indent));
  print_table_config(&ttc->Config, indent+1);
}

void sdnet_print_target_config (void)
{
  struct XilSdnetTargetConfig *tcfg = &XilSdnetTargetConfig_sdnet_0;

  printf("Endian: %s\n", sdnet_endian_str(tcfg->Endian));
  printf("Tables: [n=%u]\n", tcfg->TableListSize);
  for (unsigned int tidx = 0; tidx < tcfg->TableListSize; tidx++) {
    XilSdnetTargetTableConfig *t = tcfg->TableListPtr[tidx];
    print_target_table_config(t, 0);
  }
}
