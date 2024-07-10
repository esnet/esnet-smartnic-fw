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
    ErrorCode,
    MatchType,
    PipelineInfoRequest,
    TableEndian,
    TableMode,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options

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

    COL_WIDTH = 20
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

    supported_ops = (
        BatchOperation.BOP_GET,
    )
    if resp.op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {resp.op}')

    resp = resp.pipeline_info
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('info'):
        _show_pipeline_info(resp.dev_id, resp.pipeline_id, resp.info)
    return True

def batch_pipeline_info(op, **kargs):
    return batch_generate_pipeline_info_req(op, **kargs), batch_process_pipeline_info_resp

#---------------------------------------------------------------------------------------------------
pipeline_id_option = click.option(
    '--pipeline-id', '-p',
    'pipeline_id', # Name of keyword argument passed to command handler.
    type=click.INT,
    default=0,
    help='0-based index of the pipeline to operate on. Set to -1 for all pipelines.',
)

def pipeline_info_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='show-pipeline-info')
    @pipeline_info_options
    def show_pipeline_info(**kargs):
        '''
        Display information about a SmartNIC P4 pipeline.
        '''
        return batch_pipeline_info(BatchOperation.BOP_GET, **kargs)

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
    @pipeline_info_options
    @click.pass_context
    def info(ctx, **kargs):
        '''
        Display information about a SmartNIC P4 pipeline.
        '''
        show_pipeline_info(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_show_commands(cmds.show)
