#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import grpc
import re
import types

from sn_cfg_proto import (
    BatchOperation,
    BatchRequest,
    ErrorCode,
    StatsFilters,
    SwitchBypassMode,
    SwitchConfig,
    SwitchConfigRequest,
    SwitchDestination,
    SwitchInterface,
    SwitchStatsRequest,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options, ChoiceFields, format_timestamp, natural_sort_key

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
INTF_MAP = {
    SwitchInterface.SW_INTF_PHYSICAL: 'physical',
    SwitchInterface.SW_INTF_TEST: 'test',
}
INTF_RMAP = dict((name, enum) for enum, name in INTF_MAP.items())
INTF_CHOICES = tuple(sorted(INTF_RMAP))

#---------------------------------------------------------------------------------------------------
DEST_MAP = {
    SwitchDestination.SW_DEST_BYPASS: 'bypass',
    SwitchDestination.SW_DEST_DROP: 'drop',
    SwitchDestination.SW_DEST_APP: 'app',
}
DEST_RMAP = dict((name, enum) for enum, name in DEST_MAP.items())
DEST_CHOICES = tuple(sorted(DEST_RMAP))

#---------------------------------------------------------------------------------------------------
BYPASS_MODE_MAP = {
    SwitchBypassMode.SW_BYPASS_STRAIGHT: 'straight',
    SwitchBypassMode.SW_BYPASS_SWAP: 'swap',
}
BYPASS_MODE_RMAP = dict((name, enum) for enum, name in BYPASS_MODE_MAP.items())
BYPASS_MODE_CHOICES = tuple(sorted(BYPASS_MODE_RMAP))

#---------------------------------------------------------------------------------------------------
INDEX_CHOICES = tuple(str(i) for i in range(2))
def index_choice_field_convert(value, param, ctx):
    return int(value)

#---------------------------------------------------------------------------------------------------
def switch_config_req(dev_id, **config_kargs):
    req_kargs = {'dev_id': dev_id}

    if config_kargs:
        config = SwitchConfig()
        req_kargs['config'] = config

        for index, intf, dest in config_kargs.get('ingress_selector', ()):
            sel = config.ingress_selectors.add()
            sel.index = index
            sel.intf = INTF_RMAP[intf]
            sel.dest = DEST_RMAP[dest]

        for index, intf in config_kargs.get('egress_selector', ()):
            sel = config.egress_selectors.add()
            sel.index = index
            sel.intf = INTF_RMAP[intf]

        bmode = config_kargs.get('bypass_mode')
        if bmode is not None:
            config.bypass_mode = BYPASS_MODE_RMAP[bmode]

    return SwitchConfigRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_switch_config(op, **kargs):
    req = switch_config_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_switch_config(stub, **kargs):
    for resp in rpc_switch_config(stub.GetSwitchConfig, **kargs):
        yield resp.dev_id, resp.config

def rpc_set_switch_config(stub, **kargs):
    for resp in rpc_switch_config(stub.SetSwitchConfig, **kargs):
        yield resp.dev_id

#---------------------------------------------------------------------------------------------------
def configure_switch(client, **kargs):
    for dev_id in rpc_set_switch_config(client.stub, **kargs):
        click.echo(f'Configured switch for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_switch_config(dev_id, config):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    rows.append('Ingress Selectors:')
    for sel in config.ingress_selectors:
        rows.append(f'    Port {sel.index}:')
        rows.append(f'        Interface:   {INTF_MAP[sel.intf]}')
        rows.append(f'        Destination: {DEST_MAP[sel.dest]}')

    rows.append('Egress Selectors:')
    for sel in config.egress_selectors:
        rows.append(f'    Port {sel.index}:')
        rows.append(f'        Interface: {INTF_MAP[sel.intf]}')

    rows.append(f'Bypass Mode: {BYPASS_MODE_MAP[config.bypass_mode]}')

    click.echo('\n'.join(rows))

def show_switch_config(client, **kargs):
    for dev_id, config in rpc_get_switch_config(client.stub, **kargs):
        _show_switch_config(dev_id, config)

#---------------------------------------------------------------------------------------------------
def batch_generate_switch_config_req(op, **kargs):
    yield BatchRequest(op=op, switch_config=switch_config_req(**kargs))

def batch_process_switch_config_resp(resp):
    if not resp.HasField('switch_config'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
        BatchOperation.BOP_SET: 'Configured',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.switch_config
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_switch_config(resp.dev_id, resp.config)
    else:
        click.echo(f'{op_label} switch for device ID {resp.dev_id}.')
    return True

def batch_switch_config(op, **kargs):
    return batch_generate_switch_config_req(op, **kargs), batch_process_switch_config_resp

#---------------------------------------------------------------------------------------------------
def switch_stats_req(dev_id, **stats_kargs):
    req_kargs = {'dev_id': dev_id}
    if stats_kargs:
        filters_kargs = {}
        if not stats_kargs.get('zeroes'):
            filters_kargs['non_zero'] = True

        if filters_kargs:
            req_kargs['filters'] = StatsFilters(**filters_kargs)

    return SwitchStatsRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_switch_stats(op, **kargs):
    req = switch_stats_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_clear_switch_stats(stub, **kargs):
    for resp in rpc_switch_stats(stub.ClearSwitchStats, **kargs):
        yield resp.dev_id

def rpc_get_switch_stats(stub, **kargs):
    for resp in rpc_switch_stats(stub.GetSwitchStats, **kargs):
        yield resp.dev_id, resp.stats

#---------------------------------------------------------------------------------------------------
def clear_switch_stats(client, **kargs):
    for dev_id in rpc_clear_switch_stats(client.stub, **kargs):
        click.echo(f'Cleared statistics for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_switch_stats(dev_id, stats, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    name_len = 0
    value_len = 0
    metrics = {}
    for metric in stats.metrics:
        is_array = metric.num_elements > 0
        last_update = format_timestamp(metric.last_update)
        for value in metric.values:
            name = metric.name
            if is_array:
                name += f'[{value.index}]'
            name_len = max(name_len, len(name))

            svalue = f'{value.u64}'
            value_len = max(value_len, len(svalue))

            metrics[name] = {
                'value': svalue,
                'last_update': last_update,
            }

    if metrics:
        last_update = kargs.get('last_update', False)
        for name in sorted(metrics, key=natural_sort_key):
            m = metrics[name]
            row = f'{name:>{name_len}}: {m["value"]:<{value_len}}'
            if last_update:
                row += f'    [{m["last_update"]}]'
            rows.append(row)

    click.echo('\n'.join(rows))

def show_switch_stats(client, **kargs):
    for dev_id, stats in rpc_get_switch_stats(client.stub, **kargs):
        _show_switch_stats(dev_id, stats, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_switch_stats_req(op, **kargs):
    yield BatchRequest(op=op, switch_stats=switch_stats_req(**kargs))

def batch_process_switch_stats_resp(kargs):
    def process(resp):
        if not resp.HasField('switch_stats'):
            return False

        supported_ops = {
            BatchOperation.BOP_GET: 'Got',
            BatchOperation.BOP_CLEAR: 'Cleared',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.switch_stats
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_switch_stats(resp.dev_id, resp.stats, kargs)
        else:
            click.echo(f'{op_label} statistics for device ID {resp.dev_id}.')
        return True

    return process

def batch_switch_stats(op, **kargs):
    return batch_generate_switch_stats_req(op, **kargs), batch_process_switch_stats_resp(kargs)

#---------------------------------------------------------------------------------------------------
def clear_switch_stats_options(fn):
    options = (
        device_id_option,
    )
    return apply_options(options, fn)

def configure_switch_options(fn):
    options = (
        device_id_option,
        click.option(
            '--ingress-selector', '-i',
            type=ChoiceFields(
                ':',
                ('index', INDEX_CHOICES, index_choice_field_convert),
                ('intf', INTF_CHOICES, None),
                ('dest', DEST_CHOICES, None),
            ),
            multiple=True,
            help='''
            Selects where packets are received from and how they are processed. A selector is
            specified as a triplet of the form <index>:<intf>:<dest>, where <index> is a 0-based
            port index on the switch, <intf> indicates whether packets are to be received from the
            physical interface or the test interface (from host via QDMA), and <dest> is how the
            packets are to be processed.
            ''',
        ),
        click.option(
            '--egress-selector', '-e',
            type=ChoiceFields(
                ':',
                ('index', INDEX_CHOICES, index_choice_field_convert),
                ('intf', INTF_CHOICES, None),
            ),
            multiple=True,
            help='''
            Selects how processed packets are to be transmitted. A selector is specified as a pair
            of the form <index>:<intf>, where <index> is a 0-based port index on the switch and
            <intf> indicates whether packets are to be transmitted by the physical interface or the
            test interface (to host via QDMA).
            ''',
        ),
        click.option(
            '--bypass-mode', '-b',
            type=click.Choice(BYPASS_MODE_CHOICES),
            help='''
            When at least one of the ingress selectors has a destination of "bypass", this option
            determines how the ingress ports map to the egress ports.  The "straight" mode is a
            one-to-one mapping of the port (such that ingress-port0 --> egress-port0, ingress-port1
            --> egress-port1), whereas the "swap" mode reverses the mapping (such that ingress-port0
            --> egress-port1, ingress-port1 --> egress-port0).
            ''',
        ),
    )
    return apply_options(options, fn)

def show_switch_options(fn):
    options = (
        device_id_option,
    )
    return apply_options(options, fn)

def show_switch_stats_options(fn):
    options = (
        device_id_option,
        click.option(
            '--zeroes',
            is_flag=True,
            help='Include zero valued counters in the display.',
        ),
        click.option(
            '--last-update',
            is_flag=True,
            help='Include the counter last update timestamp in the display.',
        ),
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='clear-switch-stats')
    @clear_switch_stats_options
    def clear_switch_stats(**kargs):
        '''
        Clear the statistics of the SmartNIC packet switch.
        '''
        return batch_switch_stats(BatchOperation.BOP_CLEAR, **kargs)

    @cmd.command(name='configure-switch')
    @configure_switch_options
    def configure_switch(**kargs):
        '''
        Configuration for the SmartNIC packet switch.
        '''
        return batch_switch_config(BatchOperation.BOP_SET, **kargs)

    @cmd.command(name='show-switch-config')
    @show_switch_options
    def show_switch_config(**kargs):
        '''
        Display the configuration of the SmartNIC packet switch.
        '''
        return batch_switch_config(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-switch-stats')
    @show_switch_stats_options
    def show_switch_stats(**kargs):
        '''
        Display the statistics of the SmartNIC packet switch.
        '''
        return batch_switch_stats(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_clear_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def switch(ctx):
        '''
        Clear for the SmartNIC packet switch.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1}
            clear_switch_stats(client, **kargs)

    @switch.command
    @clear_switch_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Clear the statistics of the SmartNIC packet switch.
        '''
        clear_switch_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_configure_commands(cmd):
    @cmd.command
    @configure_switch_options
    @click.pass_context
    def switch(ctx, **kargs):
        '''
        Configuration for the SmartNIC packet switch.
        '''
        configure_switch(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def switch(ctx):
        '''
        Display for the SmartNIC packet switch.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1}
            show_switch_config(client, **kargs)

    @switch.command
    @show_switch_options
    @click.pass_context
    def config(ctx, **kargs):
        '''
        Display the configuration of the SmartNIC packet switch.
        '''
        show_switch_config(ctx.obj, **kargs)

    @switch.command
    @show_switch_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Display the statistics of the SmartNIC packet switch.
        '''
        show_switch_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_configure_commands(cmds.configure)
    add_show_commands(cmds.show)
