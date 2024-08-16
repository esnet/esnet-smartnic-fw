#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import re

from .device import device_id_option
from .pipeline import pipeline_id_option, rpc_get_pipeline_info
from .table import rpc_clear_table, rpc_insert_table_rule
from .utils import apply_options

#---------------------------------------------------------------------------------------------------
class P4bmOp:
    def __init__(self, args, lineno):
        self.lineno = lineno
        self.kargs = self.parse_init(args)

    def parse_init(self, args):
        return {}

    def parse_finalize(self, info):
        return {}

    def apply(self, client, dev_id, pipeline_id, info):
        kargs = self.kargs.copy()
        kargs.update(self.parse_finalize(info))
        kargs.update({
            'dev_id': dev_id,
            'pipeline_id': pipeline_id,
        })

        # Note: Use type(self) to access the RPC class attribute as a regular function instead of
        #       the default which assumes RPC is a method of the class.
        for _ in type(self).RPC(client.stub, **kargs):
            ...

class P4bmClearAll(P4bmOp):
    NAME = 'clear_all'
    HELP = NAME
    RPC = rpc_clear_table

    def parse_init(self, args):
        if args:
            raise click.ClickException(
                f'Unexpected arguments passed to P4BM operation "{self.NAME}" '
                f'on line {self.lineno}: "{args}".')
        return {'table_name': ''}

class P4bmTableAdd(P4bmOp):
    NAME = 'table_add'
    HELP = f'{NAME} <table_name> <action_name> ' \
            '<match0>[ <match1>...] => <param0>[ <param1>...] [priority]'
    RPC = rpc_insert_table_rule

    ARGS_RE = re.compile(
        '^(?P<table>\w+)\s+(?P<action>\w+)\s+(?P<matches>.+)\s+=>\s+(?P<params>.*)$')

    def parse_init(self, args):
        rm = self.ARGS_RE.match(args)
        if rm is None:
            raise click.ClickException(
                f'Invalid argument formatting to P4BM operation "{self.NAME}" '
                f'on line {self.lineno}: "{args}". Expected format is: "{self.HELP}".')

        # Finalize the action parameters and optional priority once the pipeline info is known.
        self.table_name = rm['table']
        self.matches = re.split(r'\s+', rm['matches'].strip())
        self.action_name = rm['action']
        self.action_params = re.split(r'\s+', rm['params'].strip())

        return {
            'table_name': self.table_name,
            'action_name': self.action_name,
            'matches': self.matches,
        }

    def parse_finalize(self, info):
        for table in info.tables:
            if table.name == self.table_name:
                break
        else:
            raise click.ClickException(
                f'Unknown table name "{self.table_name}" referenced by P4BM operation '
                f'"{self.NAME}" on line {self.lineno}.')

        given = len(self.matches)
        expected = len(table.matches)
        if given != expected:
            raise click.ClickException(
                f'Mismatched number of matches for table "{self.table_name}" referenced by P4BM '
                f'operation "{self.NAME}" on line {self.lineno}. Expected {expected} matches, but '
                f'was given {given}.')

        for action in table.actions:
            if action.name == self.action_name:
                break
        else:
            raise click.ClickException(
                f'Unknown action name "{self.action_name}" for table "{self.table_name}" '
                f'referenced by P4BM operation "{self.NAME}" on line {self.lineno}.')

        kargs = {}
        given = len(self.action_params)
        expected = len(action.parameters)

        # If an extra parameter was given, use it as the priority.
        action_params = list(self.action_params)
        if given > expected:
            kargs['priority'] = int(action_params.pop())
            given -= 1

        if given != expected:
            raise click.ClickException(
                f'Mismatched number of action parameters for action "{self.action_name}" in '
                f'table "{self.table_name}" referenced by P4BM operation "{self.NAME}" '
                f'on line {self.lineno}. Expected {expected} parameters, but was given {given}.')
        kargs['action_params'] = action_params

        return kargs

class P4bmTableClear(P4bmOp):
    NAME = 'table_clear'
    HELP = f'{NAME} <table_name>'
    RPC = rpc_clear_table

    def parse_init(self, args):
        op_args = re.split(r'\s+', args.strip())
        if not op_args:
            raise click.ClickException(
                f'Missing required table_name argument to P4BM operation "{self.NAME}" '
                f'on line {self.lineno}. Expected format is: "{self.HELP}".')

        if len(op_args) > 1:
            extra_args = ' '.join(op_args[1:])
            raise click.ClickException(
                f'Unknown extra arguments passed to P4BM operation "{self.NAME}" '
                f'on line {self.lineno}: "{extra_args}". Expected format is: "{self.HELP}".')

        return {'table_name': op_args[0]}

P4BM_OPS = {
    'clear_all': P4bmClearAll,
    'table_add': P4bmTableAdd,
    'table_clear': P4bmTableClear,
}

#---------------------------------------------------------------------------------------------------
P4BM_LINE_RE = re.compile(r'^(?P<cmd>[^#]+)(\s*#.*)?$')
P4BM_COMMAND_RE = re.compile(r'^(?P<op>\w+)\s*(?P<args>.*)$')

def parse_p4bm(p4bm_file):
    ops = []
    lineno = 0
    for line in p4bm_file:
        lineno += 1

        # Skip blank and comment lines.
        line = line.strip()
        if not line or line[0] == '#':
            continue

        # Extract the command from the line.
        rm = P4BM_LINE_RE.match(line)
        if rm is None:
            click.echo(f'Skipping unknown P4BM formatting on line {lineno}: "{line}".')
            continue

        # Extract the command's operation and arguments.
        rm = P4BM_COMMAND_RE.match(rm['cmd'])
        if rm is None:
            click.echo(f'Skipping unknown P4BM command formatting on line {lineno}: "{line}".')
            continue

        # Lookup the operation's class.
        op_cls = P4BM_OPS.get(rm['op'])
        if op_cls is None:
            click.echo(f'Skipping unknown P4BM operation "{rm["op"]}" on line {lineno}.')
            continue

        ops.append(op_cls(rm['args'], lineno))

    return ops

def apply_p4bm(client, dev_id, pipeline_id, p4bm_file, **kargs):
    ops = parse_p4bm(p4bm_file)

    info_kargs = {'dev_id': dev_id, 'pipeline_id': pipeline_id}
    for dev_id, pipeline_id, info in rpc_get_pipeline_info(client.stub, **info_kargs):
        click.echo(f'Applying P4 behaviour model commands to pipeline ID {pipeline_id} '
                   f'on device ID {dev_id}.')
        for op in ops:
            op.apply(client, dev_id, pipeline_id, info)

#---------------------------------------------------------------------------------------------------
def apply_p4bm_options(fn):
    options = (
        device_id_option,
        pipeline_id_option,
        click.argument(
            'p4bm-file',
            type=click.File('r'),
        ),
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_apply_commands(cmd):
    @cmd.command
    @apply_p4bm_options
    @click.pass_context
    def p4bm(ctx, **kargs):
        '''
        Apply table rules described in a Xilinx P4 behaviour model file.
        '''
        apply_p4bm(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_apply_commands(cmds.apply)
