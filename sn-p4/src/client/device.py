#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
    'device_id_option',
)

import click
import grpc

from sn_p4_proto.v2 import (
    BatchOperation,
    BatchRequest,
    DeviceInfo,
    DeviceInfoRequest,
    ErrorCode,
)

from .error import error_code_str
from .utils import apply_options

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
def device_info_req(dev_id):
    return DeviceInfoRequest(dev_id=dev_id)

#---------------------------------------------------------------------------------------------------
def rpc_device_info(op, **kargs):
    req = device_info_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_device_info(stub, **kargs):
    for resp in rpc_device_info(stub.GetDeviceInfo, **kargs):
        yield resp.dev_id, resp.info

#---------------------------------------------------------------------------------------------------
def _show_device_info(dev_id, info):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    pci = info.pci
    rows.append('PCI:')
    rows.append(f'    Bus ID:    {pci.bus_id}')
    rows.append(f'    Vendor ID: 0x{pci.vendor_id:04x}')
    rows.append(f'    Device ID: 0x{pci.device_id:04x}')

    build = info.build
    rows.append('Build:')
    rows.append(f'    Number: {build.number}')
    rows.append(f'    Status: 0x{build.status:08x}')
    for i, d in enumerate(build.dna):
        rows.append(f'    DNA[{i}]: 0x{d:08x}')

    env = build.env
    rows.append( '    Environment:')
    rows.append(f'        HW Version: {env.hw_version}')
    rows.append(f'        FW Version: {env.fw_version}')
    rows.append(f'        SW Version: {env.sw_version}')

    click.echo('\n'.join(rows))

#---------------------------------------------------------------------------------------------------
def show_device_info(client, **kargs):
    for dev_id, info in rpc_get_device_info(client.stub, **kargs):
        _show_device_info(dev_id, info)

#---------------------------------------------------------------------------------------------------
def batch_generate_device_info_req(op, **kargs):
    yield BatchRequest(op=op, device_info=device_info_req(**kargs))

def batch_process_device_info_resp(resp):
    if not resp.HasField('device_info'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.device_info
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_device_info(resp.dev_id, resp.info)
    return True

def batch_device_info(op, **kargs):
    return batch_generate_device_info_req(op, **kargs), batch_process_device_info_resp

#---------------------------------------------------------------------------------------------------
device_id_option = click.option(
    '--device-id', '-d',
    'dev_id', # Name of keyword argument passed to command handler.
    type=click.INT,
    default=-1,
    help='0-based index of the device to operate on. Leave unset or set to -1 for all devices.',
)

def device_info_options(fn):
    options = (
        device_id_option,
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='show-device-info')
    @device_info_options
    def show_device_info(**kargs):
        '''
        Display information about a SmartNIC P4 device.
        '''
        return batch_device_info(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def device(ctx):
        '''
        Display SmartNIC P4 devices.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1}
            show_device_info(client, **kargs)

    @device.command
    @device_info_options
    @click.pass_context
    def info(ctx, **kargs):
        '''
        Display information about a SmartNIC P4 device.
        '''
        show_device_info(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_show_commands(cmds.show)
