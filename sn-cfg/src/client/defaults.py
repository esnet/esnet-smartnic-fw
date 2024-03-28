#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import grpc

from .device import device_id_option
from .error import error_code_str
from .sn_cfg_v1_pb2 import (
    BatchOperation,
    BatchRequest,
    DefaultsProfile,
    DefaultsRequest,
    ErrorCode,
)
from .utils import apply_options

#---------------------------------------------------------------------------------------------------
PROFILE_MAP = {
    DefaultsProfile.DS_ONE_TO_ONE: 'one-to-one',
}
PROFILE_RMAP = dict((name, enum) for enum, name in PROFILE_MAP.items())

#---------------------------------------------------------------------------------------------------
def defaults_req(dev_id, profile):
    return DefaultsRequest(dev_id=dev_id, profile=PROFILE_RMAP[profile])

#---------------------------------------------------------------------------------------------------
def rpc_defaults(op, **kargs):
    req = defaults_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException('RPC {rpc!r} failed: ' + str(e))

def rpc_set_defaults(stub, **kargs):
    for resp in rpc_defaults(stub.SetDefaults, **kargs):
        yield resp.dev_id

#---------------------------------------------------------------------------------------------------
def configure_defaults(client, **kargs):
    for dev_id in rpc_set_defaults(client.stub, **kargs):
        click.echo(f'Applied defaults on device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def batch_generate_defaults_req(op, **kargs):
    yield BatchRequest(op=op, defaults=defaults_req(**kargs))

def batch_process_defaults_resp(resp):
    if not resp.HasField('defaults'):
        return False

    resp = resp.defaults
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    click.echo(f'Applied defaults on device ID {resp.dev_id}.')
    return True

def batch_defaults(op, **kargs):
    return batch_generate_defaults_req(op, **kargs), batch_process_defaults_resp

#---------------------------------------------------------------------------------------------------
def configure_defaults_options(fn):
    options = (
        device_id_option,
        click.option(
            '--profile', '-p',
            type=click.Choice(sorted(name for name in PROFILE_RMAP)),
            default='one-to-one',
            show_default=True,
            help='Defaults profile to apply to the device.',
        ),
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='configure-defaults')
    @configure_defaults_options
    def configure_defaults(**kargs):
        '''
        Configuration for SmartNIC defaults.
        '''
        return batch_defaults(BatchOperation.BOP_SET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_configure_commands(cmd):
    @cmd.command
    @configure_defaults_options
    @click.pass_context
    def defaults(ctx, **kargs):
        '''
        Configuration for SmartNIC defaults.
        '''
        configure_defaults(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_configure_commands(cmds.configure)
