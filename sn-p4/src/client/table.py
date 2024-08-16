#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import grpc
import re

from sn_p4_proto.v2 import (
    BatchOperation,
    BatchRequest,
    ErrorCode,
    TableRequest,
    TableRule,
    TableRuleRequest,
)

from .device import device_id_option
from .error import error_code_str
from .pipeline import pipeline_id_option
from .utils import apply_options, MIXED_INT, StrList

#---------------------------------------------------------------------------------------------------
def table_req(dev_id, pipeline_id, table_name, **kargs):
    return TableRequest(dev_id=dev_id, pipeline_id=pipeline_id, table_name=table_name)

#---------------------------------------------------------------------------------------------------
def rpc_table(op, **kargs):
    req = table_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_clear_table(stub, **kargs):
    for resp in rpc_table(stub.ClearTable, **kargs):
        yield resp.dev_id, resp.pipeline_id

#---------------------------------------------------------------------------------------------------
def clear_table(client, **kargs):
    table_name = kargs['table_name']
    table_label = f'table "{table_name}"' if table_name else 'all tables'
    for dev_id, pipeline_id in rpc_clear_table(client.stub, **kargs):
        click.echo(f'Cleared {table_label} of pipeline ID {pipeline_id} for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def batch_generate_table_req(op, **kargs):
    yield BatchRequest(op=op, table=table_req(**kargs))

def batch_process_table_resp(kargs):
    def process(resp):
        if not resp.HasField('table'):
            return False

        supported_ops = {
            BatchOperation.BOP_CLEAR: 'Cleared',
        }
        if resp.op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {resp.op}')
        op_label = supported_ops[resp.op]

        resp = resp.table
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        table_name = kargs['table_name']
        table_label = f'table "{table_name}"' if table_name else 'all tables'
        click.echo(f'{op_label} {table_label} of pipeline ID {resp.pipeline_id} '
                   f'for device ID {resp.dev_id}.')
        return True

    return process

def batch_table(op, **kargs):
    return batch_generate_table_req(op, **kargs), batch_process_table_resp(kargs)

#---------------------------------------------------------------------------------------------------
# Restrict parsing to formatting of mpz_set_str (https://gmplib.org/manual/Assigning-Integers)
# with base 0 (0x/0X for hexadecimal, 0b/0B for binary, 0 for octal, or decimal otherwise).
MPZ_RE = r'(0[xX][0-9a-fA-F]+|0[bB][01]+|0[0-7]+|\d+)'
MATCH_KEY_ONLY_RE = re.compile(r'^(?P<key>' + MPZ_RE + ')$')
MATCH_KEY_MASK_RE = re.compile(r'^(?P<key>' + MPZ_RE + ')&&&(?P<mask>' + MPZ_RE + ')$')
MATCH_KEY_PREFIX_RE = re.compile(r'^(?P<key>' + MPZ_RE + ')/(?P<prefix_length>\d+)$')
MATCH_RANGE_RE = re.compile(r'^(?P<lower>\d+)->(?P<upper>\d+)$')
ACTION_PARAM_RE = re.compile(r'^' + MPZ_RE + r'$')

def table_rule_req(dev_id, pipeline_id, table_name, **kargs):
    rule = TableRule(table_name=table_name)

    priority = kargs.get('priority', None)
    if priority is not None:
        rule.priority = priority

    replace = kargs.get('replace', None)
    if replace is not None:
        rule.replace = replace

    for m in kargs.get('matches'):
        match = rule.matches.add()

        if m == '.':
            match.unused = True
            continue

        rm = MATCH_KEY_ONLY_RE.match(m)
        if rm is not None:
            match.key_only.key = rm['key']
            continue

        rm = MATCH_KEY_MASK_RE.match(m)
        if rm is not None:
            match.key_mask.key = rm['key']
            match.key_mask.mask = rm['mask']
            continue

        rm = MATCH_KEY_PREFIX_RE.match(m)
        if rm is not None:
            match.key_prefix.key = rm['key']
            match.key_prefix.prefix_length = int(rm['prefix_length'])
            continue

        rm = MATCH_RANGE_RE.match(m)
        if rm is not None:
            match.range.lower = int(rm['lower'])
            match.range.upper = int(rm['upper'])
            continue

        raise click.ClickException(f'Unknown table rule match format: "{m}"')

    action_name = kargs.get('action_name', None)
    if action_name is not None:
        rule.action.name = action_name

        action_params = kargs.get('action_params', None)
        if action_params is not None:
            for p in action_params:
                rm = ACTION_PARAM_RE.match(p)
                if rm is None:
                    raise click.ClickException(f'Unknown action parameter format: "{p}"')

                param = rule.action.parameters.add()
                param.value = p

    return TableRuleRequest(dev_id=dev_id, pipeline_id=pipeline_id, rules=[rule])

#---------------------------------------------------------------------------------------------------
def rpc_table_rule(op, **kargs):
    req = table_rule_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_delete_table_rule(stub, **kargs):
    for resp in rpc_table_rule(stub.DeleteTableRule, **kargs):
        yield resp.dev_id, resp.pipeline_id

def rpc_insert_table_rule(stub, **kargs):
    for resp in rpc_table_rule(stub.InsertTableRule, **kargs):
        yield resp.dev_id, resp.pipeline_id

#---------------------------------------------------------------------------------------------------
def delete_table_rule(client, **kargs):
    table_name = kargs['table_name']
    for dev_id, pipeline_id in rpc_delete_table_rule(client.stub, **kargs):
        click.echo(f'Deleted rule from table "{table_name}" of pipeline ID {pipeline_id} '
                   f'for device ID {dev_id}.')

def insert_table_rule(client, **kargs):
    table_name = kargs['table_name']
    for dev_id, pipeline_id in rpc_insert_table_rule(client.stub, **kargs):
        click.echo(f'Inserted rule into table "{table_name}" of pipeline ID {pipeline_id} '
                   f'for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def batch_generate_table_rule_req(op, **kargs):
    yield BatchRequest(op=op, table_rule=table_rule_req(**kargs))

def batch_process_table_rule_resp(kargs):
    def process(resp):
        if not resp.HasField('table_rule'):
            return False

        supported_ops = {
            BatchOperation.BOP_INSERT: 'Inserted rule into',
            BatchOperation.BOP_DELETE: 'Deleted rule from',
        }
        if resp.op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {resp.op}')
        op_label = supported_ops[resp.op]

        resp = resp.table_rule
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        table_name = kargs['table_name']
        click.echo(f'{op_label} table "{table_name}" of pipeline ID {resp.pipeline_id} '
                   f'for device ID {resp.dev_id}.')
        return True

    return process

def batch_table_rule(op, **kargs):
    return batch_generate_table_rule_req(op, **kargs), batch_process_table_rule_resp(kargs)

#---------------------------------------------------------------------------------------------------
table_name_option = click.option(
    '--table', '-t',
    'table_name',
    required=True,
    help='Name of the table to operate on.',
)

table_rule_match_option = click.option(
    '--match', '-m',
    'matches',
    type=StrList(' '),
    required=True,
    help='''
    Space separated list of one or more match specifications for defining the rule key.
    ''',
)

def clear_table_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
        click.option(
            '--table', '-t',
            'table_name',
            default='',
            help='Name of the table to operate on. Default is to operate on all tables.',
        ),
    )
    return apply_options(options, fn)

def delete_table_rule_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
        table_name_option,
        table_rule_match_option,
    )
    return apply_options(options, fn)

def insert_table_rule_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
        table_name_option,
        table_rule_match_option,
        click.option(
            '--priority',
            type=MIXED_INT,
            help='Priority of the rule used for resolving multiple matches in the table.',
        ),
        click.option(
            '--replace',
            is_flag=True,
            help='Replace the rule if it already exists in the table.',
        ),
        click.option(
            '--action',
            'action_name',
            required=True,
            help='Name of the action to invoke when the rule is matched.',
        ),
        click.option(
            '--param',
            'action_params',
            type=StrList(' '),
            help='Space separated list of one or more values for action parameters.',
        ),
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='clear-table')
    @clear_table_options
    def clear_table(**kargs):
        '''
        Clear SmartNIC P4 tables.
        '''
        return batch_table(BatchOperation.BOP_CLEAR, **kargs)

    @cmd.command(name='delete-table-rule')
    @delete_table_rule_options
    def delete_table_rule(**kargs):
        '''
        Delete a rule from a SmartNIC P4 table.
        '''
        return batch_table_rule(BatchOperation.BOP_DELETE, **kargs)

    @cmd.command(name='insert-table-rule')
    @insert_table_rule_options
    def insert_table_rule(**kargs):
        '''
        Insert a rule into a SmartNIC P4 table.
        '''
        return batch_table_rule(BatchOperation.BOP_INSERT, **kargs)

#---------------------------------------------------------------------------------------------------
def add_clear_commands(cmd):
    @cmd.command
    @clear_table_options
    @click.pass_context
    def table(ctx, **kargs):
        '''
        Clear SmartNIC P4 tables.
        '''
        clear_table(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_delete_commands(cmd):
    @cmd.group
    @click.pass_context
    def table(ctx):
        '''
        Delete entries from SmartNIC P4 tables.
        '''

    @table.command
    @delete_table_rule_options
    @click.pass_context
    def rule(ctx, **kargs):
        '''
        Delete a rule from a SmartNIC P4 table.
        '''
        delete_table_rule(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_insert_commands(cmd):
    @cmd.group
    @click.pass_context
    def table(ctx):
        '''
        Insert entries into SmartNIC P4 tables.
        '''

    @table.command
    @insert_table_rule_options
    @click.pass_context
    def rule(ctx, **kargs):
        '''
        Insert a rule into a SmartNIC P4 table.
        '''
        insert_table_rule(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_delete_commands(cmds.delete)
    add_insert_commands(cmds.insert)
