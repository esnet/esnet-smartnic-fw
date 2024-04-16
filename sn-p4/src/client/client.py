#---------------------------------------------------------------------------------------------------
__all__ = (
    'connect_client',
)

import grpc
import os

from sn_p4_proto import SmartnicP4Stub

#---------------------------------------------------------------------------------------------------
def connect_client(address=None, port=None):
    if address is None:
        address = os.environ.get('SN_P4_CLI_SERVER', 'localhost')
    if port is None:
        port = os.environ.get('SN_P4_CLI_PORT', 50051)

    channel = grpc.insecure_channel(f'{address}:{port}')
    return SmartnicP4Stub(channel)
