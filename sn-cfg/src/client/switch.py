#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import grpc
import re
import types

from .device import device_id_option
from .error import error_code_str
from .sn_cfg_v1_pb2 import (
    BatchOperation,
    BatchRequest,
    ErrorCode,
    SwitchConfig,
    SwitchConfigRequest,
    SwitchInterfaceType,
    SwitchProcessorType,
)
from .utils import apply_options, ChoiceFields

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
INTF_TYPE_MAP = {
    SwitchInterfaceType.SW_INTF_PORT: 'port',
    SwitchInterfaceType.SW_INTF_HOST: 'host',
}
INTF_TYPE_RMAP = dict((name, enum) for enum, name in INTF_TYPE_MAP.items())

# TODO: How to discover these? Query from server? Load from config file?
INTF_TYPE_RANGES = {
    'port': range(2),
    'host': range(2),
}

INTF_CHOICES = tuple(sorted(
    f'{ty}{idx}'
    for ty in INTF_TYPE_RMAP
    for idx in INTF_TYPE_RANGES[ty]
))

def intf_id_to_name(intf):
    name = INTF_TYPE_MAP[intf.itype]
    if isinstance(INTF_TYPE_RANGES[name], range):
        name += str(intf.index)
    return name

#---------------------------------------------------------------------------------------------------
PROC_TYPE_MAP = {
    SwitchProcessorType.SW_PROC_BYPASS: 'bypass',
    SwitchProcessorType.SW_PROC_DROP: 'drop',
    SwitchProcessorType.SW_PROC_APP: 'app',
}
PROC_TYPE_RMAP = dict((name, enum) for enum, name in PROC_TYPE_MAP.items())

# TODO: How to discover these? Query from server? Load from config file?
PROC_TYPE_RANGES = {
    'bypass': ('',),
    'drop': ('',),
    'app': range(2),
}

TO_PROC_CHOICES = tuple(sorted(
    f'{ty}{idx}'
    for ty in PROC_TYPE_RMAP
    for idx in PROC_TYPE_RANGES[ty]
))

ON_PROC_CHOICES = tuple(filter(lambda p: not p.startswith('drop'), TO_PROC_CHOICES))

def proc_id_to_name(proc):
    name = PROC_TYPE_MAP[proc.ptype]
    if isinstance(PROC_TYPE_RANGES[name], range):
        name += str(proc.index)
    return name

#---------------------------------------------------------------------------------------------------
CHOICE_FIELD_RE = re.compile(r'^(?P<type>.+?)(?P<index>\d*)$')
def choice_field_convert(value, param, ctx):
    match = CHOICE_FIELD_RE.match(value)
    index = match['index']
    index = int(index) if index else 0
    return types.SimpleNamespace(type=match['type'], index=index)

#---------------------------------------------------------------------------------------------------
def switch_config_req(dev_id, **config_kargs):
    req_kargs = {'dev_id': dev_id}

    if config_kargs:
        config = SwitchConfig()
        req_kargs['config'] = config

        # Attach an ingress source mapping for each pair.
        for fi, ti in config_kargs.get('ingress_source', ()):
            src = config.ingress_sources.add()
            src.from_intf.itype = INTF_TYPE_RMAP[fi.type]
            src.from_intf.index = fi.index
            src.to_intf.itype = INTF_TYPE_RMAP[ti.type]
            src.to_intf.index = ti.index

        # Attach an ingress connection mapping for each pair.
        for fi, tp in config_kargs.get('ingress_conn', ()):
            conn = config.ingress_connections.add()
            conn.from_intf.itype = INTF_TYPE_RMAP[fi.type]
            conn.from_intf.index = fi.index
            conn.to_proc.ptype = PROC_TYPE_RMAP[tp.type]
            conn.to_proc.index = tp.index

        # Attach an egress connection mapping for each triplet.
        for op, fi, ti in config_kargs.get('egress_conn', ()):
            conn = config.egress_connections.add()
            conn.on_proc.ptype = PROC_TYPE_RMAP[op.type]
            conn.on_proc.index = op.index
            conn.from_intf.itype = INTF_TYPE_RMAP[fi.type]
            conn.from_intf.index = fi.index
            conn.to_intf.itype = INTF_TYPE_RMAP[ti.type]
            conn.to_intf.index = ti.index

    return SwitchConfigRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_switch_config(op, **kargs):
    req = switch_config_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_switch_config(stub, **kargs):
    for resp in rpc_switch_config(stub.GetSwitchConfig, **kargs):
        yield resp.dev_id, resp.config

def rpc_set_switch_config(stub, **kargs):
    for resp in rpc_switch_config(stub.SetSwitchConfig, **kargs):
        yield resp.dev_id

#---------------------------------------------------------------------------------------------------
def configure_switch(client, **kargs):
    for dev_id in rpc_set_switch_config(client.stub, **kargs):
        click.echo(f'Configured switch for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_switch_config(dev_id, config):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    rows.append('Ingress Sources:')
    for src in config.ingress_sources:
        from_intf = intf_id_to_name(src.from_intf)
        to_intf = intf_id_to_name(src.to_intf)
        rows.append(f'    {from_intf} --> {to_intf}')

    rows.append('Ingress Connections:')
    for conn in config.ingress_connections:
        from_intf = intf_id_to_name(conn.from_intf)
        to_proc = proc_id_to_name(conn.to_proc)
        rows.append(f'    {from_intf} --> {to_proc}')

    rows.append('Egress Connections:')
    for conn in config.egress_connections:
        on_proc = proc_id_to_name(conn.on_proc)
        from_intf = intf_id_to_name(conn.from_intf)
        to_intf = intf_id_to_name(conn.to_intf)
        rows.append(f'    {on_proc}: {from_intf} --> {to_intf}')

    click.echo('\n'.join(rows))

def show_switch_config(client, **kargs):
    for dev_id, config in rpc_get_switch_config(client.stub, **kargs):
        _show_switch_config(dev_id, config)

#---------------------------------------------------------------------------------------------------
def batch_generate_switch_config_req(op, **kargs):
    yield BatchRequest(op=op, switch_config=switch_config_req(**kargs))

def batch_process_switch_config_resp(resp):
    if not resp.HasField('switch_config'):
        return False

    resp = resp.switch_config
    if resp.error_code != ErrorCode.EC_OK:
        raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

    if resp.HasField('config'):
        _show_switch_config(resp.dev_id, resp.config)
    else:
        click.echo(f'Configured switch for device ID {resp.dev_id}.')
    return True

def batch_switch_config(op, **kargs):
    return batch_generate_switch_config_req(op, **kargs), batch_process_switch_config_resp

#---------------------------------------------------------------------------------------------------
def configure_switch_options(fn):
    options = (
        device_id_option,
        click.option(
            '--ingress-source', '-s',
            type=ChoiceFields(
                ':',
                ('from-intf', INTF_CHOICES, choice_field_convert),
                ('to-intf', INTF_CHOICES, choice_field_convert),
            ),
            multiple=True,
            help='''
            A pair of ingress interface names, of the form <from-intf>:<to-intf>, where the actual
            ingress <from-intf> interface is re-labelled to appear as if ingress occurred on the
            <to-intf> interface.
            ''',
        ),
        click.option(
            '--ingress-conn', '-i',
            type=ChoiceFields(
                ':',
                ('from-intf', INTF_CHOICES, choice_field_convert),
                ('to-proc', TO_PROC_CHOICES, choice_field_convert),
            ),
            multiple=True,
            help='''
            A pair of ingress interface and processor names, of the form <from-intf>:<to-proc>,
            where the ingress <from-intf> interface is connected to the <to-proc> P4 processor.
            ''',
        ),
        click.option(
            '--egress-conn', '-e',
            type=ChoiceFields(
                ':',
                ('on-proc', ON_PROC_CHOICES, choice_field_convert),
                ('from-intf', INTF_CHOICES, choice_field_convert),
                ('to-intf', INTF_CHOICES, choice_field_convert),
            ),
            multiple=True,
            help='''
            A triplet of processor, ingress and egress interface names, of the form
            <on-proc>:<from-intf>:<to-intf>, where traffic that ingressed on the <from-intf>
            interface and processed by the <on-proc> P4 processor should egress the <to-intf>
            interface.
            ''',
        ),
    )
    return apply_options(options, fn)

def show_switch_options(fn):
    options = (
        device_id_option,
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='configure-switch')
    @configure_switch_options
    def configure_switch(**kargs):
        '''
        Configuration for the SmartNIC packet switch.
        '''
        return batch_switch_config(BatchOperation.BOP_SET, **kargs)

    @cmd.command(name='show-switch-config')
    @show_switch_options
    def show_switch_config(**kargs):
        '''
        Display the configuration of the SmartNIC packet switch.
        '''
        return batch_switch_config(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_configure_commands(cmd):
    @cmd.command
    @configure_switch_options
    @click.pass_context
    def switch(ctx, **kargs):
        '''
        Configuration for the SmartNIC packet switch.
        '''
        configure_switch(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd):
    @cmd.group(invoke_without_command=True)
    @click.pass_context
    def switch(ctx):
        '''
        Display for the SmartNIC packet switch.
        '''
        if ctx.invoked_subcommand is None:
            client = ctx.obj
            kargs = {'dev_id': -1}
            show_switch_config(client, **kargs)

    @switch.command
    @device_id_option
    @click.pass_context
    def config(ctx, **kargs):
        '''
        Display the configuration of the SmartNIC packet switch.
        '''
        show_switch_config(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_configure_commands(cmds.configure)
    add_show_commands(cmds.show)
