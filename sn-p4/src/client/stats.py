#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import grpc

from sn_p4_proto.v2 import (
    BatchOperation,
    BatchRequest,
    ErrorCode,
    StatsFilters,
    StatsMetricType,
    StatsRequest,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options, natural_sort_key

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
METRIC_TYPE_MAP = {
    StatsMetricType.STATS_METRIC_TYPE_COUNTER: 'counter',
    StatsMetricType.STATS_METRIC_TYPE_GAUGE: 'gauge',
    StatsMetricType.STATS_METRIC_TYPE_FLAG: 'flag',
}
METRIC_TYPE_RMAP = dict((name, enum) for enum, name in METRIC_TYPE_MAP.items())

#---------------------------------------------------------------------------------------------------
def stats_req(dev_id, **stats_kargs):
    req_kargs = {'dev_id': dev_id}
    if stats_kargs:
        filters_kargs = {}

        key = 'metric_types'
        names = stats_kargs.get(key)
        if names:
            enums = []
            for name in names:
                enums.append(METRIC_TYPE_RMAP[name])
            filters_kargs[key] = enums

        if not stats_kargs.get('zeroes'):
            filters_kargs['non_zero'] = True

        if filters_kargs:
            req_kargs['filters'] = StatsFilters(**filters_kargs)

    return StatsRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_stats(op, **kargs):
    req = stats_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_clear_stats(stub, **kargs):
    for resp in rpc_stats(stub.ClearStats, **kargs):
        yield resp.dev_id

def rpc_get_stats(stub, **kargs):
    for resp in rpc_stats(stub.GetStats, **kargs):
        yield resp.dev_id, resp.stats

#---------------------------------------------------------------------------------------------------
def clear_stats(client, **kargs):
    for dev_id in rpc_clear_stats(client.stub, **kargs):
        click.echo(f'Cleared statistics for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_stats(dev_id, stats, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    metrics = {}
    for metric in stats.metrics:
        base_name = metric.scope.zone
        if not metric.name.startswith(metric.scope.block):
            base_name += f'_{metric.scope.block}'
        base_name += f'_{metric.name}'

        is_array = metric.num_elements > 0
        for value in metric.values:
            if metric.type == StatsMetricType.STATS_METRIC_TYPE_FLAG:
                svalue = 'yes' if value.u64 != 0 else 'no'
            elif metric.type == StatsMetricType.STATS_METRIC_TYPE_GAUGE:
                svalue = f'{value.f64:.4g}'
            else:
                svalue = f'{value.u64}'

            name = base_name
            if is_array:
                name += f'[{value.index}]'
            metrics[name] = svalue

    if metrics:
        name_len = max(len(name) for name in metrics)
        for name in sorted(metrics, key=natural_sort_key):
            rows.append(f'{name:>{name_len}}: {metrics[name]}')

    click.echo('\n'.join(rows))

def show_stats(client, **kargs):
    for dev_id, stats in rpc_get_stats(client.stub, **kargs):
        _show_stats(dev_id, stats, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_stats_req(op, **kargs):
    yield BatchRequest(op=op, stats=stats_req(**kargs))

def batch_process_stats_resp(kargs):
    def process(resp):
        if not resp.HasField('stats'):
            return False

        resp = resp.stats
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if resp.HasField('stats'):
            _show_stats(resp.dev_id, resp.stats, kargs)
        else:
            click.echo(f'Cleared statistics for device ID {resp.dev_id}.')
        return True

    return process

def batch_stats(op, **kargs):
    return batch_generate_stats_req(op, **kargs), batch_process_stats_resp(kargs)

#---------------------------------------------------------------------------------------------------
def clear_stats_options(fn):
    options = (
        device_id_option,
    )
    return apply_options(options, fn)

def show_stats_options(fn):
    options = (
        device_id_option,
        click.option(
            '--metric-type', '-m',
            'metric_types',
            type=click.Choice(sorted(name for name in METRIC_TYPE_RMAP)),
            multiple=True,
            help='Filter to restrict statistic metrics to the given type(s).',
        ),
        click.option(
            '--zeroes',
            is_flag=True,
            help='Include zero valued statistic metrics in the display.',
        ),
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='clear-stats')
    @clear_stats_options
    def clear_stats(**kargs):
        '''
        Clear SmartNIC statistics.
        '''
        return batch_stats(BatchOperation.BOP_CLEAR, **kargs)

    @cmd.command(name='show-stats')
    @show_stats_options
    def show_stats(**kargs):
        '''
        Display SmartNIC statistics.
        '''
        return batch_stats(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_clear_commands(cmd):
    @cmd.command
    @clear_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Clear SmartNIC statistics.
        '''
        clear_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.command
    @show_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Display SmartNIC statistics.
        '''
        show_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_show_commands(cmds.show)
