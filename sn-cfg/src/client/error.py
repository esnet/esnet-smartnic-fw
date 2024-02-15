#---------------------------------------------------------------------------------------------------
__all__ = (
    'error_code_str'
)

from .sn_cfg_v1_pb2 import ErrorCode

#---------------------------------------------------------------------------------------------------
ERROR_MAP = {
    # General error codes.
    ErrorCode.EC_OK: 'ok',

    # Batch error codes.
    ErrorCode.EC_UNKNOWN_BATCH_REQUEST: 'unknown-batch-request',
    ErrorCode.EC_UNKNOWN_BATCH_OP: 'unknown-batch-operation',
}

def error_code_str(ec):
    return ERROR_MAP.get(ec, 'unknown')
