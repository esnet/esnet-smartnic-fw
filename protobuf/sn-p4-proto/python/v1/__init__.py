#---------------------------------------------------------------------------------------------------
from sn_p4_v1_pb2 import *
from sn_p4_v1_pb2_grpc import *

# For RPCs with an empty request message, call the RPC as client.rpc(Empty()).
from google.protobuf.empty_pb2 import Empty
