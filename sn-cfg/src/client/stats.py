#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
)

import click
import collections
import gettext
import grpc

from sn_cfg_proto import (
    BatchOperation,
    BatchRequest,
    ErrorCode,
    StatsFilters,
    StatsMetricFilter,
    StatsMetricMatch,
    StatsMetricMatchIndexSlice,
    StatsMetricMatchIndices,
    StatsMetricMatchLabel,
    StatsMetricMatchString,
    StatsMetricType,
    StatsRequest,
    StringRegexp,
)

from .device import device_id_option
from .error import error_code_str
from .utils import apply_options, format_timestamp, natural_sort_key

HEADER_SEP = '-' * 40

#---------------------------------------------------------------------------------------------------
METRIC_TYPE_MAP = {
    StatsMetricType.STATS_METRIC_TYPE_COUNTER: 'counter',
    StatsMetricType.STATS_METRIC_TYPE_GAUGE: 'gauge',
    StatsMetricType.STATS_METRIC_TYPE_FLAG: 'flag',
}
METRIC_TYPE_RMAP = dict((name, enum) for enum, name in METRIC_TYPE_MAP.items())

#---------------------------------------------------------------------------------------------------
def stats_req(dev_id, **stats_kargs):
    req_kargs = {'dev_id': dev_id}
    if stats_kargs:
        root = StatsMetricFilter()

        filters = stats_kargs.get('filters')
        if filters:
            root.all_set.members.extend(filters)

        metric_types = stats_kargs.get('metric_types')
        if metric_types:
            type_filters = root.all_set.members.add()
            for mt in metric_types:
                tf = type_filters.any_set.members.add()
                tf.match.type = METRIC_TYPE_RMAP[mt]

        req_kargs['filters'] = StatsFilters(
            non_zero=not stats_kargs.get('zeroes'),
            metric_filter=root,
        )

    return StatsRequest(**req_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_stats(op, **kargs):
    req = stats_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_clear_stats(stub, **kargs):
    for resp in rpc_stats(stub.ClearStats, **kargs):
        yield resp.dev_id

def rpc_get_stats(stub, **kargs):
    for resp in rpc_stats(stub.GetStats, **kargs):
        yield resp.dev_id, resp.stats

#---------------------------------------------------------------------------------------------------
def clear_stats(client, **kargs):
    for dev_id in rpc_clear_stats(client.stub, **kargs):
        click.echo(f'Cleared statistics for device ID {dev_id}.')

#---------------------------------------------------------------------------------------------------
def _show_stats(dev_id, stats, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)

    name_len = 0
    value_len = 0
    metrics = {}
    for metric in stats.metrics:
        base_name = metric.scope.zone
        if not metric.name.startswith(metric.scope.block):
            base_name += f'_{metric.scope.block}'
        base_name += f'_{metric.name}'

        is_array = metric.num_elements > 0
        last_update = format_timestamp(metric.last_update)
        for value in metric.values:
            if metric.type == StatsMetricType.STATS_METRIC_TYPE_FLAG:
                svalue = 'yes' if value.u64 != 0 else 'no'
            elif metric.type == StatsMetricType.STATS_METRIC_TYPE_GAUGE:
                svalue = f'{value.f64:.4g}'
            else:
                svalue = f'{value.u64}'
            value_len = max(value_len, len(svalue))

            name = base_name
            if is_array:
                name += f'[{value.index}]'
            name_len = max(name_len, len(name))

            metrics[name] = {
                'value': svalue,
                'last_update': last_update,
            }

    if metrics:
        last_update = kargs.get('last_update', False)
        for name in sorted(metrics, key=natural_sort_key):
            m = metrics[name]
            row = f'{name:>{name_len}}: {m["value"]:<{value_len}}'
            if last_update:
                row += f'    [{m["last_update"]}]'
            rows.append(row)

    click.echo('\n'.join(rows))

def show_stats(client, **kargs):
    for dev_id, stats in rpc_get_stats(client.stub, **kargs):
        _show_stats(dev_id, stats, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_stats_req(op, **kargs):
    yield BatchRequest(op=op, stats=stats_req(**kargs))

def batch_process_stats_resp(kargs):
    def process(resp):
        if not resp.HasField('stats'):
            return False

        supported_ops = {
            BatchOperation.BOP_GET: 'Got',
            BatchOperation.BOP_CLEAR: 'Cleared',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.stats
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_stats(resp.dev_id, resp.stats, kargs)
        else:
            click.echo(f'{op_label} statistics for device ID {resp.dev_id}.')
        return True

    return process

def batch_stats(op, **kargs):
    return batch_generate_stats_req(op, **kargs), batch_process_stats_resp(kargs)

#---------------------------------------------------------------------------------------------------
def clear_stats_options(fn):
    options = (
        device_id_option,
    )
    return apply_options(options, fn)

class Filter(click.ParamType):
    # Needed for auto-generated help (or implement get_metavar method instead).
    name = 'filter'

    TYPE_MAP = dict((name.upper(), enum) for enum, name in METRIC_TYPE_MAP.items())
    HELP = f'''
\b
\b
Filtering
================================================================================
Custom filters can be specified by providing Python "eval" style expressions via
one or more --filter options. A filter is applied to each metric and a boolean
result is computed which determines whether or not a particular metric should be
included. Multiple --filter options are logically ANDed together (which is
equivalent to using a single --filter option where the root filter is "all(...)"
and the sub-filters are it's members -- i.e. "--filter x --filter y" ==
"--filter 'all(x, y)'").
\b
\b
Methods for manipulating filter properties.
- neg(filter) -> filter
  Mark the filter to have it's boolean result negated after being applied.
\b
\b
Methods for grouping a set of filters for matching.
- any(filter_list) - > filter
  Create a new filter that is the union of multiple sub-filters such that the
  boolean result is the logical OR of the members.
\b
- all(filter_list) - > filter
  Create a new filter that is the intersection of multiple sub-filters such that
  the boolean result is the logical AND of the members.
\b
\b
Methods for creating a filter to match a specific attribute.
- type({"|".join(sorted(TYPE_MAP))}) -> filter
  Create a new filter to match on a metric's type attribute.
\b
- domain(string_match) -> filter
  Create a new filter to match on a metric's domain attribute string.
\b
- zone(string_match) -> filter
  Create a new filter to match on a metric's zone attribute string.
\b
- block(string_match) -> filter
  Create a new filter to match on a metric's block attribute string.
\b
- name(string_match) -> filter
  Create a new filter to match on a metric's name attribute string.
\b
- singleton - filter
  Create a new filter to match non-array metrics.
\b
- indices[slice_spec_list] -> filter
  Create a new filter to match array metrics with indices falling into at least
  one of the given slice spefications. A slice specification can select either a
  singular value or a range of values.
  - Selecting a singular value:
    - indices[i] -> filter
      Select array metrics with a valid index i.
    - indices[-i] -> filter
      Select array metrics with a valid index len(array) - i.
  - Selecting a range of values:
    - indices[i:j] -> filter
      Select array metrics with any valid indices k such that i <= k <= j.
    - indices[i:j:s] -> filter
      Select array metrics with any valid indices k such that i <= k <= j in
      increments of s.
    - indices[slice_spec0,slice_spec1,...] -> filter
      Select array metrics with any valid indices k such that k is in at least
      one of the given slice specifications.
\b
- label(key_string_match, value_string_match) -> filter
  Create a new filter to match on a metric's labels. The key and value fields
  may be set to "None" to indicate that they should be ignored during the match
  and treated as wilcards.
\b
\b
Methods for creating string matches used for specifying whether or not a filter
will select a metric.
- exact(string) -> string_match
  Create a new string match to assert the exact value of an attribute.
\b
- prefix(string) -> string_match
  Create a new string match to assert that an attribute begins with the given
  prefix.
\b
- suffix(string) -> string_match
  Create a new string match to assert that an attribute ends with the given
  suffix.
\b
- sub(string) -> string_match
  Create a new string match to assert that an attribute contains the given
  sub-string.
\b
- re(pattern) -> string_match
  Create a new string match to assert that an attribute conforms to the given
  regular expression pattern. Regular expressions use the ECMAScript format
  (as per https://en.cppreference.com/w/cpp/regex/ecmascript).
\b
- split_any(pattern, part_match) -> string_match
- split_all(pattern, part_match) -> string_match
  Create a new string match which will first split a string into parts using the
  given regular expression pattern and then apply the same match to each part.
  The boolean result of the split is computed from the individual results of the
  match on each part according to the variant being used:
  - split_any: Logical OR of the individual part matches.
  - split_all: Logical AND of the individual part matches.
\b
\b
Methods for creating matches on each part resulting from splitting a string.
- part_value(string_match) -> split_part_match
  Create a new split part match to select the part by it's string value. Same
  as all string matches (above).
\b
- part_index(index) -> split_part_match
  Create a new split part match to select the part by index. Used to match
  against a single part of the split (for cases where the order of the parts
  is significant). Use as one term in an "all" set of matches to restrict the
  other matches to only the chosen part.
\b
- any(split_part_list) - > split_part_match
  Create a new split part match that is the union of multiple sub-matches such
  that the boolean result is the logical OR of the members.
\b
- all(split_part_list) - > split_part_match
  Create a new split part match that is the intersection of multiple sub-matches
  such that the boolean result is the logical AND of the members.
\b
\b
Example 1: Filter to select only counter type metrics.
- Using the provided option:
  {{prog_name}} show stats --zeroes --metric-type counter
\b
- Using a custom filter:
  {{prog_name}} show stats --zeroes --filter 'type(COUNTER)'
\b
\b
Example 2: Filter to select only metrics counting packets.
- Using the provided option:
  {{prog_name}} show stats --zeroes --units packets
\b
- Using a custom filter:
  {{prog_name}} show stats --zeroes --filter 'label(exact("units"), exact("packets"))'
\b
\b
Example 3: Various custom filters to select metrics by array indices.
- Select all singleton (non-array) metrics:
  {{prog_name}} show stats --zeroes --filter singleton
\b
- Select all array metrics with the range of even indices 10 <= i <= 20:
  {{prog_name}} show stats --zeroes --filter 'indices[10:20:2]'
\b
- Select all singleton metrics and any array metrics outside the range of even
  indices 10 <= i <= 20:
  {{prog_name}} show stats --zeroes --filter 'neg(indices[10:20:2])'
\b
- Select only array metrics outside the range of even indices 10 <= i <= 20:
  {{prog_name}} show stats --zeroes \\
      --filter 'all(neg(singleton), neg(indices[10:20:2]))'
'''

    def convert(self, value, param, ctx):
        # Evaluate the expression to produce the filter message structure.
        try:
            return eval(value.strip(), self._namespace(param, ctx))
        except SyntaxError as e:
            msg = gettext.gettext('Syntax error in filter expression:\n' + str(e))
            self.fail(msg, param, ctx)

    def _namespace(self, param, ctx):
        fail = self.fail

        # Setup a mapping to restrict the scope of the global and local namespaces within which the
        # filter expression will be evaluated. The intent is to fully block access to all Python
        # builtins and only provide the methods and constants necessary for building filter message
        # structures for matching metrics.
        ns = {
            '__builtins__': None,
        }

        # Methods for manipulating filter properties.
        def _neg(filter):
            filter.negated = True
            return filter
        ns['neg'] = _neg

        # Methods for grouping a set of filters for matching.
        def _any(*members):
            if isinstance(members[0], StatsMetricMatchString.Split.Part):
                return StatsMetricMatchString.Split.Part(
                    any_set=StatsMetricMatchString.Split.Part.Set(members=members)
                )

            return StatsMetricFilter(any_set=StatsMetricFilter.Set(members=members))
        ns['any'] = _any

        def _all(*members):
            if isinstance(members[0], StatsMetricMatchString.Split.Part):
                return StatsMetricMatchString.Split.Part(
                    all_set=StatsMetricMatchString.Split.Part.Set(members=members)
                )
            return StatsMetricFilter(all_set=StatsMetricFilter.Set(members=members))
        ns['all'] = _all

        # Methods for creating a filter to match a specific attribute.
        ns.update(self.TYPE_MAP)
        def _type(type):
            return StatsMetricFilter(match=StatsMetricMatch(type=type))
        ns['type'] = _type

        def _domain(string):
            return StatsMetricFilter(match=StatsMetricMatch(domain=string))
        ns['domain'] = _domain

        def _zone(string):
            return StatsMetricFilter(match=StatsMetricMatch(zone=string))
        ns['zone'] = _zone

        def _block(string):
            return StatsMetricFilter(match=StatsMetricMatch(block=string))
        ns['block'] = _block

        def _name(string):
            return StatsMetricFilter(match=StatsMetricMatch(name=string))
        ns['name'] = _name

        class _indices:
            def __getitem__(self, key):
                if key is None:
                    key = ()
                elif not isinstance(key, collections.abc.Sequence):
                    key = (key,)

                slices = []
                for k in key:
                    if isinstance(k, int):
                        start = k
                        end = k
                        step = 1
                    elif isinstance(k, slice):
                        start = 0 if k.start is None else k.start
                        end = -1 if k.stop is None else k.stop
                        step = 1 if k.step is None else k.step
                    else:
                        msg = gettext.gettext(f'Array index "{k}" has unsupported type {type(k)}.')
                        fail(msg, param, ctx)
                    slices.append(StatsMetricMatchIndexSlice(start=start, end=end, step=step))

                return StatsMetricFilter(
                    match=StatsMetricMatch(indices=StatsMetricMatchIndices(slices=slices))
                )
        ns['indices'] = _indices()
        ns['singleton'] = _indices()[None]

        def _label(key, value):
            fields = {}
            if key is not None:
                fields['key'] = key
            if value is not None:
                fields['value'] = value
            return StatsMetricFilter(match=StatsMetricMatch(label=StatsMetricMatchLabel(**fields)))
        ns['label'] = _label

        # Methods for creating string matches.
        def _exact(string):
            return StatsMetricMatchString(exact=string)
        ns['exact'] = _exact

        def _prefix(string):
            return StatsMetricMatchString(prefix=string)
        ns['prefix'] = _prefix

        def _suffix(string):
            return StatsMetricMatchString(suffix=string)
        ns['suffix'] = _suffix

        def _sub(string):
            return StatsMetricMatchString(substring=string)
        ns['sub'] = _sub

        def _re(pattern):
            return StatsMetricMatchString(regexp=StringRegexp(pattern=pattern))
        ns['re'] = _re

        def _split(pattern, any, part):
            return StatsMetricMatchString(
                split=StatsMetricMatchString.Split(pattern=pattern, any=any, part=part)
            )

        def _split_any(pattern, part):
            return _split(pattern, True, part)
        ns['split_any'] = _split_any

        def _split_all(pattern, part):
            return _split(pattern, False, part)
        ns['split_all'] = _split_all

        # Methods for creating matches on each part resulting from a split.
        def _part_value(string):
            return StatsMetricMatchString.Split.Part(
                match=StatsMetricMatchString.Split.Part.Match(value=string)
            )
        ns['part_value'] = _part_value

        def _part_index(index):
            return StatsMetricMatchString.Split.Part(
                match=StatsMetricMatchString.Split.Part.Match(index=index)
            )
        ns['part_index'] = _part_index

        return ns

def show_stats_options(fn):
    options = (
        device_id_option,
        click.option(
            '--metric-type', '-m',
            'metric_types',
            type=click.Choice(sorted(name for name in METRIC_TYPE_RMAP)),
            multiple=True,
            help='Filter to restrict statistic metrics to the given type(s).',
        ),
        click.option(
            '--zeroes',
            is_flag=True,
            help='Include zero valued statistic metrics in the display.',
        ),
        click.option(
            '--last-update',
            is_flag=True,
            help='Include the metric last update timestamp in the display.',
        ),
        click.option(
            '--filter', '-f',
            'filters',
            type=Filter(),
            multiple=True,
            help='Custom expression for filtering metrics.',
        ),
    )
    return apply_options(options, fn)

#---------------------------------------------------------------------------------------------------
def add_batch_commands(cmd):
    # Click doesn't support nested groups when using command chaining, so the command hierarchy
    # needs to be flattened.
    @cmd.command(name='clear-stats')
    @clear_stats_options
    def clear_stats(**kargs):
        '''
        Clear SmartNIC statistics.
        '''
        return batch_stats(BatchOperation.BOP_CLEAR, **kargs)

    @cmd.command(name='show-stats')
    @show_stats_options
    def show_stats(**kargs):
        '''
        Display SmartNIC statistics.
        '''
        return batch_stats(BatchOperation.BOP_GET, **kargs)

#---------------------------------------------------------------------------------------------------
def add_clear_commands(cmd):
    @cmd.command
    @clear_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        '''
        Clear SmartNIC statistics.
        '''
        clear_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_show_commands(cmd, settings):
    filter_help = Filter.HELP.format(**settings)
    @cmd.command(
        help=f'''
Display SmartNIC statistics.
{filter_help}
''',
    )
    @show_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        show_stats(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_show_commands(cmds.show, cmds.settings)
