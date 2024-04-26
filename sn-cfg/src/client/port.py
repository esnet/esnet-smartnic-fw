#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import grpc

from sn_cfg_proto import (
    BatchOperation,
    BatchRequest,
    ErrorCode,
    PortConfig,
    PortConfigRequest,
    PortFec,
    PortLink,
    PortLoopback,
    PortState,
    PortStatsRequest,
    PortStatusRequest,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
STATE_MAP = {
    PortState.PORT_STATE_DISABLE: 'disable',
    PortState.PORT_STATE_ENABLE: 'enable',
}
STATE_RMAP = dict((name, enum) for enum, name in STATE_MAP.items())

FEC_MAP = {
    PortFec.PORT_FEC_NONE: 'none',
    PortFec.PORT_FEC_REED_SOLOMON: 'reed-solomon',
}
FEC_RMAP = dict((name, enum) for enum, name in FEC_MAP.items())

LOOPBACK_MAP = {
    PortLoopback.PORT_LOOPBACK_NONE: 'none',
    PortLoopback.PORT_LOOPBACK_NEAR_END_PMA: 'near-end-pma',
}
LOOPBACK_RMAP = dict((name, enum) for enum, name in LOOPBACK_MAP.items())

LINK_MAP = {
    PortLink.PORT_LINK_DOWN: 'down',
    PortLink.PORT_LINK_UP: 'up',
}
LINK_RMAP = dict((name, enum) for enum, name in LINK_MAP.items())

#---------------------------------------------------------------------------------------------------
def port_config_req(dev_id, port_id, **config_kargs):
    req_kargs = {'dev_id': dev_id, 'port_id': port_id}
    if config_kargs:
        for key, rmap in (('state', STATE_RMAP), ('fec', FEC_RMAP), ('loopback', LOOPBACK_RMAP)):
            value = config_kargs.get(key)
            if value is not None:
                config_kargs[key] = rmap[value]
        req_kargs['config'] = PortConfig(**config_kargs)
    return PortConfigRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_port_config(op, **kargs):
    req = port_config_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_port_config(stub, **kargs):
    for resp in rpc_port_config(stub.GetPortConfig, **kargs):
        yield resp.dev_id, resp.port_id, resp.config

def rpc_set_port_config(stub, **kargs):
    for resp in rpc_port_config(stub.SetPortConfig, **kargs):
        yield resp.dev_id, resp.port_id

#---------------------------------------------------------------------------------------------------
def configure_port(client, **kargs):
    for dev_id, port_id in rpc_set_port_config(client.stub, **kargs):
        click.echo(f'Configured port ID {port_id} on device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_port_config(dev_id, port_id, config):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Port ID: {port_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    rows.append(f'State:    {STATE_MAP[config.state]}')
    rows.append(f'FEC:      {FEC_MAP[config.fec]}')
    rows.append(f'Loopback: {LOOPBACK_MAP[config.loopback]}')

    click.echo('\n'.join(rows))

def show_port_config(client, **kargs):
    for dev_id, port_id, config in rpc_get_port_config(client.stub, **kargs):
        _show_port_config(dev_id, port_id, config)

#---------------------------------------------------------------------------------------------------
def batch_generate_port_config_req(op, **kargs):
    yield BatchRequest(op=op, port_config=port_config_req(**kargs))

def batch_process_port_config_resp(resp):
    if not resp.HasField('port_config'):
        return False

    resp = resp.port_config
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('config'):
        _show_port_config(resp.dev_id, resp.port_id, resp.config)
    else:
        click.echo(f'Configured port ID {resp.port_id} on device ID {resp.dev_id}.')
    return True

def batch_port_config(op, **kargs):
    return batch_generate_port_config_req(op, **kargs), batch_process_port_config_resp

#---------------------------------------------------------------------------------------------------
def port_stats_req(dev_id, port_id, **stats_kargs):
    req_kargs = {'dev_id': dev_id, 'port_id': port_id}
    return PortStatsRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_port_stats(op, **kargs):
    req = port_stats_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_clear_port_stats(stub, **kargs):
    for resp in rpc_port_stats(stub.ClearPortStats, **kargs):
        yield resp.dev_id, resp.port_id

def rpc_get_port_stats(stub, **kargs):
    for resp in rpc_port_stats(stub.GetPortStats, **kargs):
        yield resp.dev_id, resp.port_id, resp.stats

#---------------------------------------------------------------------------------------------------
def clear_port_stats(client, **kargs):
    for dev_id, port_id in rpc_clear_port_stats(client.stub, **kargs):
        click.echo(f'Cleared statistics for port ID {port_id} on device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_port_stats(dev_id, port_id, stats):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Port ID: {port_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    for cnt in stats.counters:
        if cnt.value != 0:
            rows.append(f'{cnt.name}: {cnt.value}')
    click.echo('\n'.join(rows))

def show_port_stats(client, **kargs):
    for dev_id, port_id, stats in rpc_get_port_stats(client.stub, **kargs):
        _show_port_stats(dev_id, port_id, stats)

#---------------------------------------------------------------------------------------------------
def batch_generate_port_stats_req(op, **kargs):
    yield BatchRequest(op=op, port_stats=port_stats_req(**kargs))

def batch_process_port_stats_resp(resp):
    if not resp.HasField('port_stats'):
        return False

    resp = resp.port_stats
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('stats'):
        _show_port_stats(resp.dev_id, resp.port_id, resp.stats)
    else:
        click.echo(f'Cleared statistics for port ID {resp.port_id} on device ID {resp.dev_id}.')
    return True

def batch_port_stats(op, **kargs):
    return batch_generate_port_stats_req(op, **kargs), batch_process_port_stats_resp

#---------------------------------------------------------------------------------------------------
def port_status_req(dev_id, port_id, **status_kargs):
    req_kargs = {'dev_id': dev_id, 'port_id': port_id}
    return PortStatusRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_port_status(op, **kargs):
    req = port_status_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_port_status(stub, **kargs):
    for resp in rpc_port_status(stub.GetPortStatus, **kargs):
        yield resp.dev_id, resp.port_id, resp.status

#---------------------------------------------------------------------------------------------------
def _show_port_status(dev_id, port_id, status):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Port ID: {port_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    rows.append(f'Link: {LINK_MAP[status.link]}')

    click.echo('\n'.join(rows))

def show_port_status(client, **kargs):
    for dev_id, port_id, status in rpc_get_port_status(client.stub, **kargs):
        _show_port_status(dev_id, port_id, status)

#---------------------------------------------------------------------------------------------------
def batch_generate_port_status_req(op, **kargs):
    yield BatchRequest(op=op, port_status=port_status_req(**kargs))

def batch_process_port_status_resp(resp):
    if not resp.HasField('port_status'):
        return False

    resp = resp.port_status
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('status'):
        _show_port_status(resp.dev_id, resp.port_id, resp.status)
    return True

def batch_port_status(op, **kargs):
    return batch_generate_port_status_req(op, **kargs), batch_process_port_status_resp

#---------------------------------------------------------------------------------------------------
port_id_option = click.option(
    '--port-id', '-p',
    type=click.INT,
    default=-1,
    help='''
    0-based index of the physical port to operate on. Leave unset or set to -1 for all ports.
    ''',
)

def clear_port_options(fn):
    options = (
        device_id_option,
        port_id_option,
    )
    return apply_options(options, fn)

def configure_port_options(fn):
    options = (
        device_id_option,
        port_id_option,
        click.option(
            '--state', '-s',
            type=click.Choice(sorted(name for name in STATE_RMAP)),
            help='Operational state to apply to the port.',
        ),
        click.option(
            '--fec', '-f',
            type=click.Choice(sorted(name for name in FEC_RMAP)),
            help='Type of forward error correction to apply to the port.',
        ),
        click.option(
            '--loopback', '-l',
            type=click.Choice(sorted(name for name in LOOPBACK_RMAP)),
            help='Type of loopback to apply to the port.',
        ),
    )
    return apply_options(options, fn)

def show_port_options(fn):
    options = (
        device_id_option,
        port_id_option,
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='clear-port-stats')
    @clear_port_options
    def clear_port_stats(**kargs):
        '''
        Clear the statistics of SmartNIC ports.
        '''
        return batch_port_stats(BatchOperation.BOP_CLEAR, **kargs)

    @cmd.command(name='configure-port')
    @configure_port_options
    def configure_port(**kargs):
        '''
        Configuration for SmartNIC ports.
        '''
        return batch_port_config(BatchOperation.BOP_SET, **kargs)

    @cmd.command(name='show-port-config')
    @show_port_options
    def show_port_config(**kargs):
        '''
        Display the configuration of SmartNIC ports.
        '''
        return batch_port_config(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-port-stats')
    @show_port_options
    def show_port_stats(**kargs):
        '''
        Display the statistics of SmartNIC ports.
        '''
        return batch_port_stats(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-port-status')
    @show_port_options
    def show_port_status(**kargs):
        '''
        Display the status of SmartNIC ports.
        '''
        return batch_port_status(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_clear_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def port(ctx):
        '''
        Clear for SmartNIC ports.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1, 'port_id': -1}
            clear_port_stats(client, **kargs)

    @port.command
    @clear_port_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Clear the statistics of SmartNIC ports.
        '''
        clear_port_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_configure_commands(cmd):
    @cmd.command
    @configure_port_options
    @click.pass_context
    def port(ctx, **kargs):
        '''
        Configuration for SmartNIC ports.
        '''
        configure_port(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def port(ctx):
        '''
        Display for SmartNIC ports.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1, 'port_id': -1}
            show_port_config(client, **kargs)
            show_port_status(ctx.obj, **kargs)

    @port.command
    @show_port_options
    @click.pass_context
    def config(ctx, **kargs):
        '''
        Display the configuration of SmartNIC ports.
        '''
        show_port_config(ctx.obj, **kargs)

    @port.command
    @show_port_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Display the statistics of SmartNIC ports.
        '''
        show_port_stats(ctx.obj, **kargs)

    @port.command
    @show_port_options
    @click.pass_context
    def status(ctx, **kargs):
        '''
        Display the status of SmartNIC ports.
        '''
        show_port_status(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_configure_commands(cmds.configure)
    add_show_commands(cmds.show)
