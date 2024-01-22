#---------------------------------------------------------------------------------------------------
__all__ = (
    'error_code_str'
)

from .sn_cfg_v1_pb2 import ErrorCode

#---------------------------------------------------------------------------------------------------
ERROR_MAP = {
    # General error codes.
    ErrorCode.EC_OK: 'ok',
}

def error_code_str(ec):
    return ERROR_MAP.get(ec, 'unknown')
