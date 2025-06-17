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
    Register,
    RegisterId,
    RegisterSlice,
    RegistersRequest,
)

from .device import device_id_option
from .error import error_code_str
from .pipeline import pipeline_id_option
from .utils import apply_options

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
def registers_req(dev_id, pipeline_id, **kargs):
    req_kargs = {
        'dev_id': dev_id,
        'pipeline_id': pipeline_id,
        'clear_on_read': kargs.get('clear_on_read', False),
        'non_zero': not kargs.get('zeroes'),
    }

    block_name = kargs.get('block_name')
    if block_name is not None:
        s = RegisterSlice()
        req_kargs['slices'] = [s]

        alias = kargs.get('alias')
        value = kargs.get('value')
        if value is not None:
            r = s.registers.add()
            r.id.block_name = block_name
            if alias is not None:
                r.id.alias = alias
            else:
                r.id.index = kargs['index']
            r.value = value
        else:
            s.first_id.block_name = block_name
            if alias is not None:
                s.first_id.alias = alias
            else:
                s.first_id.index = kargs['index']
            s.count = kargs['count']

    return RegistersRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_registers(op, **kargs):
    req = registers_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_clear_registers(stub, **kargs):
    for resp in rpc_registers(stub.ClearRegisters, **kargs):
        yield resp.dev_id, resp.pipeline_id

def rpc_get_registers(stub, **kargs):
    for resp in rpc_registers(stub.GetRegisters, **kargs):
        yield resp.dev_id, resp.pipeline_id, resp.slices

def rpc_set_registers(stub, **kargs):
    for resp in rpc_registers(stub.SetRegisters, **kargs):
        yield resp.dev_id, resp.pipeline_id

#---------------------------------------------------------------------------------------------------
def clear_registers(client, **kargs):
    for dev_id, pipeline_id in rpc_clear_registers(client.stub, **kargs):
        click.echo(f'Cleared registers of pipeline ID {pipeline_id} for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def configure_registers(client, **kargs):
    for dev_id, pipeline_id in rpc_set_registers(client.stub, **kargs):
        click.echo(f'Configured registers of pipeline ID {pipeline_id} for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_registers(dev_id, pipeline_id, slices, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Pipeline ID: {pipeline_id} on device ID {dev_id}')

    for s in slices:
        index_len = 0
        value_len = 0
        for r in s.registers:
            index_len = max(index_len, len(str(r.id.index)))
            value_len = max(value_len, len(r.value))
        index_len += 2

        rows.append(HEADER_SEP)
        rows.append(f'Register Block: {s.first_id.block_name}')
        for r in s.registers:
            index = f'[{r.id.index}]'
            row = f'{index:>{index_len}}: {r.value:<{value_len}}'
            if r.id.alias:
                row += f'  <{r.id.alias}>'
            rows.append(row)

    click.echo('\n'.join(rows))

#---------------------------------------------------------------------------------------------------
def show_registers(client, **kargs):
    for dev_id, pipeline_id, slices in rpc_get_registers(client.stub, **kargs):
        _show_registers(dev_id, pipeline_id, slices, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_registers_req(op, **kargs):
    yield BatchRequest(op=op, registers=registers_req(**kargs))

def batch_process_registers_resp(kargs):
    def process(resp):
        if not resp.HasField('registers'):
            return False

        supported_ops = {
            BatchOperation.BOP_CLEAR: 'Cleared',
            BatchOperation.BOP_GET: 'Got',
            BatchOperation.BOP_SET: 'Configured',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.registers
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_registers(resp.dev_id, resp.pipeline_id, resp.slices, kargs)
        else:
            click.echo(f'{op_label} registers of pipeline ID {resp.pipeline_id} '
                       f'for device ID {resp.dev_id}.')
        return True

    return process

def batch_registers(op, **kargs):
    return batch_generate_registers_req(op, **kargs), batch_process_registers_resp(kargs)

#---------------------------------------------------------------------------------------------------
block_name_option = click.option(
    '--block-name', '-b',
    help='Name of the block containing the register(s) being accessed.',
)

index_option = click.option(
    '--index', '-i',
    type=click.INT,
    default=0,
    help='0-based index of the register to operate on.',
)

alias_option = click.option(
    '--alias', '-a',
    help='String name assigned to a specifc register index within the block. When present in the '
         'pipeline, it can be used instead of --index when selecting the register.'
)

count_option = click.option(
    '--count', '-c',
    type=click.INT,
    default=0,
    help='Number of contiguous values to read starting from the index.',
)

def clear_registers_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
        block_name_option,
        index_option,
        alias_option,
        count_option,
    )
    return apply_options(options, fn)

def configure_registers_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
        click.option(
            '--block-name', '-b',
            required=True,
            help='Name of the block containing the register(s) being accessed.',
        ),
        index_option,
        alias_option,
        click.option(
            '--value', '-v',
            required=True,
            help='Value to be written to the register.',
        ),
    )
    return apply_options(options, fn)

def show_registers_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
        block_name_option,
        index_option,
        alias_option,
        count_option,
        click.option(
            '--clear-on-read',
            is_flag=True,
            help='Reset registers to their initial value after reading.',
        ),
        click.option(
            '--zeroes', '-z',
            is_flag=True,
            help='Include zero valued registers in the display.',
        ),
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='clear-registers')
    @clear_registers_options
    def clear_registers(**kargs):
        '''
        Clear SmartNIC P4 registers.
        '''
        return batch_registers(BatchOperation.BOP_CLEAR, **kargs)

    @cmd.command(name='configure-registers')
    @configure_registers_options
    def configure_registers(**kargs):
        '''
        Configure SmartNIC P4 registers.
        '''
        return batch_registers(BatchOperation.BOP_SET, **kargs)

    @cmd.command(name='show-registers')
    @show_registers_options
    def show_registers(**kargs):
        '''
        Display SmartNIC P4 registers.
        '''
        return batch_registers(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_clear_commands(cmd):
    @cmd.command
    @clear_registers_options
    @click.pass_context
    def registers(ctx, **kargs):
        '''
        Clear SmartNIC P4 registers.
        '''
        clear_registers(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_configure_commands(cmd):
    @cmd.command
    @configure_registers_options
    @click.pass_context
    def registers(ctx, **kargs):
        '''
        Configure SmartNIC P4 registers.
        '''
        configure_registers(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.command
    @show_registers_options
    @click.pass_context
    def registers(ctx, **kargs):
        '''
        Display SmartNIC P4 registers.
        '''
        show_registers(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_configure_commands(cmds.configure)
    add_show_commands(cmds.show)
