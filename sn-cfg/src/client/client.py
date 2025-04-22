#---------------------------------------------------------------------------------------------------
__all__ = (
    'connect_client',
    'main',
)

import click
import grpc
import json
import os
import pathlib
import types

from sn_cfg_proto import ErrorCode, SmartnicConfigStub

from .error import error_code_str
from . import completions
from . import defaults
from . import device
from . import host
from . import module
from . import port
from . import server
from . import stats
from . import switch

SUB_COMMAND_MODULES = (
    completions,
    defaults,
    device,
    host,
    module,
    port,
    server,
    stats,
    switch,
)

#---------------------------------------------------------------------------------------------------
HELP_CONFIG_TLS_ROOT_CERTS = '{"client":{"tls":{"root-certs":"<PATH-TO-PEM-FILE>"}}}'
HELP_CONFIG_AUTH_TOKEN     = '{"client":{"auth":{"token":"<TOKEN>"}}}'

DEFAULT_ADDRESS = 'ip6-localhost'
DEFAULT_PORT = 50100

#---------------------------------------------------------------------------------------------------
def connect_client(address=None, port=None, auth_token=None,
                   tls_root_certs=None, tls_hostname_override=None, tls_insecure=False):
    if address is None:
        address = os.environ.get('SN_CFG_CLI_ADDRESS', DEFAULT_ADDRESS)
    if port is None:
        port = os.environ.get('SN_CFG_CLI_PORT', DEFAULT_PORT)
    if auth_token is None:
        auth_token = os.environ.get('SN_CFG_CLI_AUTH_TOKEN')
    if tls_insecure:
        # The current implementation of the Python SSLChannelCredentials (implemented by a cython
        # wrapper around the gRPC core C library) does not expose the peer verification options from
        # the grpc_ssl_credentials_create function, which would have allowed ignoring the server's
        # certificate by simply providing a custom verification callback. Since the cython interface
        # isn't compatible with ctypes, the grpc_ssl_credentials_create function can't be called
        # directly. This is worked around by simply fetching the server's certificate and using it
        # as the CA chain.
        import ssl
        server_certs = ssl.get_server_certificate((address, port))
        tls_root_certs = bytes(server_certs, 'utf-8')
    if tls_root_certs is None:
        tls_root_certs = os.environ.get('SN_CFG_CLI_TLS_ROOT_CERTS')
    if tls_root_certs is not None and not isinstance(tls_root_certs, bytes):
        tls_root_certs = pathlib.Path(tls_root_certs).read_bytes()
    if tls_hostname_override is None:
        tls_hostname_override = os.environ.get('SN_CFG_CLI_TLS_HOSTNAME_OVERRIDE')

    # Setup channel option to override the hostname used during verification.
    # https://grpc.github.io/grpc/core/group__grpc__arg__keys.html#ga218bf55b665134a11baf07ada5980825
    options = ()
    if tls_hostname_override is not None:
        options += (('grpc.ssl_target_name_override', tls_hostname_override,),)

    # Setup the credentials and authentication methods.
    tls_creds = grpc.ssl_channel_credentials(root_certificates=tls_root_certs)
    call_creds = grpc.access_token_call_credentials(auth_token)
    channel_creds = grpc.composite_channel_credentials(tls_creds, call_creds)

    # Setup the TLS encrypted channel.
    channel = grpc.secure_channel(f'{address}:{port}', channel_creds, options)

    # Create the RPC proxy.
    return SmartnicConfigStub(channel)

#---------------------------------------------------------------------------------------------------
def __connect_client(client):
    config = {}
    if not client.args.no_config_file:
        config_file = pathlib.Path(client.args.config_file)
        if config_file.exists():
            with config_file.open() as fo:
                config = json.load(fo)

    config = config.get('client', {})
    config_auth = config.get('auth', {})
    config_tls = config.get('tls', {})

    # Setup the authentication token.
    auth_token = client.args.auth_token
    if auth_token is None:
        auth_token = config_auth.get('token')
    if auth_token is None:
        raise click.ClickException(
            'Missing token needed for authenticating with the server. Specify the token to use '
            'with the --auth-token option, via environment variable or add to the config file '
            f'as {HELP_CONFIG_AUTH_TOKEN}.')

    # Setup the root certificates needed for verifying the server.
    tls_root_certs = client.args.tls_root_certs
    if not client.args.tls_insecure:
        if tls_root_certs is None:
            tls_root_certs = config_tls.get('root-certs')
        if tls_root_certs is not None:
            tls_root_certs = pathlib.Path(tls_root_certs).read_bytes()

    # Create the RPC proxy.
    client.stub = connect_client(
        client.args.address, client.args.port, auth_token,
        tls_root_certs, client.args.tls_hostname_override, client.args.tls_insecure)

#---------------------------------------------------------------------------------------------------
@click.group(
    context_settings={
        'help_option_names': ('--help', '-h'),
    },
)
@click.option(
    '--address',
    default=DEFAULT_ADDRESS,
    show_default=True,
    show_envvar=True,
    help='Address of the server to connect to. Can also be set via environment variable.',
)
@click.option(
    '--port',
    type=click.INT,
    default=DEFAULT_PORT,
    show_default=True,
    show_envvar=True,
    help='Port the server listens on. Can also be set via environment variable.',
)
@click.option(
    '--tls-insecure',
    is_flag=True,
    help='Disable verification of the server certificate during TLS handshake.',
)
@click.option(
    '--tls-root-certs',
    show_envvar=True,
    help=f'''
    Path to a file containing the concatenation of all X.509 root certificates needed for
    authenticating the server when in secure mode. Can also be set via environment variable. When
    not specified, value is taken from config file as {HELP_CONFIG_TLS_ROOT_CERTS} or defaulted to
    the system standard certificates.
    ''',
)
@click.option(
    '--tls-hostname-override',
    show_envvar=True,
    help='''
    Override the hostname of the server used for verification during TLS handshake. Can also be set
    via environment variable.
    ''',
)
@click.option(
    '--auth-token',
    show_envvar=True,
    help=f'''
    Token to use for authenticating remote procedure calls sent to the server. Can also be set via
    environment variable. Default taken from config file as {HELP_CONFIG_AUTH_TOKEN}.
    '''
)
@click.option(
    '--no-config-file',
    is_flag=True,
    help='Disable support for loading client configuration from a file.',
)
@click.option(
    '--config-file',
    type=click.Path(dir_okay=False),
    default='sn-cfg.json',
    show_default=True,
    help='Client JSON configuration file.',
)
@click.pass_context
def click_main(ctx, **kargs):
    ctx.obj = types.SimpleNamespace(args=types.SimpleNamespace(**kargs))

@click_main.group(chain=True)
def batch(): ...

@batch.result_callback()
@click.pass_context
def batch_callback(ctx, results):
    # The "results" sequence contains the return values from each command handler in the order they
    # were invoked. Each value is a pair of the form (request-generator, response-processor) for
    # each of the sub-commands parsed from the command line.
    client = ctx.obj

    def generate():
        for gen, _ in results:
            yield from gen

    def process():
        for _, proc in results:
            yield proc

    __connect_client(client)
    try:
        for resp in client.stub.Batch(generate()):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

            for proc in process():
                if proc(resp):
                    break
            else:
                raise click.ClickException('Unhandled batch item: ' + str(resp.WhichOneof('item')))
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

@click_main.group
@click.pass_context
def clear(ctx):
    __connect_client(ctx.obj)

@click_main.group
@click.pass_context
def configure(ctx):
    __connect_client(ctx.obj)

@click_main.group
@click.pass_context
def show(ctx):
    __connect_client(ctx.obj)

#---------------------------------------------------------------------------------------------------
def main():
    class Commands: ...
    cmds = Commands()
    cmds.settings = {'prog_name': 'sn-cfg'}
    cmds.main = click_main
    cmds.batch = batch
    cmds.clear = clear
    cmds.configure = configure
    cmds.show = show

    for mod in SUB_COMMAND_MODULES:
        mod.add_sub_commands(cmds)

    cmds.main(
        prog_name=cmds.settings['prog_name'],
        auto_envvar_prefix='SN_CFG_CLI',
    )
