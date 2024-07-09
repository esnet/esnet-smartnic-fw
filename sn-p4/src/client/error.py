#---------------------------------------------------------------------------------------------------
__all__ = (
    'error_code_str'
)

from sn_p4_proto.v2 import ErrorCode

#---------------------------------------------------------------------------------------------------
ERROR_MAP = {
    # General error codes.
    ErrorCode.EC_OK: 'OK',

    # Batch error codes.
    ErrorCode.EC_UNKNOWN_BATCH_REQUEST: 'UNKNOWN_BATCH_REQUEST',
    ErrorCode.EC_UNKNOWN_BATCH_OP: 'UNKNOWN_BATCH_OP',

    # Device configuration error codes.
    ErrorCode.EC_INVALID_DEVICE_ID: 'INVALID_DEVICE_ID',
}

def error_code_str(ec):
    return ERROR_MAP.get(ec, 'UNKNOWN')
