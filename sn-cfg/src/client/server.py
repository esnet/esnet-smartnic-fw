#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import datetime
import grpc
import time

from sn_cfg_proto import (
    BatchOperation,
    BatchRequest,
    ServerStatusRequest,
    ErrorCode,
)

from .error import error_code_str
from .utils import apply_options, format_timestamp

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
def server_status_req(**kargs):
    return ServerStatusRequest()

#---------------------------------------------------------------------------------------------------
def rpc_server_status(op, **kargs):
    req = server_status_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_server_status(stub, **kargs):
    for resp in rpc_server_status(stub.GetServerStatus, **kargs):
        yield resp.status

#---------------------------------------------------------------------------------------------------
def _show_server_status(status):
    rows = []
    rows.append(HEADER_SEP)

    utc = time.strftime('%Y-%m-%d %H:%M:%S %z', time.gmtime(status.start_time.seconds))
    rows.append(f'Start Time: {utc} [{format_timestamp(status.start_time)}]')

    up = datetime.timedelta(seconds=status.up_time.seconds)
    rows.append(f'Up Time:    {up} [{format_timestamp(status.up_time)}]')

    click.echo('\n'.join(rows))

#---------------------------------------------------------------------------------------------------
def show_server_status(client, **kargs):
    for status in rpc_get_server_status(client.stub, **kargs):
        _show_server_status(status)

#---------------------------------------------------------------------------------------------------
def batch_generate_server_status_req(op, **kargs):
    yield BatchRequest(op=op, server_status=server_status_req(**kargs))

def batch_process_server_status_resp(resp):
    if not resp.HasField('server_status'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.server_status
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_server_status(resp.status)
    return True

def batch_server_status(op, **kargs):
    return batch_generate_server_status_req(op, **kargs), batch_process_server_status_resp

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='show-server-status')
    def show_server_status(**kargs):
        '''
        Display the current status of the SmartNIC Configuration server.
        '''
        return batch_server_status(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def server(ctx):
        '''
        Display SmartNIC Configuration server.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            show_server_status(client)

    @server.command
    @click.pass_context
    def status(ctx, **kargs):
        '''
        Display the current status of the SmartNIC Configuration server.
        '''
        show_server_status(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_show_commands(cmds.show)
