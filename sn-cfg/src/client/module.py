#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import grpc
import string

from sn_cfg_proto import (
    BatchOperation,
    BatchRequest,
    ErrorCode,
    ModuleGpio,
    ModuleGpioRequest,
    ModuleGpioState,
    ModuleInfo,
    ModuleInfoRequest,
    ModuleMem,
    ModuleMemRequest,
    ModuleStatus,
    ModuleStatusRequest,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options, MIXED_INT

HEADER_SEP = '-' * 40
PAGE_SIZE = 128

#---------------------------------------------------------------------------------------------------
GPIO_STATE_MAP = {
    ModuleGpioState.GPIO_STATE_ASSERT: 'assert',
    ModuleGpioState.GPIO_STATE_DEASSERT: 'deassert',
}
GPIO_STATE_RMAP = dict((name, enum) for enum, name in GPIO_STATE_MAP.items())

#---------------------------------------------------------------------------------------------------
def yes_or_no(flag):
    return "yes" if flag else "no"

#---------------------------------------------------------------------------------------------------
def module_gpio_req(dev_id, mod_id, **gpio_kargs):
    req_kargs = {'dev_id': dev_id, 'mod_id': mod_id}
    if gpio_kargs:
        for key in ('reset', 'low_power_mode'):
            value = gpio_kargs.get(key)
            if value is not None:
                gpio_kargs[key] = GPIO_STATE_RMAP[value]
        req_kargs['gpio'] = ModuleGpio(**gpio_kargs)
    return ModuleGpioRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_module_gpio(op, **kargs):
    req = module_gpio_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_module_gpio(stub, **kargs):
    for resp in rpc_module_gpio(stub.GetModuleGpio, **kargs):
        yield resp.dev_id, resp.mod_id, resp.gpio

def rpc_set_module_gpio(stub, **kargs):
    for resp in rpc_module_gpio(stub.SetModuleGpio, **kargs):
        yield resp.dev_id, resp.mod_id

#---------------------------------------------------------------------------------------------------
def configure_module_gpio(client, **kargs):
    for dev_id, mod_id in rpc_set_module_gpio(client.stub, **kargs):
        click.echo(f'Configured GPIOs for module ID {mod_id} on device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_module_gpio(dev_id, mod_id, gpio):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Module ID: {mod_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    rows.append(f'Reset:          {GPIO_STATE_MAP[gpio.reset]}')
    rows.append(f'Low-Power Mode: {GPIO_STATE_MAP[gpio.low_power_mode]}')
    rows.append(f'Select:         {GPIO_STATE_MAP[gpio.select]}')
    rows.append(f'Present:        {GPIO_STATE_MAP[gpio.present]}')
    rows.append(f'Interrupt:      {GPIO_STATE_MAP[gpio.interrupt]}')

    click.echo('\n'.join(rows))

def show_module_gpio(client, **kargs):
    for dev_id, mod_id, gpio in rpc_get_module_gpio(client.stub, **kargs):
        _show_module_gpio(dev_id, mod_id, gpio)

#---------------------------------------------------------------------------------------------------
def batch_generate_module_gpio_req(op, **kargs):
    yield BatchRequest(op=op, module_gpio=module_gpio_req(**kargs))

def batch_process_module_gpio_resp(resp):
    if not resp.HasField('module_gpio'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
        BatchOperation.BOP_SET: 'Configured',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.module_gpio
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_module_gpio(resp.dev_id, resp.mod_id, resp.gpio)
    else:
        click.echo(f'{op_label} GPIOs for module ID {resp.mod_id} on device ID {resp.dev_id}.')
    return True

def batch_module_gpio(op, **kargs):
    return batch_generate_module_gpio_req(op, **kargs), batch_process_module_gpio_resp

#---------------------------------------------------------------------------------------------------
def module_info_req(dev_id, mod_id, **info_kargs):
    req_kargs = {'dev_id': dev_id, 'mod_id': mod_id}
    return ModuleInfoRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_module_info(op, **kargs):
    req = module_info_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_module_info(stub, **kargs):
    for resp in rpc_module_info(stub.GetModuleInfo, **kargs):
        yield resp.dev_id, resp.mod_id, resp.info

#---------------------------------------------------------------------------------------------------
def _show_module_info(dev_id, mod_id, info):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Module ID: {mod_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    vendor = info.vendor
    rows.append( 'Vendor:')
    rows.append(f'    Name:          {vendor.name}')
    rows.append(f'    OUI:           {vendor.oui}')
    rows.append(f'    Revision:      {vendor.revision}')
    rows.append(f'    Part Number:   {vendor.part_number}')
    rows.append(f'    Serial Number: {vendor.serial_number}')
    rows.append(f'    CLEI:          {vendor.clei}')
    rows.append( '    Date:')
    rows.append(f'        Year:      {vendor.date.year}')
    rows.append(f'        Month:     {vendor.date.month}')
    rows.append(f'        Day:       {vendor.date.day}')
    rows.append(f'        Vendor:    {vendor.date.vendor}')

    dev = info.device
    ident = dev.identifier
    opt_pages = ', '.join(f'0x{p:02x}' for p in ident.optional_upper_pages)
    eth = ', '.join(ident.spec_compliance.ethernet)
    sonet = ', '.join(ident.spec_compliance.sonet)
    sas = ', '.join(ident.spec_compliance.sas)
    geth = ', '.join(ident.spec_compliance.gigabit_ethernet)
    fibre_len = ', '.join(ident.spec_compliance.fibre.length)
    fibre_tx = ', '.join(ident.spec_compliance.fibre.tx_technology)
    fibre_media = ', '.join(ident.spec_compliance.fibre.media)
    fibre_speed = ', '.join(ident.spec_compliance.fibre.speed)

    any_fibre = any((fibre_len, fibre_tx, fibre_media, fibre_speed))
    any_spec_comp = any((eth, sonet, sas, geth, any_fibre))

    rows.append( 'Device:')
    rows.append(f'    Identifier:            {ident.identifier}')
    rows.append(f'    Revision Compliance:   {ident.revision_compliance}')
    rows.append(f'    Optional Upper Pages:  {opt_pages}')
    rows.append(f'    Power Class:           {ident.power_class}')
    rows.append(f'    Rx CDR:                {yes_or_no(ident.rx_cdr)}')
    rows.append(f'    Tx CDR:                {yes_or_no(ident.tx_cdr)}')
    rows.append(f'    Connector Type:        {ident.connector_type}')
    rows.append(f'    Encoding:              {ident.encoding}')
    rows.append(f'    Baud Rate:             {ident.baud_rate} Megabaud')

    if any_spec_comp:
        rows.append( '    Spec Compliance:')
        if eth:
            rows.append(f'        Ethernet:          {eth}')
        if sonet:
            rows.append(f'        SONET:             {sonet}')
        if sas:
            rows.append(f'        SAS:               {sas}')
        if geth:
            rows.append(f'        GigaBit Ethernet:  {geth}')

        if any_fibre:
            rows.append( '        Fibre:')
            if fibre_len:
                rows.append(f'            Length:        {fibre_len}')
            if fibre_tx:
                rows.append(f'            Tx Technology: {fibre_tx}')
            if fibre_media:
                rows.append(f'            Media:         {fibre_media}')
            if fibre_speed:
                rows.append(f'            Speed:         {fibre_speed}')

    click.echo('\n'.join(rows))

def show_module_info(client, **kargs):
    for dev_id, mod_id, info in rpc_get_module_info(client.stub, **kargs):
        _show_module_info(dev_id, mod_id, info)

#---------------------------------------------------------------------------------------------------
def batch_generate_module_info_req(op, **kargs):
    yield BatchRequest(op=op, module_info=module_info_req(**kargs))

def batch_process_module_info_resp(resp):
    if not resp.HasField('module_info'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.module_info
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_module_info(resp.dev_id, resp.mod_id, resp.info)
    return True

def batch_module_info(op, **kargs):
    return batch_generate_module_info_req(op, **kargs), batch_process_module_info_resp

#---------------------------------------------------------------------------------------------------
def module_mem_req(dev_id, mod_id, **mem_kargs):
    key = 'data'
    data = mem_kargs.get(key)
    if data is not None:
        mem_kargs[key] = bytes(data)

    req_kargs = {
        'dev_id': dev_id,
        'mod_id': mod_id,
        'mem': ModuleMem(**mem_kargs),
    }
    return ModuleMemRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_module_mem(op, **kargs):
    req = module_mem_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_module_mem(stub, **kargs):
    for resp in rpc_module_mem(stub.GetModuleMem, **kargs):
        yield resp.dev_id, resp.mod_id, resp.mem

def rpc_set_module_mem(stub, **kargs):
    for resp in rpc_module_mem(stub.SetModuleMem, **kargs):
        yield resp.dev_id, resp.mod_id

#---------------------------------------------------------------------------------------------------
def configure_module_mem(client, **kargs):
    for dev_id, mod_id in rpc_set_module_mem(client.stub, **kargs):
        click.echo(f'Configured memory of module ID {mod_id} on device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_module_mem(dev_id, mod_id, mem):
    N_COLS = 16

    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Module ID: {mod_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)
    rows.append(' | '.join(('  ', ' '.join(f'{c:02x}' for c in range(N_COLS)), ' ' * N_COLS)))
    rows.append(' | '.join(('--', '-' * (3 * N_COLS - 1), '-' * N_COLS)))

    start_offset = mem.offset
    end_offset = mem.offset + mem.count - 1
    start_col = start_offset % N_COLS
    end_col = end_offset % N_COLS
    start_row = start_offset // N_COLS
    end_row = end_offset // N_COLS

    def byte_to_ascii(byte):
        if byte is not None:
            char = chr(byte)
            if char.isprintable() and (char == ' ' or char not in string.whitespace):
                return char
        return '.'

    data = [None] * start_col + list(mem.data) + [None] * (N_COLS - end_col - 1)
    data_idx = 0
    for row_idx in range(end_row - start_row + 1):
        data_idx = row_idx * N_COLS
        data_slice = data[data_idx:data_idx+N_COLS]
        row_data = ['  ' if b is None else f'{b:02x}' for b in data_slice]
        row_ascii = [byte_to_ascii(b) for b in data_slice]
        row_offset = (start_row + row_idx) * N_COLS
        rows.append(' | '.join((f'{row_offset:02x}',
                                ' '.join(row_data),
                                ''.join(row_ascii))))

    click.echo('\n'.join(rows))

def show_module_mem(client, **kargs):
    for dev_id, mod_id, mem in rpc_get_module_mem(client.stub, **kargs):
        _show_module_mem(dev_id, mod_id, mem)

#---------------------------------------------------------------------------------------------------
def batch_generate_module_mem_req(op, **kargs):
    yield BatchRequest(op=op, module_mem=module_mem_req(**kargs))

def batch_process_module_mem_resp(resp):
    if not resp.HasField('module_mem'):
        return False

    supported_ops = {
        BatchOperation.BOP_GET: 'Got',
        BatchOperation.BOP_SET: 'Configured',
    }
    op = resp.op
    if op not in supported_ops:
        raise click.ClickException('Response for unsupported batch operation: {op}')
    op_label = supported_ops[op]

    resp = resp.module_mem
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if op == BatchOperation.BOP_GET:
        _show_module_mem(resp.dev_id, resp.mod_id, resp.mem)
    else:
        click.echo(f'{op_label} memory of module ID {resp.mod_id} on device ID {resp.dev_id}.')
    return True

def batch_module_mem(op, **kargs):
    return batch_generate_module_mem_req(op, **kargs), batch_process_module_mem_resp

#---------------------------------------------------------------------------------------------------
def module_status_req(dev_id, mod_id, **status_kargs):
    req_kargs = {'dev_id': dev_id, 'mod_id': mod_id}
    return ModuleStatusRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_module_status(op, **kargs):
    req = module_status_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_module_status(stub, **kargs):
    for resp in rpc_module_status(stub.GetModuleStatus, **kargs):
        yield resp.dev_id, resp.mod_id, resp.status

#---------------------------------------------------------------------------------------------------
def _show_module_status(dev_id, mod_id, status, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Module ID: {mod_id} on device ID {dev_id}')
    rows.append(HEADER_SEP)

    if status.alarms:
        all_alarms = kargs.get('all_alarms', False)
        alarms = {}
        for alarm in status.alarms:
            if all_alarms or alarm.active:
                alarms[alarm.name] = yes_or_no(alarm.active)

        if alarms:
            rows.append('Alarms:')
            name_len = max(len(name) for name in alarms)
            for name in sorted(alarms):
                rows.append(f'    {name:>{name_len}}: {alarms[name]}')

    if status.monitors:
        monitors = {}
        for mon in status.monitors:
            monitors[mon.name] = f'{mon.value:.4g}'
        name_len = max(len(name) for name in monitors)

        rows.append('Monitors:')
        for name in sorted(monitors):
            rows.append(f'    {name:>{name_len}}: {monitors[name]}')

    click.echo('\n'.join(rows))

def show_module_status(client, **kargs):
    for dev_id, mod_id, status in rpc_get_module_status(client.stub, **kargs):
        _show_module_status(dev_id, mod_id, status, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_module_status_req(op, **kargs):
    yield BatchRequest(op=op, module_status=module_status_req(**kargs))

def batch_process_module_status_resp(kargs):
    def process(resp):
        if not resp.HasField('module_status'):
            return False

        supported_ops = {
            BatchOperation.BOP_GET: 'Got',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.module_status
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_module_status(resp.dev_id, resp.mod_id, resp.status, kargs)
        return True

    return process

def batch_module_status(op, **kargs):
    return batch_generate_module_status_req(op, **kargs), batch_process_module_status_resp(kargs)

#---------------------------------------------------------------------------------------------------
module_id_option = click.option(
    '--module-id', '-m',
    'mod_id', # Name of keyword argument passed to command handler.
    type=click.INT,
    default=-1,
    help='''
    0-based index of the pluggable module to operate on. Leave unset or set to -1 for all modules.
    ''',
)

module_mem_offset_option = click.option(
    '--offset', '-o',
    type=MIXED_INT,
    default=0,
    show_default=True,
    help = '''
    Offset into the 256 byte module memory. Memory is divided in 2 128 byte pages, where the lower
    page (bytes 0-127) are fixed and the upper page (bytes 128-255) are dynamically selected by the
    page index.
    ''',
)
module_mem_page_option = click.option(
    '--page', '-p',
    type=MIXED_INT,
    default=0,
    show_default=True,
    help='Index used to select one of 256 possible dynamic upper memory pages to access.',
)

GPIO_STATE_CHOICE = click.Choice(sorted(name for name in GPIO_STATE_RMAP))
def configure_module_gpio_options(fn):
    options = (
        device_id_option,
        module_id_option,
        click.option(
            '--reset',
            type=GPIO_STATE_CHOICE,
            help='Set the state of the reset GPIO signal.',
        ),
        click.option(
            '--low-power-mode',
            type=GPIO_STATE_CHOICE,
            help='Set the state of the low power mode GPIO signal.',
        ),
    )
    return apply_options(options, fn)

def configure_module_mem_options(fn):
    options = (
        device_id_option,
        module_id_option,
        module_mem_offset_option,
        module_mem_page_option,
        click.argument(
            'data',
            type=MIXED_INT,
            nargs=-1,
            required=True,
        ),
    )
    return apply_options(options, fn)

def show_module_options(fn):
    options = (
        device_id_option,
        module_id_option,
    )
    return apply_options(options, fn)

def show_module_mem_options(fn):
    options = (
        device_id_option,
        module_id_option,
        module_mem_offset_option,
        module_mem_page_option,
        click.option(
            '--count', '-c',
            type=MIXED_INT,
            default=2*PAGE_SIZE,
            show_default=True,
            help='Number of bytes to access starting from the specified offset.',
        ),
    )
    return apply_options(options, fn)

def show_module_status_options(fn):
    options = (
        device_id_option,
        module_id_option,
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
    @cmd.command(name='configure-module-gpio')
    @configure_module_gpio_options
    def configure_module_gpio(**kargs):
        '''
        Configure GPIOs of SmartNIC pluggable modules.
        '''
        return batch_module_gpio(BatchOperation.BOP_SET, **kargs)

    @cmd.command(name='configure-module-mem')
    @configure_module_mem_options
    def configure_module_mem(**kargs):
        '''
        Write bytes to management memory of SmartNIC pluggable modules.
        '''
        return batch_module_mem(BatchOperation.BOP_SET, **kargs)

    @cmd.command(name='show-module-gpio')
    @show_module_options
    def show_module_gpio(**kargs):
        '''
        Display GPIOs of SmartNIC pluggable modules.
        '''
        return batch_module_gpio(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-module-info')
    @show_module_options
    def show_module_info(**kargs):
        '''
        Display information from SmartNIC pluggable modules.
        '''
        return batch_module_info(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-module-mem')
    @show_module_mem_options
    def show_module_mem(**kargs):
        '''
        Display bytes from management memory of SmartNIC pluggable modules.
        '''
        return batch_module_mem(BatchOperation.BOP_GET, **kargs)

    @cmd.command(name='show-module-status')
    @show_module_status_options
    def show_module_status(**kargs):
        '''
        Display status from SmartNIC pluggable modules.
        '''
        return batch_module_status(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_configure_commands(cmd):
    @cmd.group
    @click.pass_context
    def module(ctx, **kargs):
        '''
        Configuration for SmartNIC pluggable modules.
        '''

    @module.command
    @configure_module_gpio_options
    @click.pass_context
    def gpio(ctx, **kargs):
        '''
        Configure GPIOs of SmartNIC pluggable modules.
        '''
        configure_module_gpio(ctx.obj, **kargs)

    @module.command
    @configure_module_mem_options
    @click.pass_context
    def mem(ctx, **kargs):
        '''
        Write bytes to management memory of SmartNIC pluggable modules.
        '''
        configure_module_mem(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def module(ctx):
        '''
        Display for SmartNIC pluggable modules.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1, 'mod_id': -1}
            show_module_info(client, **kargs)
            show_module_status(client, **kargs)

    @module.command
    @show_module_options
    @click.pass_context
    def gpio(ctx, **kargs):
        '''
        Display GPIOs of SmartNIC pluggable modules.
        '''
        show_module_gpio(ctx.obj, **kargs)

    @module.command
    @show_module_options
    @click.pass_context
    def info(ctx, **kargs):
        '''
        Display information of SmartNIC pluggable modules.
        '''
        show_module_info(ctx.obj, **kargs)

    @module.command
    @show_module_mem_options
    @click.pass_context
    def mem(ctx, **kargs):
        '''
        Display bytes from management memory of SmartNIC pluggable modules.
        '''
        show_module_mem(ctx.obj, **kargs)

    @module.command
    @show_module_status_options
    @click.pass_context
    def status(ctx, **kargs):
        '''
        Display status of SmartNIC pluggable modules.
        '''
        show_module_status(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_configure_commands(cmds.configure)
    add_show_commands(cmds.show)
