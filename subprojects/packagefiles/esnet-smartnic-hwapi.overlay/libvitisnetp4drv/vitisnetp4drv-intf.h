#ifndef VITISNETP4DRV_INTF_H
#define VITISNETP4DRV_INTF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WITH_VITISNETP4_STUBS
#include <stdbool.h>

/*
 * This section stubs out the vitisnetp4 driver definitions used by the wrapper
 * layer in the event that it's being compiled in the absence of p4 IP in the
 * FPGA.  Stubbed values don't matter since they are only used as a translation
 * layer for structures and registers between the wrapper and lower level driver
 * generated for the p4 IP (without IP, there's nothing to translate!).
 */

/*----------------------------------------------------------------------------*/
// vitisnetp4_common.h
typedef enum {
    XIL_VITIS_NET_P4_SUCCESS,
    XIL_VITIS_NET_P4_CAM_ERR_KEY_NOT_FOUND,
    XIL_VITIS_NET_P4_TABLE_ERR_FUNCTION_NOT_SUPPORTED,
    XIL_VITIS_NET_P4_GENERAL_ERR_NULL_PARAM,
    XIL_VITIS_NET_P4_GENERAL_ERR_INTERNAL_ASSERTION,
}  XilVitisNetP4ReturnType;

typedef enum {
    XIL_VITIS_NET_P4_LITTLE_ENDIAN,
    XIL_VITIS_NET_P4_BIG_ENDIAN,
} XilVitisNetP4Endian;

typedef void* XilVitisNetP4UserCtxType;
typedef uintptr_t XilVitisNetP4AddressType;
typedef struct _XilVitisNetP4EnvIf XilVitisNetP4EnvIf;
typedef XilVitisNetP4ReturnType (*XilVitisNetP4WordWrite32Fp)(
    XilVitisNetP4EnvIf*, XilVitisNetP4AddressType, uint32_t);
typedef XilVitisNetP4ReturnType (*XilVitisNetP4WordRead32Fp)(
    XilVitisNetP4EnvIf*, XilVitisNetP4AddressType, uint32_t*);
typedef XilVitisNetP4ReturnType (*XilVitisNetP4LogFp)(
    XilVitisNetP4EnvIf*, const char*);

struct _XilVitisNetP4EnvIf {
    XilVitisNetP4UserCtxType UserCtx;
    XilVitisNetP4WordWrite32Fp WordWrite32;
    XilVitisNetP4WordRead32Fp WordRead32;
    XilVitisNetP4LogFp LogError;
    XilVitisNetP4LogFp LogInfo;
};

typedef enum {
    XIL_VITIS_NET_P4_CAM_OPTIMIZE_NONE,
    XIL_VITIS_NET_P4_CAM_OPTIMIZE_RAM,
    XIL_VITIS_NET_P4_CAM_OPTIMIZE_LOGIC,
    XIL_VITIS_NET_P4_CAM_OPTIMIZE_ENTRIES,
    XIL_VITIS_NET_P4_CAM_OPTIMIZE_MASKS,
} XilVitisNetP4CamOptimizationType;

typedef enum {
    XIL_VITIS_NET_P4_CAM_MEM_AUTO,
    XIL_VITIS_NET_P4_CAM_MEM_BRAM,
    XIL_VITIS_NET_P4_CAM_MEM_URAM,
    XIL_VITIS_NET_P4_CAM_MEM_HBM,
    XIL_VITIS_NET_P4_CAM_MEM_RAM,
} XilVitisNetP4CamMemType;

typedef struct {
    XilVitisNetP4AddressType BaseAddr;
    char *FormatStringPtr;
    uint32_t NumEntries;
    uint32_t RamFrequencyHz;
    uint32_t LookupFrequencyHz;
    uint32_t LookupsPerSec;
    uint16_t ResponseSizeBits;
    uint8_t PrioritySizeBits;
    uint8_t NumMasks;
    XilVitisNetP4Endian Endian;
    XilVitisNetP4CamMemType MemType;
    uint32_t RamSizeKbytes;
    XilVitisNetP4CamOptimizationType OptimizationType;
}  XilVitisNetP4CamConfig;


typedef struct {
    const char *NameStringPtr;
    uint32_t Value;
} XilVitisNetP4Attribute;

/*----------------------------------------------------------------------------*/
// vitisnetp4_table.h
typedef enum {
    XIL_VITIS_NET_P4_TABLE_MODE_DCAM,
    XIL_VITIS_NET_P4_TABLE_MODE_BCAM,
    XIL_VITIS_NET_P4_TABLE_MODE_TINY_BCAM,
    XIL_VITIS_NET_P4_TABLE_MODE_STCAM,
    XIL_VITIS_NET_P4_TABLE_MODE_TCAM,
    XIL_VITIS_NET_P4_TABLE_MODE_TINY_TCAM,
} XilVitisNetP4TableMode;

typedef struct {
} XilVitisNetP4TableCtx;

typedef struct {
    const char *NameStringPtr;
    uint32_t ParamListSize;
    XilVitisNetP4Attribute *ParamListPtr;
} XilVitisNetP4Action;

typedef struct {
    XilVitisNetP4Endian Endian;
    XilVitisNetP4TableMode Mode;
    uint32_t KeySizeBits;
    XilVitisNetP4CamConfig CamConfig;
    uint32_t ActionIdWidthBits;
    uint32_t ActionListSize;
    XilVitisNetP4Action **ActionListPtr;
} XilVitisNetP4TableConfig;

/*----------------------------------------------------------------------------*/
// counter_extern.h
typedef enum {
    XIL_VITIS_NET_P4_COUNTER_PACKETS,
    XIL_VITIS_NET_P4_COUNTER_BYTES,
    XIL_VITIS_NET_P4_COUNTER_PACKETS_AND_BYTES,
    XIL_VITIS_NET_P4_COUNTER_FLAG,
} XilVitisNetP4CounterType;

typedef struct {
} XilVitisNetP4CounterCtx;

typedef struct {
    XilVitisNetP4CounterType CounterType;
    uint32_t NumCounters;
    uint32_t Width;
} XilVitisNetP4CounterConfig;

/*----------------------------------------------------------------------------*/
// register_top.h
typedef struct {
} XilVitisNetP4RegisterTopCtx;

typedef struct {
    uint32_t largest_index;
    uint16_t data_size;
    uint32_t InitialData[0];
    bool dram;
} XilVitisNetP4RegisterTopConfig;

/*----------------------------------------------------------------------------*/
// vitisnetp4_target.h
typedef struct {
} XilVitisNetP4TargetCtx;

typedef struct {
    const char *NameStringPtr;
    XilVitisNetP4TableConfig Config;
} XilVitisNetP4TargetTableConfig;


typedef struct {
    const char *NameStringPtr;
    XilVitisNetP4CounterConfig Config;
} XilVitisNetP4TargetCounterConfig;

typedef struct {
    const char *NameStringPtr;
    XilVitisNetP4RegisterTopConfig Config;
} XilVitisNetP4TargetRegisterConfig;

typedef struct {
    XilVitisNetP4Endian Endian;
    uint32_t TableListSize;
    XilVitisNetP4TargetTableConfig **TableListPtr;
    uint32_t CounterListSize;
    XilVitisNetP4TargetCounterConfig **CounterListPtr;
    uint32_t RegisterListSize;
    XilVitisNetP4TargetRegisterConfig **RegisterListPtr;
} XilVitisNetP4TargetConfig;
#else /* !WITH_VITISNETP4_STUBS ==> building with actual vitisnetp4 driver */
#include "vitisnetp4_common.h"
#include "vitisnetp4_table.h"
#include "vitisnetp4_target.h"
#endif

/*----------------------------------------------------------------------------*/
struct vitis_net_p4_drv_metadata_counter_block {
    const char* name;
    const char* const* aliases;
    size_t num_aliases;
};

/*----------------------------------------------------------------------------*/
struct vitis_net_p4_drv_metadata_register_block {
    const char* name;
    const char* const* aliases;
    size_t num_aliases;
};

/*----------------------------------------------------------------------------*/
struct vitis_net_p4_drv_metadata {
    const struct vitis_net_p4_drv_metadata_counter_block* const* counter_blocks;
    size_t num_counter_blocks;

    const struct vitis_net_p4_drv_metadata_register_block* const* register_blocks;
    size_t num_register_blocks;
};

/*----------------------------------------------------------------------------*/
struct vitis_net_p4_drv_intf {
    struct {
        const char* name;
        uintptr_t offset;
        const struct vitis_net_p4_drv_metadata* metadata;
    } info;

    struct {
        XilVitisNetP4ReturnType (*stub_env_if)(XilVitisNetP4EnvIf *EnvIfPtr);
    } common;

    struct {
        XilVitisNetP4ReturnType (*init)(
            XilVitisNetP4CounterCtx *CtxPtr, XilVitisNetP4EnvIf *EnvIfPtr,
            XilVitisNetP4CounterConfig *ConfigPtr);

        XilVitisNetP4ReturnType (*exit)(XilVitisNetP4CounterCtx *CtxPtr);

        XilVitisNetP4ReturnType (*reset)(XilVitisNetP4CounterCtx *CtxPtr);

        XilVitisNetP4ReturnType (*simple_read)(
            XilVitisNetP4CounterCtx *CtxPtr, uint32_t Index, uint64_t *ValuePtr);

        XilVitisNetP4ReturnType (*simple_write)(
            XilVitisNetP4CounterCtx *CtxPtr, uint32_t Index, uint64_t Value);

        XilVitisNetP4ReturnType (*combo_read)(
            XilVitisNetP4CounterCtx *CtxPtr, uint32_t Index,
            uint64_t *PacketCountPtr, uint64_t *ByteCountPtr);

        XilVitisNetP4ReturnType (*combo_write)(
            XilVitisNetP4CounterCtx *CtxPtr, uint32_t Index,
            uint64_t PacketCount, uint64_t ByteCount);

        XilVitisNetP4ReturnType (*collect_simple_read)(
            XilVitisNetP4CounterCtx *CtxPtr,  uint32_t Index,
            uint32_t NumCounters, uint64_t *ValuePtr);

        XilVitisNetP4ReturnType (*collect_combo_read)(
            XilVitisNetP4CounterCtx *CtxPtr,  uint32_t Index,
            uint32_t NumCounters, uint64_t *Packets, uint64_t *Bytes);
    } counter;

    struct {
        XilVitisNetP4ReturnType (*init)(
            XilVitisNetP4RegisterTopCtx *CtxPtr, XilVitisNetP4EnvIf *EnvIfPtr,
            XilVitisNetP4RegisterTopConfig *ConfigPtr);

        XilVitisNetP4ReturnType (*exit)(XilVitisNetP4RegisterTopCtx *CtxPtr);

        XilVitisNetP4ReturnType (*reset)(XilVitisNetP4RegisterTopCtx *CtxPtr);

        XilVitisNetP4ReturnType (*read)(
            XilVitisNetP4RegisterTopCtx *CtxPtr, uint32_t Index, uint8_t *Data_ptr);

        XilVitisNetP4ReturnType (*write)(
            XilVitisNetP4RegisterTopCtx *CtxPtr, uint32_t Index, uint8_t *Data_ptr);
    } registers;

    struct {
        XilVitisNetP4ReturnType (*reset)(XilVitisNetP4TableCtx *CtxPtr);

        XilVitisNetP4ReturnType (*update)(
            XilVitisNetP4TableCtx *CtxPtr, uint8_t *KeyPtr, uint8_t *MaskPtr,
            uint32_t ActionId, uint8_t *ActionParamsPtr);

        XilVitisNetP4ReturnType (*insert)(
            XilVitisNetP4TableCtx *CtxPtr, uint8_t *KeyPtr, uint8_t *MaskPtr,
            uint32_t Priority, uint32_t ActionId, uint8_t *ActionParamsPtr);

        XilVitisNetP4ReturnType (*delete)(
            XilVitisNetP4TableCtx *CtxPtr, uint8_t *KeyPtr, uint8_t *MaskPtr);

        XilVitisNetP4ReturnType (*get_mode)(
            XilVitisNetP4TableCtx *CtxPtr, XilVitisNetP4TableMode *ModePtr);

        XilVitisNetP4ReturnType (*get_action_id)(
            XilVitisNetP4TableCtx *CtxPtr, char *ActionNamePtr, uint32_t *ActionIdPtr);

        XilVitisNetP4ReturnType (*get_action_name)(
            XilVitisNetP4TableCtx *CtxPtr,
            uint32_t ActionId,
            char *ActionNamePtr,
            uint32_t ActionNameNumBytes);

        XilVitisNetP4ReturnType (*get_num_actions)(
            XilVitisNetP4TableCtx *CtxPtr, uint32_t *NumActionsPtr);

        XilVitisNetP4ReturnType (*get_key_size_bits)(
            XilVitisNetP4TableCtx *CtxPtr, uint32_t *KeySizeBitsPtr);

        XilVitisNetP4ReturnType (*get_action_id_size_bits)(
            XilVitisNetP4TableCtx *CtxPtr, uint32_t *ActionIdWidthBitsPtr);

        XilVitisNetP4ReturnType (*get_action_params_size_bits)(
            XilVitisNetP4TableCtx *CtxPtr, uint32_t *ActionParamsSizeBitsPtr);

        XilVitisNetP4ReturnType (*get_by_key)(
            XilVitisNetP4TableCtx *CtxPtr,
            uint8_t *KeyPtr,
            uint8_t *MaskPtr,
            uint32_t *PriorityPtr,
            uint32_t *ActionIdPtr,
            uint8_t *ActionParamsPtr);

        XilVitisNetP4ReturnType (*get_by_response)(
            XilVitisNetP4TableCtx *CtxPtr,
            uint32_t ActionId,
            uint8_t *ActionParamsPtr,
            uint8_t *ActionParamsMaskPtr,
            uint32_t *PositionPtr,
            uint8_t *KeyPtr,
            uint8_t *MaskPtr);

        XilVitisNetP4ReturnType (*get_ecc_counters)(
            XilVitisNetP4TableCtx *CtxPtr,
            uint32_t *CorrectedSingleBitErrorsPtr,
            uint32_t *DetectedDoubleBitErrorsPtr);
    } table;

    struct {
        XilVitisNetP4TargetConfig *config;

        XilVitisNetP4ReturnType (*init)(
            XilVitisNetP4TargetCtx *CtxPtr, XilVitisNetP4EnvIf *EnvIfPtr,
             XilVitisNetP4TargetConfig *ConfigPtr);

        XilVitisNetP4ReturnType (*exit)(XilVitisNetP4TargetCtx *CtxPtr);

        XilVitisNetP4ReturnType (*get_table_by_index)(
            XilVitisNetP4TargetCtx *CtxPtr, uint32_t Index,
            XilVitisNetP4TableCtx **TableCtxPtrPtr);

        XilVitisNetP4ReturnType (*get_table_by_name)(
            XilVitisNetP4TargetCtx *CtxPtr, char *TableNamePtr,
            XilVitisNetP4TableCtx **TableCtxPtrPtr);

        XilVitisNetP4ReturnType (*get_table_count)(
            XilVitisNetP4TargetCtx *CtxPtr, uint32_t *NumTablesPtr);
    } target;
};

/*----------------------------------------------------------------------------*/
size_t vitis_net_p4_drv_intf_count(void);
const struct vitis_net_p4_drv_intf* vitis_net_p4_drv_intf_get(unsigned int idx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VITISNETP4DRV_INTF_H */
