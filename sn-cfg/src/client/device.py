#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
    'device_id_option',
)

import click
import grpc

from .error import error_code_str
from .sn_cfg_v1_pb2 import (
    BatchOperation,
    BatchRequest,
    DeviceInfo,
    DeviceInfoRequest,
    DeviceStatus,
    DeviceStatusRequest,
    ErrorCode,
)
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

    rows.append('PCI:')
    rows.append(f'    Bus ID:    {info.pci.bus_id}')
    rows.append(f'    Vendor ID: 0x{info.pci.vendor_id:04x}')
    rows.append(f'    Device ID: 0x{info.pci.device_id:04x}')

    rows.append('Build:')
    rows.append(f'    Number: 0x{info.build.number:08x}')
    rows.append(f'    Status: 0x{info.build.status:08x}')
    for i, d in enumerate(info.build.dna):
        rows.append(f'    DNA[{i}]: 0x{d:08x}')

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

    resp = resp.device_info
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('info'):
        _show_device_info(resp.dev_id, resp.info)
    return True

def batch_device_info(op, **kargs):
    return batch_generate_device_info_req(op, **kargs), batch_process_device_info_resp

#---------------------------------------------------------------------------------------------------
def device_status_req(dev_id):
    return DeviceStatusRequest(dev_id=dev_id)

#---------------------------------------------------------------------------------------------------
def rpc_device_status(op, **kargs):
    req = device_status_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_device_status(stub, **kargs):
    for resp in rpc_device_status(stub.GetDeviceStatus, **kargs):
        yield resp.dev_id, resp.status

#---------------------------------------------------------------------------------------------------
def _show_device_status(dev_id, status):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    if status.sysmons:
        rows.append('System Monitors:')
        for sm in status.sysmons:
            rows.append(f'    {sm.index}:')
            rows.append(f'        temperature: {sm.temperature:7.3f}℃')
    click.echo('\n'.join(rows))

def show_device_status(client, **kargs):
    for dev_id, status in rpc_get_device_status(client.stub, **kargs):
        _show_device_status(dev_id, status)

#---------------------------------------------------------------------------------------------------
def batch_generate_device_status_req(op, **kargs):
    yield BatchRequest(op=op, device_status=device_status_req(**kargs))

def batch_process_device_status_resp(resp):
    if not resp.HasField('device_status'):
        return False

    resp = resp.device_status
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('status'):
        _show_device_status(resp.dev_id, resp.status)
    return True

def batch_device_status(op, **kargs):
    return batch_generate_device_status_req(op, **kargs), batch_process_device_status_resp

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

device_status_options = device_info_options

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='show-device-info')
    @device_info_options
    def show_device_info(**kargs):
        '''
        Display information about a SmartNIC device.
        '''
        return batch_device_info(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-device-status')
    @device_status_options
    def show_device_status(**kargs):
        '''
        Display the status of a SmartNIC device.
        '''
        return batch_device_status(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def device(ctx):
        '''
        Display SmartNIC devices.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1}
            show_device_info(client, **kargs)
            show_device_status(client, **kargs)

    @device.command
    @device_info_options
    @click.pass_context
    def info(ctx, **kargs):
        '''
        Display information about a SmartNIC device.
        '''
        show_device_info(ctx.obj, **kargs)

    @device.command
    @device_status_options
    @click.pass_context
    def status(ctx, **kargs):
        '''
        Display the status of a SmartNIC device.
        '''
        show_device_status(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_show_commands(cmds.show)
