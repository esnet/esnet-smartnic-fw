#ifndef VITISNETP4DRV_INTF_H
#define VITISNETP4DRV_INTF_H

#include "vitisnetp4_common.h"
#include "vitisnetp4_table.h"
#include "vitisnetp4_target.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*/
struct vitis_net_p4_drv_intf {
    struct {
        const char* name;
    } info;

    struct {
        XilVitisNetP4ReturnType (*stub_env_if)(XilVitisNetP4EnvIf *EnvIfPtr);
    } common;

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

        XilVitisNetP4ReturnType (*get_action_id)(
            XilVitisNetP4TableCtx *CtxPtr, char *ActionNamePtr,
            uint32_t *ActionIdPtr);

        XilVitisNetP4ReturnType (*get_mode)(
            XilVitisNetP4TableCtx *CtxPtr, XilVitisNetP4TableMode *ModePtr);
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
