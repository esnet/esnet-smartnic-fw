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
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options

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

    resp = resp.host_config
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('config'):
        _show_host_config(resp.dev_id, resp.host_id, resp.config)
    else:
        click.echo(f'Configured host ID {resp.host_id} on device ID {resp.dev_id}.')
    return True

def batch_host_config(op, **kargs):
    return batch_generate_host_config_req(op, **kargs), batch_process_host_config_resp

#---------------------------------------------------------------------------------------------------
host_id_option = click.option(
    '--host-id', '-i',
    type=click.INT,
    default=-1,
    help='''
    0-based index of the host interface to operate on. Leave unset or set to -1 for all interfaces.
    ''',
)

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

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
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

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_configure_commands(cmds.configure)
    add_show_commands(cmds.show)
