#---------------------------------------------------------------------------------------------------
__all__ = (
    'add_sub_command',
    'stats_req_kargs',
    'stats_show_base_options',
    'stats_show_format',
)

import click
import collections
import gettext
import grpc
import re
import types

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
def stats_req_kargs(dev_id, stats_kargs):
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

        units = stats_kargs.get('units')
        if units:
            units_filters = root.all_set.members.add()
            for u in units:
                uf = units_filters.any_set.members.add()
                uf.match.label.key.exact = 'units'
                uf.match.label.value.exact = u

        req_kargs['filters'] = StatsFilters(
            non_zero=not stats_kargs.get('zeroes'),
            with_labels=stats_kargs.get('labels') or stats_kargs.get('aliases'),
            metric_filter=root,
        )

    return req_kargs

def stats_req(dev_id, **stats_kargs):
    return StatsRequest(**stats_req_kargs(dev_id, stats_kargs))

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
def stats_show_format(stats, kargs):
    with_last_update = kargs.get('last_update', False)
    with_labels = kargs.get('labels', False)
    with_aliases = kargs.get('aliases', False)
    with_long_name = kargs.get('long_name', False)

    metrics = types.SimpleNamespace(by_short_name={}, by_partial_name={}, by_long_name={})
    for metric in stats.metrics:
        is_array = metric.num_elements > 0
        last_update = format_timestamp(metric.last_update)
        for value in metric.values:
            if metric.type == StatsMetricType.STATS_METRIC_TYPE_FLAG:
                svalue = 'yes' if value.u64 != 0 else 'no'
            elif metric.type == StatsMetricType.STATS_METRIC_TYPE_GAUGE:
                svalue = f'{value.f64:.4g}'
            else:
                svalue = f'{value.u64}'

            labels = dict((l.key, l.value) for l in value.labels)

            short_name = None
            if with_aliases:
                alias = labels.get('alias')
                if alias:
                    short_name = alias
            if short_name is None:
                short_name = metric.name
                if is_array:
                    short_name += f'[{value.index}]'

            partial_name = f'{metric.scope.block}.{short_name}'
            long_name = f'{metric.scope.zone}.{partial_name}'
            m = types.SimpleNamespace(
                short_name=short_name,
                partial_name=partial_name,
                long_name=long_name,
                value=svalue,
                last_update=last_update,
                labels=labels,
            )

            mlist = metrics.by_short_name.setdefault(short_name, [])
            mlist.append(m)

            mlist = metrics.by_partial_name.setdefault(partial_name, [])
            mlist.append(m)

            mlist = metrics.by_long_name.setdefault(long_name, [])
            mlist.append(m)

    if with_long_name:
        metrics.final = metrics.by_long_name
    else:
        metrics.final = {}
        for short_name, slist in metrics.by_short_name.items():
            if len(slist) == 1:
                metrics.final[short_name] = slist
                continue

            for m in slist:
                plist = metrics.by_partial_name[m.partial_name]
                if len(plist) == 1:
                    metrics.final[m.partial_name] = plist
                else:
                    metrics.final[m.long_name] = metrics.by_long_name[m.long_name]

    rows = []
    if metrics.final:
        name_len = 0
        value_len = 0
        for name, mlist in metrics.final.items():
            name_len = max(name_len, len(name))
            for metric in mlist:
                value_len = max(value_len, len(metric.value))

        for name in sorted(metrics.final, key=natural_sort_key):
            for metric in metrics.final[name]:
                row = f'{name:>{name_len}}: {metric.value:<{value_len}}'
                if with_last_update:
                    row += f'    [{metric.last_update}]'
                rows.append(row)

                if with_labels:
                    for k, v in sorted(metric.labels.items(), key=lambda pair: pair[0]):
                        rows.append(f'{"":>{name_len}}  <{k}="{v}">')

    return rows

def _show_stats(dev_id, stats, kargs):
    rows = []
    rows.append(HEADER_SEP)
    rows.append(f'Device ID: {dev_id}')
    rows.append(HEADER_SEP)
    rows.extend(stats_show_format(stats, kargs))
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
def stats_view_req(dev_id, **stats_kargs):
    views = StatsMetricMatchLabel()
    views.key.exact = 'views'
    views.value.split.pattern = ','
    views.value.split.any = True

    for vf in stats_kargs.get('view_filters', ()):
        p = views.value.split.part.any_set.members.add()
        p.match.value.regexp.pattern = vf

    stats_kargs.update({
        'filters': (StatsMetricFilter(match=StatsMetricMatch(label=views)),),
        'labels': True,
        'metric_types': ('counter',),
        'units': ('packets',) + (('bytes',) if stats_kargs.get('bytes', False) else ()),
    })
    return stats_req(dev_id, **stats_kargs)

#---------------------------------------------------------------------------------------------------
def rpc_stats_view(op, **kargs):
    req = stats_view_req(**kargs)
    try:
        for resp in op(req):
            if resp.error_code != ErrorCode.EC_OK:
                raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))
            yield resp
    except grpc.RpcError as e:
        raise click.ClickException(str(e))

def rpc_get_stats_view(stub, **kargs):
    for resp in rpc_stats_view(stub.GetStats, **kargs):
        yield resp.dev_id, resp.stats

#---------------------------------------------------------------------------------------------------
def _show_stats_view(dev_id, stats, kargs):
    class Metric:
        def __init__(self):
            self.packets = 0
            self.bytes = 0

        def __str__(self):
            return self.format_packets()

        def format_packets(self):
            return f'{self.packets:,}'

        def format_bytes(self):
            return f'[{self.bytes:,}B]'

    class Column:
        def __init__(self, port, direction):
            self.port = port
            self.direction = direction

            self.width = 0
            self.rows = []

            self.append(None, fill='=')
            if port is None:
                self.append(f'Device ID {dev_id}', align='<')
            else:
                self.append(f'Port {port} ' + direction.capitalize())

        def append(self, entry, fill='', align='>'):
            if entry is not None:
                entry = ' ' + str(entry) + ' '
                self.width = max(self.width, len(entry))
            self.rows.append((entry, fill, align))

        def format(self, idx):
            entry, fill, align = self.rows[idx]
            if entry is None:
                entry = ''
            return f'{entry:{fill}{align}{self.width}}'

    view_filters = tuple(re.compile(v) for v in kargs.get('view_filters'))
    views = {}
    ports = {}
    for metric in stats.metrics:
        name = metric.scope.block
        value = metric.values[0]

        labels = dict((l.key, l.value) for l in value.labels)
        units = labels.get('units', 'packets')

        for view_spec in labels.get('views').split(','):
            for view_filter in view_filters:
                match = view_filter.match(view_spec)
                if match is not None:
                    view_name, view_port, view_dir = view_spec.split(ViewFilter.FIELD_SEP)
                    col_key = view_port + view_dir
                    ports.setdefault(view_port, {}).setdefault(view_dir, col_key)

                    name = name.replace('app' + view_port, 'app*')
                    suffix = '_' + view_port
                    if name.endswith(suffix):
                        name = name.removesuffix(suffix) + '_*'

                    view = views.setdefault(view_name, types.SimpleNamespace(rows={}, totals={}))
                    row_cols = view.rows.setdefault(name, {})

                    m = row_cols.setdefault(col_key, Metric())
                    t = view.totals.setdefault(col_key, Metric())
                    if units == 'bytes':
                        m.bytes = value.u64
                        t.bytes += m.bytes
                    else:
                        m.packets = value.u64
                        t.packets += m.packets
                    break

    name_col = Column(None, None)
    columns = [name_col]
    metric_cols = {}
    for port in sorted(ports, key=natural_sort_key):
        directions = ports[port]
        for direction in sorted(directions, key=natural_sort_key):
            col_key = directions[direction]
            col = Column(port, direction)
            columns.append(col)
            metric_cols[col_key] = col

    with_bytes = kargs.get('bytes', False)
    for view_name in sorted(views, key=natural_sort_key):
        for col in columns:
            col.append(None, fill='=')
            col.append(view_name if col is name_col else None, align='<')
            col.append(None, fill='-')

        view = views[view_name]
        for name in sorted(view.rows, key=natural_sort_key):
            name_col.append(name)
            if with_bytes:
                name_col.append(None)

            row_cols = view.rows[name]
            for col_key, col in metric_cols.items():
                entry = row_cols.get(col_key)
                col.append(entry)
                if with_bytes:
                    col.append(entry.format_bytes() if entry is not None else None)

        name_col.append('totals')
        if with_bytes:
            name_col.append(None)

        for col_key, col in metric_cols.items():
            entry = view.totals.get(col_key)
            col.append(entry)
            if with_bytes:
                col.append(entry.format_bytes() if entry is not None else None)

    if views:
        for col in columns:
            col.append(None, fill='=')

        rows = []
        for idx in range(len(name_col.rows)):
            row = ''
            prev = None
            for col in columns:
                col_sep = '|' if prev is None or prev.port == col.port else '||'
                row += col_sep + col.format(idx)
                prev = col
            rows.append(row + '|')

        click.echo('\n'.join(rows))

def show_stats_view(client, **kargs):
    for dev_id, stats in rpc_get_stats_view(client.stub, **kargs):
        _show_stats_view(dev_id, stats, kargs)

#---------------------------------------------------------------------------------------------------
def batch_generate_stats_view_req(op, **kargs):
    yield BatchRequest(op=op, stats=stats_view_req(**kargs))

def batch_process_stats_view_resp(kargs):
    def process(resp):
        if not resp.HasField('stats'):
            return False

        supported_ops = {
            BatchOperation.BOP_GET: 'Got',
        }
        op = resp.op
        if op not in supported_ops:
            raise click.ClickException('Response for unsupported batch operation: {op}')
        op_label = supported_ops[op]

        resp = resp.stats
        if resp.error_code != ErrorCode.EC_OK:
            raise click.ClickException('Remote failure: ' + error_code_str(resp.error_code))

        if op == BatchOperation.BOP_GET:
            _show_stats_view(resp.dev_id, resp.stats, kargs)
        return True

    return process

def batch_stats_view(op, **kargs):
    return batch_generate_stats_view_req(op, **kargs), batch_process_stats_view_resp(kargs)

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
\b
\b
Example 4: Filter to select only metrics counting packets on the output side of
           port 1 of the sn.egr component (and all of it's sub-components) of
           the SmartNIC datapath.
- Using the provided sub-command:
  {{prog_name}} show stats view --zeroes --view-filter 'sn.egr.*:1:out'
\b
- Using a custom filter:
  {{prog_name}} show stats --zeroes \\
      --filter 'label(exact("units"), exact("packets"))' \\
      --filter '
          label( \\
              exact("views"), \\
              split_any(",", \\
                  part_value(re(r"^sn\\.egr\\.[^:]*:1:out$")), \\
              ) \\
          )'
\b
- Using a more complex custom filter that matches each field of the view
  specification separately.
  {{prog_name}} show stats --zeroes \\
      --filter 'label(exact("units"), exact("packets"))' \\
      --filter '
          label( \\
              exact("views"), \\
              split_any(",", \\
                  part_value(
                      split_all(":", \\
                          any( \\
                              all( \\
                                  part_index(0), \\
                                  part_value(re(r"^sn\\.egr\\.[^:]*$")), \\
                              ), \\
                              all( \\
                                  part_index(1), \\
                                  part_value(exact("1")), \\
                              ), \\
                              all( \\
                                  part_index(2), \\
                                  part_value(exact("out")), \\
                              ) \\
                          ) \\
                      ) \\
                  ) \\
              ) \\
          )'
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

def stats_show_base_options():
    return (
        click.option(
            '--metric-type', '-m',
            'metric_types',
            type=click.Choice(sorted(name for name in METRIC_TYPE_RMAP)),
            multiple=True,
            help='''
            Filter to restrict statistic metrics to the given type(s). Multiple options will result
            in the logical OR of the given types. The set of all the given types will be logically
            ANDed with other filtering options.
            ''',
        ),
        click.option(
            '--zeroes', '-z',
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
            help='''
            Custom expression for filtering metrics. Multiple options will result in the logical AND
            of the given filters, along with any other filtering options.
            ''',
        ),
        click.option(
            '--labels', '-l',
            is_flag=True,
            help='Include all labels associated with each metric in the display.',
        ),
        click.option(
            '--units', '-u',
            multiple=True,
            help='''
            Filter to restrict statistic metrics to the given units (only applicable to metrics that
            have been defined with the "units" label). Common units are "packets" and "bytes".
            Multiple options will result in the logical OR of the given units. The set of all the
            given units will be logically ANDed with other filtering options.
            ''',
        ),
        click.option(
            '--aliases', '-a',
            is_flag=True,
            help='Use alias label instead of metric name when present.',
        ),
        click.option(
            '--long-name', '-n',
            is_flag=True,
            help='''
            Use the fully-qualified long format for the metric name. The format is:
            <zone>.<block>.<name>, where <name> is either the metric name or an alias if the
            --aliases flag is specified.
            ''',
        ),
    )

def show_stats_options(fn):
    options = (
        device_id_option,
    ) + stats_show_base_options()
    return apply_options(options, fn)

class ViewFilter(click.ParamType):
    # Needed for auto-generated help (or implement get_metavar method instead).
    name = 'view_filter'

    NFIELDS = 3
    FIELD_SEP = ':'
    FIELD_GLOB_RE = r'[^' + FIELD_SEP + ']*'

    def _fixup_field(self, field):
        # View fields are matched by regular expression, so special characters need to be escaped.
        return field.replace('.', r'\.').replace('*', self.FIELD_GLOB_RE)

    def convert(self, value, param, ctx):
        fields = [self._fixup_field(f) for f in value.split(self.FIELD_SEP)]
        nfields = len(fields)
        if nfields == 1 and fields[0] == '':
            msg = gettext.gettext('Empty view specification filter.')
            self.fail(msg, param, ctx)

        if nfields > self.NFIELDS:
            msg = gettext.gettext(
                f'Too many fields in view specification filter "{value}". Expected no more than '
                f'{self.NFIELDS} fields.')
            self.fail(msg, param, ctx)

        if nfields < self.NFIELDS:
            fields.extend((self.NFIELDS - nfields)*[self.FIELD_GLOB_RE])

        return '^' + self.FIELD_SEP.join(fields) + '$'

def show_stats_view_options(fn):
    options = (
        device_id_option,
        click.option(
            '--zeroes', '-z',
            is_flag=True,
            help='Include zero valued statistic metrics in the display.',
        ),
        click.option(
            '--bytes', '-b',
            is_flag=True,
            help='Include byte counts in the display.',
        ),
        click.option(
            '--view-filter', '-v',
            'view_filters',
            type=ViewFilter(),
            multiple=True,
            default=('*',),
            show_default=True,
            help = '''
            A filter to select which metrics to display based on their view specifications. A view
            specification filter is a triplet of the form <name>:<port>:<direction>, where <name> is
            a "." separated sequence of SmartNIC datapath components naming the view (any of which
            may be globbed using "*"), <port> is 0, 1 or "*" for any, and <direction> is "in", "out"
            or "*" for any.
            ''',
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

    @cmd.command(name='show-stats-view')
    @show_stats_view_options
    def show_stats_view(**kargs):
        '''
        Display SmartNIC statistics by view.
        '''
        return batch_stats_view(BatchOperation.BOP_GET, **kargs)

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
    @cmd.group(
        invoke_without_command=True,
        help=f'''
Display SmartNIC statistics.
{filter_help}
''',
    )
    @show_stats_options
    @click.pass_context
    def stats(ctx, **kargs):
        if ctx.invoked_subcommand is None:
            show_stats(ctx.obj, **kargs)

    @stats.command
    @show_stats_view_options
    @click.pass_context
    def view(ctx, **kargs):
        '''
        Display SmartNIC statistics by view.
        '''
        show_stats_view(ctx.obj, **kargs)

#---------------------------------------------------------------------------------------------------
def add_sub_commands(cmds):
    add_batch_commands(cmds.batch)
    add_clear_commands(cmds.clear)
    add_show_commands(cmds.show, cmds.settings)
