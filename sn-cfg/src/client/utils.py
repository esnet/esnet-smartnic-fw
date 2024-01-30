#---------------------------------------------------------------------------------------------------
__all__ = (
    'apply_options',
    'ChoiceFields',
)

import click

#---------------------------------------------------------------------------------------------------
def apply_options(options, fn):
    for opt in reversed(options):
        fn = opt(fn)
    return fn

#---------------------------------------------------------------------------------------------------
# Based on implementation of click.Choice from:
# https://github.com/pallets/click/blob/main/src/click/types.py#L230
class ChoiceFields(click.ParamType):
    def __init__(self, sep, *fields):
        self.sep = sep
        self.field_names = tuple(name for name, _, _ in fields)
        self.field_choices = tuple(tuple(sorted(choices)) for _, choices, _ in fields)
        self.field_converts = tuple(convert for _, _, convert in fields)
        self.nfields = len(fields)

    def get_metavar(self, param):
        names = self.sep.join(f'<{name}>' for name in self.field_names)
        choices = self.sep.join('<' + '|'.join(choices) + '>' for choices in self.field_choices)
        metavar = f'{names} = {choices}'

        if param.required and param.param_type_name == 'argument':
            return f'{{{metavar}}}'
        return f'[{metavar}]'

    def get_missing_message(self, param):
        import gettext

        names = self.sep.join(f'<{name}>' for name in self.field_names)
        choices = '\n'.join(
            '\n\t- '.join((f'- Choices for field <{name}>:',) + choices)
            for name, choices in zip(self.field_names, self.field_choices)
        )
        return gettext.gettext(f'Provide a value for each field in {names}:\n{choices}')

    def convert(self, value, param, ctx):
        fields = value.split(self.sep)
        nfields = len(fields)
        if nfields != self.nfields:
            import gettext
            msg = gettext.gettext(
                f'{value!r} must have exactly {self.nfields} fields separated by "{self.sep}". '
                f'The given value has {nfields} fields.')
            self.fail(msg, param, ctx)

        for i, (field, choices) in enumerate(zip(fields, self.field_choices)):
            if field not in choices:
                import gettext
                choices = ' | '.join(choices)
                msg = gettext.gettext(
                    f'Field {i+1}, with value {field!r}, is unknown. Must be one of: {choices}.')
                self.fail(msg, param, ctx)

            convert = self.field_converts[i]
            if convert is not None:
                fields[i] = convert(field, param, ctx)

        return tuple(fields)

    def shell_complete(self, ctx, param, incomplete):
        from click.shell_completion import CompletionItem

        fields = incomplete.split(self.sep)
        nfields = len(fields)
        if nfields > self.nfields:
            return []

        field = fields.pop()
        choices = [
            choice
            for choice in self.field_choices[nfields - 1]
            if choice.startswith(field)
        ]

        def remaining_choices(field_choices):
            if not field_choices:
                yield []
                return

            for choice in field_choices[0]:
                for remaining in remaining_choices(field_choices[1:]):
                    yield [choice] + remaining

        return [
            CompletionItem("'" + self.sep.join(fields + [choice] + remaining) + "'")
            for choice in choices
            for remaining in remaining_choices(self.field_choices[nfields:])
        ]
