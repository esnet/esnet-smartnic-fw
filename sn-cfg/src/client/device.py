#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
    'device_id_option',
)

import click
import grpc

from sn_cfg_proto import (
    BatchOperation,
    BatchRequest,
    DeviceInfo,
    DeviceInfoRequest,
    DeviceStatus,
    DeviceStatusRequest,
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

    card = info.card
    rows.append('Card:')
    rows.append(f'    Name:                  {card.name}')
    rows.append(f'    Profile:               {card.profile}')
    rows.append(f'    Serial Number:         {card.serial_number}')
    rows.append(f'    Revision:              {card.revision}')
    rows.append(f'    SC Version:            {card.sc_version}')
    rows.append(f'    Config Mode:           {card.config_mode}')
    rows.append(f'    Fan Presence:          {card.fan_presence}')
    rows.append(f'    Total Power Available: {card.total_power_avail}W')

    if card.cage_types:
        rows.append(f'    Cage Types:')
        for i, cage_type in enumerate(card.cage_types):
            rows.append(f'        {i}: {cage_type}')

    if card.mac_addrs:
        rows.append(f'    MAC Addresses:')
        for i, addr in enumerate(card.mac_addrs):
            rows.append(f'        {i}: {addr}')

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
def device_status_req(dev_id, **status_kargs):
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
def _show_device_status(dev_id, status, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    if status.alarms:
        all_alarms = kargs.get('all_alarms', False)
        alarms = {}
        for alarm in status.alarms:
            if all_alarms or alarm.active:
                src = alarms.setdefault(alarm.source, {})
                src[alarm.name] = "yes" if alarm.active else "no"

        if alarms:
            rows.append('Device Alarms:')
            for src in sorted(alarms):
                rows.append(f'    {src}:')

                name_len = max(len(name) for name in alarms[src])
                for name in sorted(alarms[src]):
                    rows.append(f'        {name:>{name_len}}: {alarms[src][name]}')

    if status.monitors:
        monitors = {}
        for mon in status.monitors:
            src = monitors.setdefault(mon.source, {})
            src[mon.name] = f'{mon.value:.4g}'

        rows.append('Device Monitors:')
        for src in sorted(monitors):
            rows.append(f'    {src}:')

            name_len = max(len(name) for name in monitors[src])
            for name in sorted(monitors[src]):
                rows.append(f'        {name:>{name_len}}: {monitors[src][name]}')

    click.echo('\n'.join(rows))

def show_device_status(client, **kargs):
    for dev_id, status in rpc_get_device_status(client.stub, **kargs):
        _show_device_status(dev_id, status, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_device_status_req(op, **kargs):
    yield BatchRequest(op=op, device_status=device_status_req(**kargs))

def batch_process_device_status_resp(kargs):
    def process(resp):
        if not resp.HasField('device_status'):
            return False

        supported_ops = {
            BatchOperation.BOP_GET: 'Got',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.device_status
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_device_status(resp.dev_id, resp.status, kargs)
        return True

    return process

def batch_device_status(op, **kargs):
    return batch_generate_device_status_req(op, **kargs), batch_process_device_status_resp(kargs)

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

def device_status_options(fn):
    options = (
        device_id_option,
        click.option(
            '--all-alarms',
            is_flag=True,
            help='''
            Include all alarms in the display. Default is to only display active alarms.
            ''',
        ),
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
