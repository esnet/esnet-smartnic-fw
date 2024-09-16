#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
    'pipeline_id_option',
)

import click
import grpc

from sn_p4_proto.v2 import (
    BatchOperation,
    BatchRequest,
    CounterType,
    ErrorCode,
    MatchType,
    PipelineInfoRequest,
    StatsFilters,
    PipelineStatsRequest,
    TableEndian,
    TableMode,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options, format_timestamp, natural_sort_key

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
MATCH_TYPE_MAP = {
    MatchType.MATCH_TYPE_UNKNOWN: 'unknown',
    MatchType.MATCH_TYPE_BITFIELD: 'bit-field',
    MatchType.MATCH_TYPE_CONSTANT: 'constant',
    MatchType.MATCH_TYPE_PREFIX: 'prefix',
    MatchType.MATCH_TYPE_RANGE: 'range',
    MatchType.MATCH_TYPE_TERNARY: 'ternary',
    MatchType.MATCH_TYPE_UNUSED: 'unused',
}
MATCH_TYPE_RMAP = dict((name, enum) for enum, name in MATCH_TYPE_MAP.items())

TABLE_ENDIAN_MAP = {
    TableEndian.TABLE_ENDIAN_UNKNOWN: 'unknown',
    TableEndian.TABLE_ENDIAN_LITTLE: 'little',
    TableEndian.TABLE_ENDIAN_BIG: 'big',
}
TABLE_ENDIAN_RMAP = dict((name, enum) for enum, name in TABLE_ENDIAN_MAP.items())

TABLE_MODE_MAP = {
    TableMode.TABLE_MODE_UNKNOWN: 'unknown',
    TableMode.TABLE_MODE_BCAM: 'BCAM',
    TableMode.TABLE_MODE_STCAM: 'STCAM',
    TableMode.TABLE_MODE_TCAM: 'TCAM',
    TableMode.TABLE_MODE_DCAM: 'DCAM',
    TableMode.TABLE_MODE_TINY_BCAM: 'tiny-BCAM',
    TableMode.TABLE_MODE_TINY_TCAM: 'tiny-TCAM',
}
TABLE_MODE_RMAP = dict((name, enum) for enum, name in TABLE_MODE_MAP.items())

COUNTER_TYPE_MAP = {
    CounterType.COUNTER_TYPE_UNKNOWN: 'unknown',
    CounterType.COUNTER_TYPE_PACKETS: 'packets',
    CounterType.COUNTER_TYPE_BYTES: 'bytes',
    CounterType.COUNTER_TYPE_PACKETS_AND_BYTES: 'packets-and-bytes',
    CounterType.COUNTER_TYPE_FLAG: 'flag',
}
COUNTER_TYPE_RMAP = dict((name, enum) for enum, name in COUNTER_TYPE_MAP.items())

#---------------------------------------------------------------------------------------------------
def pipeline_info_req(dev_id, pipeline_id, **kargs):
    return PipelineInfoRequest(dev_id=dev_id, pipeline_id=pipeline_id)

#---------------------------------------------------------------------------------------------------
def rpc_pipeline_info(op, **kargs):
    req = pipeline_info_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_pipeline_info(stub, **kargs):
    for resp in rpc_pipeline_info(stub.GetPipelineInfo, **kargs):
        yield resp.dev_id, resp.pipeline_id, resp.info

#---------------------------------------------------------------------------------------------------
def _show_pipeline_info(dev_id, pipeline_id, info):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Pipeline ID: {pipeline_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    COL_WIDTH = 28
    def add_sep_row(indent):
        indent *= 4
        prefix = ' ' * indent
        sep = f'{"":-<{COL_WIDTH-indent}}'
        rows.append(prefix + sep)

    def add_row(indent, label, value):
        prefix = '    ' * indent
        header = f'{prefix}{label}:'
        row = f'{header:<{COL_WIDTH}}'
        if value is not None:
            row += f' {value}'
        rows.append(row)

    add_row(0, 'Name', info.name)
    add_row(0, 'Number of Tables', len(info.tables))
    add_row(0, 'Number of Counter Blocks', len(info.counter_blocks))

    add_row(0, 'Tables', None)
    for table in info.tables:
        add_sep_row(1)
        add_row(1, 'Name', table.name)
        add_row(1, 'Number of Entries', table.num_entries)
        add_row(1, 'Endian', TABLE_ENDIAN_MAP[table.endian])
        add_row(1, 'Mode', TABLE_MODE_MAP[table.mode])
        if table.mode == TableMode.TABLE_MODE_STCAM:
            add_row(1, 'Number of Masks', table.num_masks)
        add_row(1, 'Priority Required', table.priority_required)
        add_row(1, 'Bit Widths', None)
        add_row(2, 'Key', table.key_width)
        add_row(2, 'Response', table.response_width)
        add_row(2, 'Priority', 'automatically computed'
                if table.priority_width == 255 else table.priority_width)
        add_row(2, 'Action ID', table.action_id_width)

        add_row(2, 'Matches', None)
        for match in table.matches:
            add_sep_row(3)
            add_row(3, 'Type', MATCH_TYPE_MAP[match.type])
            add_row(3, 'Bit Width', match.width)

        add_row(2, 'Actions', None)
        for action in table.actions:
            add_sep_row(3)
            add_row(3, 'Name', action.name)
            add_row(3, 'Bit Width', action.width)

            if action.parameters:
                add_row(3, 'Parameters', None)
                for param in action.parameters:
                    add_sep_row(4)
                    add_row(4, 'Name', param.name)
                    add_row(4, 'Bit Width', param.width)

    if info.counter_blocks:
        add_row(0, 'Counter Blocks', None)
        for block in info.counter_blocks:
            add_sep_row(1)
            add_row(1, 'Name', block.name)
            add_row(1, 'Type', COUNTER_TYPE_MAP[block.type])
            add_row(1, 'Width', block.width)
            add_row(1, 'Number of Counters', block.num_counters)

    click.echo('\n'.join(rows))

#---------------------------------------------------------------------------------------------------
def show_pipeline_info(client, **kargs):
    for dev_id, pipeline_id, info in rpc_get_pipeline_info(client.stub, **kargs):
        _show_pipeline_info(dev_id, pipeline_id, info)

#---------------------------------------------------------------------------------------------------
def batch_generate_pipeline_info_req(op, **kargs):
    yield BatchRequest(op=op, pipeline_info=pipeline_info_req(**kargs))

def batch_process_pipeline_info_resp(resp):
    if not resp.HasField('pipeline_info'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.pipeline_info
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_pipeline_info(resp.dev_id, resp.pipeline_id, resp.info)
    return True

def batch_pipeline_info(op, **kargs):
    return batch_generate_pipeline_info_req(op, **kargs), batch_process_pipeline_info_resp

#---------------------------------------------------------------------------------------------------
def pipeline_stats_req(dev_id, pipeline_id, **stats_kargs):
    req_kargs = {'dev_id': dev_id, 'pipeline_id': pipeline_id}
    if stats_kargs:
        filters_kargs = {}
        if not stats_kargs.get('zeroes'):
            filters_kargs['non_zero'] = True

        if filters_kargs:
            req_kargs['filters'] = StatsFilters(**filters_kargs)

    return PipelineStatsRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_pipeline_stats(op, **kargs):
    req = pipeline_stats_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_clear_pipeline_stats(stub, **kargs):
    for resp in rpc_pipeline_stats(stub.ClearPipelineStats, **kargs):
        yield resp.dev_id, resp.pipeline_id

def rpc_get_pipeline_stats(stub, **kargs):
    for resp in rpc_pipeline_stats(stub.GetPipelineStats, **kargs):
        yield resp.dev_id, resp.pipeline_id, resp.stats

#---------------------------------------------------------------------------------------------------
def clear_pipeline_stats(client, **kargs):
    for dev_id, pipeline_id in rpc_clear_pipeline_stats(client.stub, **kargs):
        click.echo(f'Cleared statistics for pipeline ID {pipeline_id} on device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_pipeline_stats(dev_id, pipeline_id, stats, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Pipeline ID: {pipeline_id} on device ID {dev_id}')
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

def show_pipeline_stats(client, **kargs):
    for dev_id, pipeline_id, stats in rpc_get_pipeline_stats(client.stub, **kargs):
        _show_pipeline_stats(dev_id, pipeline_id, stats, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_pipeline_stats_req(op, **kargs):
    yield BatchRequest(op=op, pipeline_stats=pipeline_stats_req(**kargs))

def batch_process_pipeline_stats_resp(kargs):
    def process(resp):
        if not resp.HasField('pipeline_stats'):
            return False

        supported_ops = {
            BatchOperation.BOP_GET: 'Got',
            BatchOperation.BOP_CLEAR: 'Cleared',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.pipeline_stats
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_pipeline_stats(resp.dev_id, resp.pipeline_id, resp.stats, kargs)
        else:
            click.echo(f'{op_label} statistics for pipeline ID {resp.pipeline_id} '
                       f'on device ID {resp.dev_id}.')
        return True

    return process

def batch_pipeline_stats(op, **kargs):
    return batch_generate_pipeline_stats_req(op, **kargs), batch_process_pipeline_stats_resp(kargs)

#---------------------------------------------------------------------------------------------------
pipeline_id_option = click.option(
    '--pipeline-id', '-p',
    'pipeline_id', # Name of keyword argument passed to command handler.
    type=click.INT,
    default=0,
    help='0-based index of the pipeline to operate on. Set to -1 for all pipelines.',
)

def clear_pipeline_stats_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
    )
    return apply_options(options, fn)

def show_pipeline_info_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
    )
    return apply_options(options, fn)

def show_pipeline_stats_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
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
    @cmd.command(name='clear-pipeline-stats')
    @clear_pipeline_stats_options
    def clear_pipeline_stats(**kargs):
        '''
        Clear the statistics of SmartNIC P4 pipelines.
        '''
        return batch_pipeline_stats(BatchOperation.BOP_CLEAR, **kargs)

    @cmd.command(name='show-pipeline-info')
    @show_pipeline_info_options
    def show_pipeline_info(**kargs):
        '''
        Display information of SmartNIC P4 pipelines.
        '''
        return batch_pipeline_info(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-pipeline-stats')
    @show_pipeline_stats_options
    def show_pipeline_stats(**kargs):
        '''
        Display the statistics of SmartNIC P4 pipelines.
        '''
        return batch_pipeline_stats(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_clear_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def pipeline(ctx):
        '''
        Clear for SmartNIC P4 pipelines.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1, 'pipeline_id': -1}
            clear_pipeline_stats(client, **kargs)

    @pipeline.command
    @clear_pipeline_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Clear the statistics of SmartNIC P4 pipelines.
        '''
        clear_pipeline_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def pipeline(ctx):
        '''
        Display SmartNIC P4 pipelines.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1, 'pipeline_id': -1}
            show_pipeline_info(client, **kargs)

    @pipeline.command
    @show_pipeline_info_options
    @click.pass_context
    def info(ctx, **kargs):
        '''
        Display information of SmartNIC P4 pipelines.
        '''
        show_pipeline_info(ctx.obj, **kargs)

    @pipeline.command
    @show_pipeline_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Display the statistics of SmartNIC P4 pipelines.
        '''
        show_pipeline_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_show_commands(cmds.show)
