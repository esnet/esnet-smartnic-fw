#---------------------------------------------------------------------------------------------------
__all__ = (
    'apply_options',
    'format_timestamp',
    'MIXED_INT',
    'natural_sort_key',
    'StrList',
)

import click
import gettext
import re

#---------------------------------------------------------------------------------------------------
def natural_sort_key(item):
    return tuple((int(d) if d.isdigit() else d) for d in re.split(r'(\d+)', item))

#---------------------------------------------------------------------------------------------------
def format_timestamp(ts):
    return f'{ts.seconds}s.{ts.nanos}ns'

#---------------------------------------------------------------------------------------------------
def apply_options(options, fn):
    for opt in reversed(options):
        fn = opt(fn)
    return fn

#---------------------------------------------------------------------------------------------------
INT_PREFIXES = {
    '0b': 2,
    '0o': 8,
    '0x': 16,
}

class MixedInt(click.ParamType):
    name = 'mixed_int' # Needed for auto-generated help (or implement get_metavar method instead).

    def convert(self, value, param, ctx):
        if isinstance(value, int):
            return value

        if not isinstance(value, str):
            msg = gettext.gettext(f'"{value}" is not a known integer format.')
            self.fail(msg, param, ctx)

        try:
            return int(value)
        except ValueError:
            pass

        prefix = value[:2]
        base = INT_PREFIXES.get(prefix.lower())
        if base is None:
            msg = gettext.gettext(
                f'Unable to determine the numerical base for prefix "{prefix}" needed to convert '
                f'"{value}" to an integer representation.')
            self.fail(msg, param, ctx)

        try:
            return int(value[2:], base)
        except ValueError:
            msg = gettext.gettext(f'Unable to convert "{value}" as a base {base} integer.')
            self.fail(msg, param, ctx)

# Since there are no arguments, instantiate a singleton for the parameter type.
MIXED_INT = MixedInt()

#---------------------------------------------------------------------------------------------------
class StrList(click.ParamType):
    name = 'str_list' # Needed for auto-generated help (or implement get_metavar method instead).

    def __init__(self, sep):
        self.sep = sep

    def convert(self, value, param, ctx):
        values = value.strip().split(self.sep)
        if len(values) == 1 and values[0] == '':
            return []
        return values
