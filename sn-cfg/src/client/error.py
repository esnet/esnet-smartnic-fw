#---------------------------------------------------------------------------------------------------
__all__ = (
    'error_code_str'
)

from sn_cfg_proto import ErrorCode

#---------------------------------------------------------------------------------------------------
def error_code_str(ec):
    try:
        ec_str = ErrorCode.Name(ec)
    except AttributeError:
        ec_str = 'UNKNOWN_ERROR_CODE'
    else:
        ec_str = ec_str[3:] # Strip the 'EC_' prefix.
    return ec_str
