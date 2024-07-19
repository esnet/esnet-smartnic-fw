#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import datetime
import grpc
import time

from sn_p4_proto.v2 import (
    BatchOperation,
    BatchRequest,
    ServerConfig,
    ServerConfigRequest,
    ServerDebug,
    ServerDebugFlag,
    ServerStatusRequest,
    ErrorCode,
)

from .error import error_code_str
from .utils import apply_options

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
DEBUG_FLAG_MAP = {
    ServerDebugFlag.DEBUG_FLAG_TABLE_CLEAR: 'table-clear',
    ServerDebugFlag.DEBUG_FLAG_TABLE_RULE_INSERT: 'table-rule-insert',
    ServerDebugFlag.DEBUG_FLAG_TABLE_RULE_DELETE: 'table-rule-delete',
}
DEBUG_FLAG_RMAP = dict((name, enum) for enum, name in DEBUG_FLAG_MAP.items())

#---------------------------------------------------------------------------------------------------
def server_config_req(**kargs):
    config_kargs = {}

    enables = set()
    disables = set()
    for key, flags in (('debug_enables', enables), ('debug_disables', disables)):
        for flag in kargs.get(key, ()):
            if flag == 'all':
                flags.update(DEBUG_FLAG_MAP)
                break
            flags.add(DEBUG_FLAG_RMAP[flag])

    if enables or disables:
        config_kargs['debug'] = ServerDebug(enables=enables, disables=disables)

    return ServerConfigRequest(config=ServerConfig(**config_kargs))

#---------------------------------------------------------------------------------------------------
def rpc_server_config(op, **kargs):
    req = server_config_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_server_config(stub, **kargs):
    for resp in rpc_server_config(stub.GetServerConfig, **kargs):
        yield resp.config

def rpc_set_server_config(stub, **kargs):
    for resp in rpc_server_config(stub.SetServerConfig, **kargs):
        yield

#---------------------------------------------------------------------------------------------------
def configure_server(client, **kargs):
    for _ in rpc_set_server_config(client.stub, **kargs):
        click.echo(f'Configured server.')

#---------------------------------------------------------------------------------------------------
def _show_server_config(config):
    rows = []
    rows.append(HEADER_SEP)

    enabled = ', '.join(sorted(DEBUG_FLAG_MAP[flag] for flag in config.debug.enables))
    disabled = ', '.join(sorted(DEBUG_FLAG_MAP[flag] for flag in config.debug.disables))
    rows.append('Debug:')
    if enabled:
        rows.append(f'    Enabled:  {enabled}')
    if disabled:
        rows.append(f'    Disabled: {disabled}')

    click.echo('\n'.join(rows))

#---------------------------------------------------------------------------------------------------
def show_server_config(client, **kargs):
    for config in rpc_get_server_config(client.stub, **kargs):
        _show_server_config(config)

#---------------------------------------------------------------------------------------------------
def batch_generate_server_config_req(op, **kargs):
    yield BatchRequest(op=op, server_config=server_config_req(**kargs))

def batch_process_server_config_resp(resp):
    if not resp.HasField('server_config'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
        BatchOperation.BOP_SET: 'Configured',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {resp.op}')
    op_label = supported_ops[op]

    resp = resp.server_config
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_server_config(resp.config)
    else:
        click.echo(f'{op_label} server.')
    return True

def batch_server_config(op, **kargs):
    return batch_generate_server_config_req(op, **kargs), batch_process_server_config_resp

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

    def format_ts(ts):
        return f'{ts.seconds}s.{ts.nanos}ns'

    utc = time.strftime('%Y-%m-%d %H:%M:%S %z', time.gmtime(status.start_time.seconds))
    rows.append(f'Start Time: {utc} [{format_ts(status.start_time)}]')

    up = datetime.timedelta(seconds=status.up_time.seconds)
    rows.append(f'Up Time:    {up} [{format_ts(status.up_time)}]')

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

    supported_ops = (
        BatchOperation.BOP_GET,
    )
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {resp.op}')

    resp = resp.server_status
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_server_status(resp.status)
    return True

def batch_server_status(op, **kargs):
    return batch_generate_server_status_req(op, **kargs), batch_process_server_status_resp

#---------------------------------------------------------------------------------------------------
def configure_server_options(fn):
    debug_flag_choice = click.Choice(['all'] + sorted(name for name in DEBUG_FLAG_RMAP))
    options = (
        click.option(
            '--debug-enable', '-e',
            'debug_enables',
            type=debug_flag_choice,
            multiple=True,
            help='Enable debug flag.',
        ),
        click.option(
            '--debug-disable', '-d',
            'debug_disables',
            type=debug_flag_choice,
            multiple=True,
            help='Disable debug flag.',
        ),
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='configure-server')
    @configure_server_options
    def configure_server(**kargs):
        '''
        Configuration for the SmartNIC P4 server.
        '''
        return batch_server_config(BatchOperation.BOP_SET, **kargs)

    @cmd.command(name='show-server-config')
    def show_server_config(**kargs):
        '''
        Display the configuration of the SmartNIC P4 server.
        '''
        return batch_server_config(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-server-status')
    def show_server_status(**kargs):
        '''
        Display the current status of the SmartNIC P4 server.
        '''
        return batch_server_status(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_configure_commands(cmd):
    @cmd.command
    @configure_server_options
    @click.pass_context
    def server(ctx, **kargs):
        '''
        Configuration for the SmartNIC P4 server.
        '''
        configure_server(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def server(ctx):
        '''
        Display SmartNIC P4 server.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            show_server_config(client)
            show_server_status(client)

    @server.command
    @click.pass_context
    def config(ctx, **kargs):
        '''
        Display the configuration of the SmartNIC P4 server.
        '''
        show_server_config(ctx.obj, **kargs)

    @server.command
    @click.pass_context
    def status(ctx, **kargs):
        '''
        Display the current status of the SmartNIC P4 server.
        '''
        show_server_status(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_configure_commands(cmds.configure)
    add_show_commands(cmds.show)
