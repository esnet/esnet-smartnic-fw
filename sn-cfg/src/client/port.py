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
    StatsFilters,
)

from .device import device_id_option
from .error import error_code_str
from .stats import stats_req_kargs, stats_show_base_options, stats_show_format
from .utils import apply_options, FLOW_CONTROL_THRESHOLD, format_timestamp, natural_sort_key

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
        config = PortConfig()
        req_kargs['config'] = config

        for key, rmap in (('state', STATE_RMAP), ('fec', FEC_RMAP), ('loopback', LOOPBACK_RMAP)):
            value = config_kargs.get(key)
            if value is not None:
                setattr(config, key, rmap[value])

        threshold = config_kargs.get('fc_egress_threshold')
        if threshold is not None:
            config.flow_control.egress_threshold = threshold

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

    rows.append(f'State:                {STATE_MAP[config.state]}')
    rows.append(f'FEC:                  {FEC_MAP[config.fec]}')
    rows.append(f'Loopback:             {LOOPBACK_MAP[config.loopback]}')

    fc = config.flow_control
    rows.append('Flow Control:')
    threshold = 'unlimited' if fc.egress_threshold < 0 else fc.egress_threshold
    rows.append(f'    Egress Threshold: {threshold}')

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

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
        BatchOperation.BOP_SET: 'Configured',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.port_config
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_port_config(resp.dev_id, resp.port_id, resp.config)
    else:
        click.echo(f'{op_label} port ID {resp.port_id} on device ID {resp.dev_id}.')
    return True

def batch_port_config(op, **kargs):
    return batch_generate_port_config_req(op, **kargs), batch_process_port_config_resp

#---------------------------------------------------------------------------------------------------
def port_stats_req(dev_id, port_id, **stats_kargs):
    req_kargs = stats_req_kargs(dev_id, stats_kargs)
    req_kargs['port_id'] = port_id
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
def _show_port_stats(dev_id, port_id, stats, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Port ID: {port_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)
    rows.extend(stats_show_format(stats, kargs))
    click.echo('\n'.join(rows))

def show_port_stats(client, **kargs):
    for dev_id, port_id, stats in rpc_get_port_stats(client.stub, **kargs):
        _show_port_stats(dev_id, port_id, stats, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_port_stats_req(op, **kargs):
    yield BatchRequest(op=op, port_stats=port_stats_req(**kargs))

def batch_process_port_stats_resp(kargs):
    def process(resp):
        if not resp.HasField('port_stats'):
            return False

        supported_ops = {
            BatchOperation.BOP_GET: 'Got',
            BatchOperation.BOP_CLEAR: 'Cleared',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.port_stats
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_port_stats(resp.dev_id, resp.port_id, resp.stats, kargs)
        else:
            click.echo(f'{op_label} statistics for port ID {resp.port_id} '
                       f'on device ID {resp.dev_id}.')
        return True

    return process

def batch_port_stats(op, **kargs):
    return batch_generate_port_stats_req(op, **kargs), batch_process_port_stats_resp(kargs)

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

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.port_status
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
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

def clear_port_stats_options(fn):
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
        click.option(
            '--fc-egress-threshold', '-e',
            type=FLOW_CONTROL_THRESHOLD,
            help='''
            FIFO fill level threshold at which to assert egress flow control. Specified as an
            integer in units of 64 byte words or "unlimited" to disable flow control.
            ''',
        ),
    )
    return apply_options(options, fn)

def show_port_options(fn):
    options = (
        device_id_option,
        port_id_option,
    )
    return apply_options(options, fn)

def show_port_stats_options(fn):
    options = (
        device_id_option,
        port_id_option,
    ) + stats_show_base_options()
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='clear-port-stats')
    @clear_port_stats_options
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
    @show_port_stats_options
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
    @clear_port_stats_options
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
            show_port_status(client, **kargs)

    @port.command
    @show_port_options
    @click.pass_context
    def config(ctx, **kargs):
        '''
        Display the configuration of SmartNIC ports.
        '''
        show_port_config(ctx.obj, **kargs)

    @port.command
    @show_port_stats_options
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
