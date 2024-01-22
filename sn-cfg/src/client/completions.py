#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_commands',
)

import click

#---------------------------------------------------------------------------------------------------
def print_completion(ctx, shell):
    prog = ctx.find_root().info_name
    var = prog.replace('-', '_').upper()

    # https://click.palletsprojects.com/en/8.1.x/shell-completion/#enabling-completion
    import subprocess
    output = subprocess.run(
        (shell, '-c', f'_{var}_COMPLETE={shell}_source {prog}'),
        stdout=subprocess.PIPE,
    )
    click.echo(output.stdout.decode())

#---------------------------------------------------------------------------------------------------
def add_main_commands(cmd):
    @cmd.group
    def completions():
        '''
        Generate shell completion scripts.
        '''

    @completions.command
    @click.pass_context
    def bash(ctx):
        '''
        Generate a completion script for the Bourne Again SHell.
        '''
        print_completion(ctx, 'bash')

    @completions.command
    @click.pass_context
    def fish(ctx):
        '''
        Generate a completion script for the Friendly Interactive SHell.
        '''
        print_completion(ctx, 'fish')

    @completions.command
    @click.pass_context
    def zsh(ctx):
        '''
        Generate a Z SHell completion script.
        '''
        print_completion(ctx, 'zsh')

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_main_commands(cmds.main)
