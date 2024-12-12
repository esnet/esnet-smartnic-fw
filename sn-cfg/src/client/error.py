#---------------------------------------------------------------------------------------------------
__all__ = (
    'error_code_str'
)

from sn_cfg_proto import ErrorCode

#---------------------------------------------------------------------------------------------------
ERROR_MAP = {
    # General error codes.
    ErrorCode.EC_OK: 'ok',

    # Batch error codes.
    ErrorCode.EC_UNKNOWN_BATCH_REQUEST: 'unknown-batch-request',
    ErrorCode.EC_UNKNOWN_BATCH_OP: 'unknown-batch-operation',

    # Device configuration error codes.
    ErrorCode.EC_INVALID_DEVICE_ID: 'invalid-device-id',
    ErrorCode.EC_CARD_INFO_READ_FAILED: 'card-info-read-failed',

    # Host configuration error codes.
    ErrorCode.EC_INVALID_HOST_ID: 'invalid-host-id',
    ErrorCode.EC_MISSING_HOST_CONFIG: 'missing-host-config',
    ErrorCode.EC_MISSING_HOST_DMA_CONFIG: 'missing-host-dma-config',
    ErrorCode.EC_UNSUPPORTED_HOST_ID: 'unsupported-host-id',
    ErrorCode.EC_UNSUPPORTED_HOST_FUNCTION: 'unsupported-host-function',
    ErrorCode.EC_FAILED_GET_HOST_FUNCTION_DMA_QUEUES: 'failed-get-host-function-dma-queues',
    ErrorCode.EC_FAILED_SET_HOST_FUNCTION_DMA_QUEUES: 'failed-set-host-function-dma-queues',

    # Port configuration error codes.
    ErrorCode.EC_INVALID_PORT_ID: 'invalid-port-id',
    ErrorCode.EC_MISSING_PORT_CONFIG: 'missing-port-config',
    ErrorCode.EC_UNSUPPORTED_PORT_ID: 'unsupported-port-id',
    ErrorCode.EC_UNSUPPORTED_PORT_STATE: 'unsupported-port-state',
    ErrorCode.EC_UNSUPPORTED_PORT_FEC: 'unsupported-port-fec',
    ErrorCode.EC_UNSUPPORTED_PORT_LOOPBACK: 'unsupported-port-loopback',

    # Switch configuration error codes.
    ErrorCode.EC_MISSING_SWITCH_CONFIG: 'missing-switch-config',
    ErrorCode.EC_MISSING_IGR_SRC_FROM_INTF: 'missing-ingress-source-from-interface',
    ErrorCode.EC_MISSING_IGR_SRC_TO_INTF: 'missing-ingress-source-to-interface',
    ErrorCode.EC_UNSUPPORTED_IGR_SRC_FROM_INTF: 'unsupported-ingress-source-from-interface',
    ErrorCode.EC_UNSUPPORTED_IGR_SRC_TO_INTF: 'unsupported-ingress-source-to-interface',
    ErrorCode.EC_FAILED_GET_IGR_SRC: 'failed-get-ingress-source',
    ErrorCode.EC_FAILED_SET_IGR_SRC: 'failed-set-ingress-source',
    ErrorCode.EC_MISSING_IGR_CONN_FROM_INTF: 'missing-ingress-connection-from-interface',
    ErrorCode.EC_MISSING_IGR_CONN_TO_PROC: 'missing-ingress-connection-to-processor',
    ErrorCode.EC_UNSUPPORTED_IGR_CONN_FROM_INTF: 'unsupported-ingress-connection-from-interface',
    ErrorCode.EC_UNSUPPORTED_IGR_CONN_TO_PROC: 'unsupported-ingress-connection-to-processor',
    ErrorCode.EC_FAILED_GET_IGR_CONN: 'failed-get-ingress-connection',
    ErrorCode.EC_FAILED_SET_IGR_CONN: 'failed-set-ingress-connection',
    ErrorCode.EC_MISSING_EGR_CONN_ON_PROC: 'missing-egress-connection-on-processor',
    ErrorCode.EC_MISSING_EGR_CONN_FROM_INTF: 'missing-egress-connection-from-interface',
    ErrorCode.EC_MISSING_EGR_CONN_TO_INTF: 'missing-egress-connection-to-interface',
    ErrorCode.EC_UNSUPPORTED_EGR_CONN_ON_PROC: 'unsupported-egress-connection-on-processor',
    ErrorCode.EC_UNSUPPORTED_EGR_CONN_FROM_INTF: 'unsupported-egress-connection-from-interface',
    ErrorCode.EC_UNSUPPORTED_EGR_CONN_TO_INTF: 'unsupported-egress-connection-to-interface',
    ErrorCode.EC_FAILED_GET_EGR_CONN: 'failed-get-egress-connection',
    ErrorCode.EC_FAILED_SET_EGR_CONN: 'failed-set-egress-connection',

    # Defaults configuration error codes.
    ErrorCode.EC_UNKNOWN_DEFAULTS_PROFILE: 'unknown-defaults-profile',

    # Module configuration error codes.
    ErrorCode.EC_INVALID_MODULE_ID: 'invalid-module-id',
    ErrorCode.EC_MODULE_PAGE_READ_FAILED: 'module-page-read-failed',
    ErrorCode.EC_INVALID_MODULE_MEM_OFFSET: 'invalid-module-mem-offset',
    ErrorCode.EC_INVALID_MODULE_MEM_PAGE: 'invalid-module-mem-page',
    ErrorCode.EC_INVALID_MODULE_MEM_COUNT: 'invalid-module-mem-count',
    ErrorCode.EC_MODULE_MEM_READ_FAILED: 'module-mem-read-failed',
    ErrorCode.EC_MODULE_MEM_WRITE_FAILED: 'module-mem-write-failed',
    ErrorCode.EC_UNKNOWN_MODULE_GPIO_TYPE: 'unknown-module-gpio-type',
    ErrorCode.EC_MODULE_GPIO_READ_FAILED: 'module-gpio-read-failed',
    ErrorCode.EC_MODULE_GPIO_WRITE_FAILED: 'module-gpio-write-failed',
    ErrorCode.EC_MODULE_NOT_PRESENT: 'module-not-present',
}

def error_code_str(ec):
    return ERROR_MAP.get(ec, 'unknown')
