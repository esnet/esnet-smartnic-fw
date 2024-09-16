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
    HostConfig,
    HostConfigRequest,
    HostDmaConfig,
    HostStatsRequest,
    StatsFilters,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options, format_timestamp, natural_sort_key

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
def host_config_req(dev_id, host_id, **config_kargs):
    req_kargs = {'dev_id': dev_id, 'host_id': host_id}
    if config_kargs:
        prefix = 'dma_'
        dma_kargs = dict(
            (k[len(prefix):], v)
            for k, v in config_kargs.items()
            if k.startswith(prefix)
        )
        dma = HostDmaConfig(**dma_kargs)
        req_kargs['config'] = HostConfig(dma=dma)
    return HostConfigRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_host_config(op, **kargs):
    req = host_config_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_host_config(stub, **kargs):
    for resp in rpc_host_config(stub.GetHostConfig, **kargs):
        yield resp.dev_id, resp.host_id, resp.config

def rpc_set_host_config(stub, **kargs):
    for resp in rpc_host_config(stub.SetHostConfig, **kargs):
        yield resp.dev_id, resp.host_id

#---------------------------------------------------------------------------------------------------
def configure_host(client, **kargs):
    for dev_id, host_id in rpc_set_host_config(client.stub, **kargs):
        click.echo(f'Configured host ID {host_id} on device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_host_config(dev_id, host_id, config):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Host ID: {host_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    rows.append(f'DMA:')
    rows.append(f'    Base queue:       {config.dma.base_queue}')
    rows.append(f'    Number of queues: {config.dma.num_queues}')

    click.echo('\n'.join(rows))

def show_host_config(client, **kargs):
    for dev_id, host_id, config in rpc_get_host_config(client.stub, **kargs):
        _show_host_config(dev_id, host_id, config)

#---------------------------------------------------------------------------------------------------
def batch_generate_host_config_req(op, **kargs):
    yield BatchRequest(op=op, host_config=host_config_req(**kargs))

def batch_process_host_config_resp(resp):
    if not resp.HasField('host_config'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
        BatchOperation.BOP_SET: 'Configured',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.host_config
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_host_config(resp.dev_id, resp.host_id, resp.config)
    else:
        click.echo(f'{op_label} host ID {resp.host_id} on device ID {resp.dev_id}.')
    return True

def batch_host_config(op, **kargs):
    return batch_generate_host_config_req(op, **kargs), batch_process_host_config_resp

#---------------------------------------------------------------------------------------------------
def host_stats_req(dev_id, host_id, **stats_kargs):
    req_kargs = {'dev_id': dev_id, 'host_id': host_id}
    if stats_kargs:
        filters_kargs = {}
        if not stats_kargs.get('zeroes'):
            filters_kargs['non_zero'] = True

        if filters_kargs:
            req_kargs['filters'] = StatsFilters(**filters_kargs)

    return HostStatsRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_host_stats(op, **kargs):
    req = host_stats_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_clear_host_stats(stub, **kargs):
    for resp in rpc_host_stats(stub.ClearHostStats, **kargs):
        yield resp.dev_id, resp.host_id

def rpc_get_host_stats(stub, **kargs):
    for resp in rpc_host_stats(stub.GetHostStats, **kargs):
        yield resp.dev_id, resp.host_id, resp.stats

#---------------------------------------------------------------------------------------------------
def clear_host_stats(client, **kargs):
    for dev_id, host_id in rpc_clear_host_stats(client.stub, **kargs):
        click.echo(f'Cleared statistics for host ID {host_id} on device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_host_stats(dev_id, host_id, stats, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Host ID: {host_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    name_len = 0
    value_len = 0
    metrics = {}
    for metric in stats.metrics:
        name = metric.name
        name_len = max(name_len, len(name))

        svalue = f'{metric.value.u64}'
        value_len = max(value_len, len(svalue))

        metrics[name] = {
            'value': svalue,
            'last_update': format_timestamp(metric.last_update),
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

def show_host_stats(client, **kargs):
    for dev_id, host_id, stats in rpc_get_host_stats(client.stub, **kargs):
        _show_host_stats(dev_id, host_id, stats, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_host_stats_req(op, **kargs):
    yield BatchRequest(op=op, host_stats=host_stats_req(**kargs))

def batch_process_host_stats_resp(kargs):
    def process(resp):
        if not resp.HasField('host_stats'):
            return False

        supported_ops = {
            BatchOperation.BOP_GET: 'Got',
            BatchOperation.BOP_CLEAR: 'Cleared',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.host_stats
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_host_stats(resp.dev_id, resp.host_id, resp.stats, kargs)
        else:
            click.echo(f'{op_label} statistics for host ID {resp.host_id} '
                       f'on device ID {resp.dev_id}.')
        return True

    return process

def batch_host_stats(op, **kargs):
    return batch_generate_host_stats_req(op, **kargs), batch_process_host_stats_resp(kargs)

#---------------------------------------------------------------------------------------------------
host_id_option = click.option(
    '--host-id', '-i',
    type=click.INT,
    default=-1,
    help='''
    0-based index of the host interface to operate on. Leave unset or set to -1 for all interfaces.
    ''',
)

def clear_host_stats_options(fn):
    options = (
        device_id_option,
        host_id_option,
    )
    return apply_options(options, fn)

def configure_host_options(fn):
    options = (
        device_id_option,
        host_id_option,
        click.option(
            '--dma-base-queue', '-b',
            type=click.INT,
            help='''
            The base queue from which all DMA queues allocated to the host interface are relative.
            ''',
        ),
        click.option(
            '--dma-num-queues', '-n',
            type=click.INT,
            help='Number of DMA queues to allocate to the host interface.',
        ),
    )
    return apply_options(options, fn)

def show_host_options(fn):
    options = (
        device_id_option,
        host_id_option,
    )
    return apply_options(options, fn)

def show_host_stats_options(fn):
    options = (
        device_id_option,
        host_id_option,
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
    @cmd.command(name='clear-host-stats')
    @clear_host_stats_options
    def clear_host_stats(**kargs):
        '''
        Clear the statistics of SmartNIC host interfaces.
        '''
        return batch_host_stats(BatchOperation.BOP_CLEAR, **kargs)

    @cmd.command(name='configure-host')
    @configure_host_options
    def configure_host(**kargs):
        '''
        Configuration for SmartNIC host interfaces.
        '''
        return batch_host_config(BatchOperation.BOP_SET, **kargs)

    @cmd.command(name='show-host-config')
    @show_host_options
    def show_host_config(**kargs):
        '''
        Display the configuration of SmartNIC host interfaces.
        '''
        return batch_host_config(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-host-stats')
    @show_host_stats_options
    def show_host_stats(**kargs):
        '''
        Display the statistics of SmartNIC host interfaces.
        '''
        return batch_host_stats(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_clear_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def host(ctx):
        '''
        Clear for SmartNIC host interfaces.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1, 'host_id': -1}
            clear_host_stats(client, **kargs)

    @host.command
    @clear_host_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Clear the statistics of SmartNIC host interfaces.
        '''
        clear_host_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_configure_commands(cmd):
    @cmd.command
    @configure_host_options
    @click.pass_context
    def host(ctx, **kargs):
        '''
        Configuration for SmartNIC host interfaces.
        '''
        configure_host(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def host(ctx):
        '''
        Display for SmartNIC host interfaces.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1, 'host_id': -1}
            show_host_config(client, **kargs)

    @host.command
    @show_host_options
    @click.pass_context
    def config(ctx, **kargs):
        '''
        Display the configuration of SmartNIC host interfaces.
        '''
        show_host_config(ctx.obj, **kargs)

    @host.command
    @show_host_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Display the statistics of SmartNIC host interfaces.
        '''
        show_host_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_configure_commands(cmds.configure)
    add_show_commands(cmds.show)
