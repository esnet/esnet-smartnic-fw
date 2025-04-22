#ifndef VITISNETP4DRV_INTF_H
#define VITISNETP4DRV_INTF_H

#include "vitisnetp4_common.h"
#include "vitisnetp4_table.h"
#include "vitisnetp4_target.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*/
struct vitis_net_p4_drv_metadata_counter_block {
    const char* name;
    const char* const* aliases;
    size_t num_aliases;
};

/*----------------------------------------------------------------------------*/
struct vitis_net_p4_drv_metadata {
    const struct vitis_net_p4_drv_metadata_counter_block* const* counter_blocks;
    size_t num_counter_blocks;
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
