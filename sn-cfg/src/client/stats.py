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
    StatsRequest,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
def stats_req(dev_id):
    return StatsRequest(dev_id=dev_id)

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
def _show_stats(dev_id, stats):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    for cnt in stats.counters:
        if cnt.value != 0:
            rows.append(f'{cnt.zone}_{cnt.block}_{cnt.name}: {cnt.value}')
    click.echo('\n'.join(rows))

def show_stats(client, **kargs):
    for dev_id, stats in rpc_get_stats(client.stub, **kargs):
        _show_stats(dev_id, stats)

#---------------------------------------------------------------------------------------------------
def batch_generate_stats_req(op, **kargs):
    yield BatchRequest(op=op, stats=stats_req(**kargs))

def batch_process_stats_resp(resp):
    if not resp.HasField('stats'):
        return False

    resp = resp.stats
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('stats'):
        _show_stats(resp.dev_id, resp.stats)
    else:
        click.echo(f'Cleared statistics for device ID {resp.dev_id}.')
    return True

def batch_stats(op, **kargs):
    return batch_generate_stats_req(op, **kargs), batch_process_stats_resp

#---------------------------------------------------------------------------------------------------
def clear_stats_options(fn):
    options = (
        device_id_option,
    )
    return apply_options(options, fn)

def show_stats_options(fn):
    options = (
        device_id_option,
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
