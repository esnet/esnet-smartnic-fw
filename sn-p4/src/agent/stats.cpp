#include "agent.hpp"
#include "device.hpp"
#include "stats.hpp"

#undef NDEBUG // Always force the assert to be non-empty.
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <regex>
#include <vector>

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
class BitArray {
public:
    BitArray(size_t nbits) : _nbits(nbits) {
        if (_nbits > NBITS_PER_WORD) {
            _nwords = (_nbits + NBITS_PER_WORD - 1) / NBITS_PER_WORD;
            _words = new Word[_nwords];
        } else {
            _nwords = 1;
            _words = &_word;
        }

        // Determine the mask of valid bits in the upper word.
        unsigned int rem = _nbits % NBITS_PER_WORD;
        _upper_mask = rem > 0 ? ((Word)1 << rem) - 1 : FULL_MASK;

        clear_all();
    }

    // Copy constructor.
    BitArray(const BitArray& src) : BitArray(src._nbits) {
        for (unsigned int w = 0; w < _nwords; ++w) {
            _words[w] = src._words[w];
        }
    }

    ~BitArray() {
        if (_words != &_word) {
            delete[] _words;
        }
    }

    size_t size() const {
        return _nbits;
    }

    size_t count_all_set() const {
        size_t count = 0;
        for (unsigned int pos = 0; pos < _nbits; ++pos) {
            if (is_bit_set(pos)) {
                count += 1;
            }
        }

        return count;
    }

    size_t count_all_cleared() const {
        return _nbits - count_all_set();
    }

    bool is_bit_set(unsigned int pos) const {
        if (pos < _nbits) {
            unsigned int w = pos / NBITS_PER_WORD;
            unsigned int p = pos % NBITS_PER_WORD;
            return (_words[w] & ((Word)1 << p)) != 0;
        }

        return false;
    }

    bool is_bit_cleared(unsigned int pos) const {
        return !is_bit_set(pos);
    }

    bool is_all_set() const {
        for (unsigned int w = 0; w < _nwords - 1; ++w) {
            if (_words[w] != FULL_MASK) {
                return false;
            }
        }

        return _words[_nwords - 1] == _upper_mask;
    }

    bool is_all_cleared() const {
        for (unsigned int w = 0; w < _nwords; ++w) {
            if (_words[w] != 0) {
                return false;
            }
        }

        return true;
    }

    void set_bit(unsigned int pos) {
        if (pos < _nbits) {
            unsigned int w = pos / NBITS_PER_WORD;
            unsigned int p = pos % NBITS_PER_WORD;
            _words[w] |= (Word)1 << p;
        }
    }

    void set_all() {
        for (unsigned int w = 0; w < _nwords - 1; ++w) {
            _words[w] = FULL_MASK;
        }
        _words[_nwords - 1] = _upper_mask;
    }

    void clear_bit(unsigned int pos) {
        if (pos < _nbits) {
            unsigned int w = pos / NBITS_PER_WORD;
            unsigned int p = pos % NBITS_PER_WORD;
            _words[w] &= ~((Word)1 << p);
        }
    }

    void clear_all() {
        for (unsigned int w = 0; w < _nwords; ++w) {
            _words[w] = 0;
        }
    }

    void negate_bit(unsigned int pos) {
        if (pos < _nbits) {
            unsigned int w = pos / NBITS_PER_WORD;
            unsigned int p = pos % NBITS_PER_WORD;
            _words[w] ^= (Word)1 << p;
        }
    }

    void negate_all() {
        for (unsigned int w = 0; w < _nwords - 1; ++w) {
            _words[w] ^= FULL_MASK;
        }
        _words[_nwords - 1] ^= _upper_mask;
    }

    void assign_bit(unsigned int pos, bool value) {
        if (value) {
            set_bit(pos);
        } else {
            clear_bit(pos);
        }
    }

    void assign_all(bool value) {
        if (value) {
            set_all();
        } else {
            clear_all();
        }
    }

    void operator&=(const BitArray& rhs) {
        assert(_nwords == rhs._nwords);

        for (unsigned int w = 0; w < _nwords; ++w) {
            _words[w] &= rhs._words[w];
        }
    }

    void operator|=(const BitArray& rhs) {
        assert(_nwords == rhs._nwords);

        for (unsigned int w = 0; w < _nwords; ++w) {
            _words[w] |= rhs._words[w];
        }
    }

private:
    typedef uint64_t Word;
    static const size_t NBITS_PER_WORD = sizeof(Word) * 8;

    size_t _nbits;
    size_t _nwords;
    Word* _words;
    Word _word;
    Word _upper_mask;
    static const Word FULL_MASK = ~((Word)0);
};

//--------------------------------------------------------------------------------------------------
static bool apply_metric_filter_match_string_regexp(const StringRegexp& regexp, const string str) {
    regex_constants::syntax_option_type syntax;
    switch (regexp.grammar()) {
    case StringRegexpGrammar::STR_REGEXP_GRAMMAR_BASIC_POSIX:
        syntax = regex_constants::basic;
        break;

    case StringRegexpGrammar::STR_REGEXP_GRAMMAR_EXTENDED_POSIX:
        syntax = regex_constants::extended;
        break;

    case StringRegexpGrammar::STR_REGEXP_GRAMMAR_AWK:
        syntax = regex_constants::awk;
        break;

    case StringRegexpGrammar::STR_REGEXP_GRAMMAR_GREP:
        syntax = regex_constants::grep;
        break;

    case StringRegexpGrammar::STR_REGEXP_GRAMMAR_EGREP:
        syntax = regex_constants::egrep;
        break;

    case StringRegexpGrammar::STR_REGEXP_GRAMMAR_UNKNOWN:
    case StringRegexpGrammar::STR_REGEXP_GRAMMAR_ECMA_SCRIPT:
    default:
        syntax = regex_constants::ECMAScript;
        break;
    }

    regex re(regexp.pattern(), syntax);
    return regex_match(str, re);
}

//--------------------------------------------------------------------------------------------------
static bool apply_metric_filter_match_string(const StatsMetricMatchString& match, const string str);

static bool apply_metric_filter_match_string_split_part_attr(
    const StatsMetricMatchString::Split::Part::Match& match,
    const string str,
    const unsigned int index) {
    bool ok = false;
    switch (match.attribute_case()) {
    case StatsMetricMatchString::Split::Part::Match::AttributeCase::kValue:
        ok = apply_metric_filter_match_string(match.value(), str);
        break;

    case StatsMetricMatchString::Split::Part::Match::AttributeCase::kIndex:
        ok = match.index() == index;
        break;

    case StatsMetricMatchString::Split::Part::Match::AttributeCase::ATTRIBUTE_NOT_SET:
        // Treat as a wildcard that always matches.
        ok = true;
        break;
    }

    return ok;
}

//--------------------------------------------------------------------------------------------------
static bool apply_metric_filter_match_string_split_part(
    const StatsMetricMatchString::Split::Part& part,
    const string str,
    const unsigned int index) {
    bool ok = false;
    switch (part.term_case()) {
    case StatsMetricMatchString::Split::Part::TermCase::kMatch:
        ok = apply_metric_filter_match_string_split_part_attr(part.match(), str, index);
        break;

    case StatsMetricMatchString::Split::Part::TermCase::kAnySet: {
        auto set = part.any_set();
        ok = set.members_size() < 1; // Treat as a wildcard that always matches.
        for (const auto& member : set.members()) {
            ok = ok || apply_metric_filter_match_string_split_part(member, str, index);
            if (ok) { // Short-circuit logical OR.
                break;
            }
        }
        break;
    }

    case StatsMetricMatchString::Split::Part::TermCase::kAllSet: {
        auto set = part.all_set();
        ok = true; // Treat as a wildcard that always matches.
        for (const auto& member : set.members()) {
            ok = ok && apply_metric_filter_match_string_split_part(member, str, index);
            if (!ok) { // Short-circuit logical AND.
                break;
            }
        }
        break;
    }

    case StatsMetricMatchString::Split::Part::TermCase::TERM_NOT_SET:
        // Treat as a wildcard that always matches.
        ok = true;
        break;
    }

    if (part.negated()) {
        ok = !ok;
    }

    return ok;
}

//--------------------------------------------------------------------------------------------------
static bool apply_metric_filter_match_string_split(
    const StatsMetricMatchString::Split& split,
    const string str) {
    regex split_re(split.pattern(), regex_constants::ECMAScript);
    sregex_token_iterator parts(str.begin(), str.end(), split_re, -1);
    sregex_token_iterator parts_end;

    auto part = split.part();
    auto any = split.any();
    unsigned int index = 0;
    for (; parts != parts_end; ++parts, ++index) {
        bool ok = apply_metric_filter_match_string_split_part(part, *parts, index);
        if (any && ok) { // Short-circuit logical OR.
            return true;
        }
        if (!any && !ok) { // Short-circuit logical AND.
            return false;
        }
    }

    return !any && index > 0;
}

//--------------------------------------------------------------------------------------------------
static bool apply_metric_filter_match_string(const StatsMetricMatchString& match,
                                             const string str) {
    bool ok = false;
    switch (match.method_case()) {
    case StatsMetricMatchString::MethodCase::kExact:
        ok = match.exact() == str;
        break;

    case StatsMetricMatchString::MethodCase::kPrefix: {
        const auto prefix = match.prefix();
        auto prefix_len = prefix.size();
        ok =
            prefix_len <= str.size() &&
            str.compare(0, prefix_len, prefix) == 0;
        break;
    }

    case StatsMetricMatchString::MethodCase::kSuffix: {
        const auto suffix = match.suffix();
        auto suffix_len = suffix.size();
        auto str_len = str.size();
        ok =
            suffix_len <= str_len &&
            str.compare(str_len - suffix_len, suffix_len, suffix) == 0;
        break;
    }

    case StatsMetricMatchString::MethodCase::kSubstring: {
        const auto sub = match.substring();
        auto sub_len = sub.size();
        ok =
            sub_len <= str.size() &&
            str.find(sub) != string::npos;
        break;
    }

    case StatsMetricMatchString::MethodCase::kRegexp:
        ok = apply_metric_filter_match_string_regexp(match.regexp(), str);
        break;

    case StatsMetricMatchString::MethodCase::kSplit:
        ok = apply_metric_filter_match_string_split(match.split(), str);
        break;

    case StatsMetricMatchString::MethodCase::METHOD_NOT_SET:
        // Treat as a wildcard that always matches.
        ok = true;
        break;
    }

    return ok;
}

//--------------------------------------------------------------------------------------------------
static void apply_metric_filter_match_indices(const StatsMetricMatchIndices& indices,
                                              const struct stats_for_each_spec* spec,
                                              BitArray& valid) {
    bool is_array = STATS_METRIC_FLAG_TEST(spec->metric->flags, ARRAY);
    if (!is_array) {
        // An empty list of slices means to only match the metric if it's a singleton.
        valid.assign_all(indices.slices_size() < 1);
        return;
    }

    auto size = valid.size();
    for (const auto& slice : indices.slices()) {
        auto start = slice.start();
        if (start < 0) {
            start += size;
            if (start < 0) {
                start = 0;
            }
        }

        if ((size_t)start >= size) {
            continue; // The range is empty.
        }

        auto end = slice.end();
        if (end < 0) {
            end += size;
            if (end < 0) {
                continue; // The range is empty.
            }
        }

        if ((size_t)end >= size) {
            end = size - 1;
        }

        if (start > end) {
            continue; // The range is empty.
        }

        // The step size must be a positive integer. A value of 0 is interpreted as 1. Since the
        // intent is for filtering rather than sorting, using a negative step to reverse the
        // sequence is not supported.
        auto step = slice.step();
        if (step < 1) {
            step = 1;
        }

        for (auto n = start; n <= end; n += step) {
            valid.set_bit(n);
        }
    }
}

//--------------------------------------------------------------------------------------------------
static void apply_metric_filter_match_label(const StatsMetricMatchLabel& label,
                                            const struct stats_for_each_spec* spec,
                                            BitArray& valid) {
    bool has_key = label.has_key();
    auto key = label.key();

    bool has_value = label.has_value();
    auto value = label.value();

    for (unsigned int n = 0; n < spec->nvalues; ++n) {
        auto v = &spec->values[n];
        for (auto vl = v->labels; vl < &v->labels[v->nlabels]; ++vl) {
            // Treat missing key as a wildcard that always matches.
            if (has_key && !apply_metric_filter_match_string(key, vl->key)) {
                continue;
            }

            // Treat missing value as a wildcard that always matches.
            if (has_value && !apply_metric_filter_match_string(value, vl->value)) {
                continue;
            }

            valid.assign_bit(n, true);
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
static void apply_metric_filter_match(const struct stats_for_each_spec* spec,
                                      const StatsMetricMatch& match,
                                      const StatsMetricType type,
                                      BitArray& valid) {
    bool ok = false;
    switch (match.attribute_case()) {
    case StatsMetricMatch::AttributeCase::kType:
        // Validity is computed once for all indices since the attribute is shared by all values.
        ok = match.type() == type;
        break;

    case StatsMetricMatch::AttributeCase::kDomain:
        // Validity is computed once for all indices since the attribute is shared by all values.
        ok = apply_metric_filter_match_string(match.domain(), spec->domain->name);
        break;

    case StatsMetricMatch::AttributeCase::kZone:
        // Validity is computed once for all indices since the attribute is shared by all values.
        ok = apply_metric_filter_match_string(match.zone(), spec->zone->name);
        break;

    case StatsMetricMatch::AttributeCase::kBlock:
        // Validity is computed once for all indices since the attribute is shared by all values.
        ok = apply_metric_filter_match_string(match.block(), spec->block->name);
        break;

    case StatsMetricMatch::AttributeCase::kName:
        // Validity is computed once for all indices since the attribute is shared by all values.
        ok = apply_metric_filter_match_string(match.name(), spec->metric->name);
        break;

    case StatsMetricMatch::AttributeCase::kIndices:
        // Validity is computed per index based on whether each is included in the given slices.
        apply_metric_filter_match_indices(match.indices(), spec, valid);
        return;

    case StatsMetricMatch::AttributeCase::kLabel:
        // Validity is computed per index based on whether each value has the given labels.
        apply_metric_filter_match_label(match.label(), spec, valid);
        return;

    case StatsMetricMatch::AttributeCase::ATTRIBUTE_NOT_SET:
        // Treat as a wildcard that always matches for all indices.
        ok = true;
        break;
    }

    valid.assign_all(ok);
}

//--------------------------------------------------------------------------------------------------
static void apply_metric_filter(const struct stats_for_each_spec* spec,
                                const StatsMetricFilter& filter,
                                const StatsMetricType type,
                                BitArray& valid) {
    switch (filter.term_case()) {
    case StatsMetricFilter::TermCase::kMatch:
        apply_metric_filter_match(spec, filter.match(), type, valid);
        break;

    case StatsMetricFilter::TermCase::kAnySet: {
        auto set = filter.any_set();
        if (set.members_size() < 1) {
            // Treat as a wildcard that always matches for all indices.
            valid.set_all();
            break;
        }

        BitArray v(valid.size());
        for (const auto& member : set.members()) {
            apply_metric_filter(spec, member, type, v);
            valid |= v;
            if (valid.is_all_set()) { // Short-circuit logical OR.
                break;
            }
            v.clear_all();
        }
        break;
    }

    case StatsMetricFilter::TermCase::kAllSet: {
        auto set = filter.all_set();
        valid.set_all();
        if (set.members_size() < 1) {
            // Treat as a wildcard that always matches for all indices.
            break;
        }

        BitArray v(valid.size());
        for (const auto& member : set.members()) {
            apply_metric_filter(spec, member, type, v);
            valid &= v;
            if (valid.is_all_cleared()) { // Short-circuit logical AND.
                break;
            }
            v.clear_all();
        }
        break;
    }

    case StatsMetricFilter::TermCase::TERM_NOT_SET:
        // Treat as a wildcard that always matches for all indices.
        valid.set_all();
        break;
    }

    if (filter.negated()) {
        valid.negate_all();
    }
}

//--------------------------------------------------------------------------------------------------
static void apply_filters(const struct stats_for_each_spec* spec,
                          const StatsFilters& filters,
                          const StatsMetricType type,
                          BitArray& valid) {
    bool non_zero = filters.non_zero();
    for(unsigned int n = 0; n < spec->nvalues; ++n) {
        valid.assign_bit(n, !non_zero || spec->values[n].u64 != 0);
    }

    if (valid.is_all_cleared()) {
        return;
    }

    if (filters.has_metric_filter()) {
        BitArray v(valid.size());
        apply_metric_filter(spec, filters.metric_filter(), type, v);
        valid &= v;
    }
}

//--------------------------------------------------------------------------------------------------
extern "C" {
    int get_stats_for_each_metric(const struct stats_for_each_spec* spec) {
        GetStatsContext* ctx = static_cast<typeof(ctx)>(spec->arg);

        StatsMetricType type;
        switch (spec->metric->type) {
        case stats_metric_type_COUNTER:
            type = StatsMetricType::STATS_METRIC_TYPE_COUNTER;
            break;

        case stats_metric_type_GAUGE:
            type = StatsMetricType::STATS_METRIC_TYPE_GAUGE;
            break;

        case stats_metric_type_FLAG:
            type = StatsMetricType::STATS_METRIC_TYPE_FLAG;
            break;

        default:
            return 0;
        }

        BitArray valid(spec->nvalues);
        apply_filters(spec, ctx->filters, type, valid);
        if (valid.is_all_cleared()) {
            return 0;
        }

        auto metric = ctx->stats->add_metrics();
        metric->set_type(type);
        metric->set_name(spec->metric->name);
        metric->set_num_elements(spec->metric->nelements);

        auto scope = metric->mutable_scope();
        scope->set_domain(spec->domain->name);
        scope->set_zone(spec->zone->name);
        scope->set_block(spec->block->name);

        auto last_update = metric->mutable_last_update();
        last_update->set_seconds(spec->last_update.tv_sec);
        last_update->set_nanos(spec->last_update.tv_nsec);

        bool with_labels = ctx->filters.with_labels();
        for (unsigned int n = 0; n < spec->nvalues; ++n) {
            if (!valid.is_bit_set(n)) {
                continue;
            }

            auto v = &spec->values[n];
            auto value = metric->add_values();
            value->set_index(n);
            value->set_u64(v->u64);
            value->set_f64(v->f64);

            if (with_labels) {
                for (auto l = v->labels; l < &v->labels[v->nlabels]; ++l) {
                    auto label = value->add_labels();
                    label->set_key(l->key);
                    label->set_value(l->value);
                }
            }
        }

        return 0;
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::get_or_clear_stats(
    const StatsRequest& req,
    bool do_clear,
    function<void(const StatsResponse&)> write_resp) {
    auto debug_flag = ServerDebugFlag::DEBUG_FLAG_STATS;
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        StatsResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    GetStatsContext ctx{
        .filters = req.filters(),
        .stats = NULL,
    };

    if (!do_clear) {
        SERVER_LOG_IF_DEBUG(debug_flag, INFO,
            "---> Filters:" << endl << ctx.filters.DebugString());
    }

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        StatsResponse resp;
        if (!do_clear) {
            ctx.stats = resp.mutable_stats();
        }

        for (auto domain : dev->stats.domains) {
            if (do_clear) {
                stats_domain_clear_metrics(domain);
            } else {
                stats_domain_for_each_metric(domain, get_stats_for_each_metric, &ctx);
            }
        }

        resp.set_error_code(ErrorCode::EC_OK);
        resp.set_dev_id(dev_id);

        write_resp(resp);
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_get_stats(
    const StatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_stats(req, false, [&rdwr](const StatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::GetStats(
    [[maybe_unused]] ServerContext* ctx,
    const StatsRequest* req,
    ServerWriter<StatsResponse>* writer) {
    get_or_clear_stats(*req, false, [&writer](const StatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_clear_stats(
    const StatsRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_or_clear_stats(req, true, [&rdwr](const StatsResponse& resp) -> void {
        BatchResponse bresp;
        auto stats = bresp.mutable_stats();
        stats->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_CLEAR);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::ClearStats(
    [[maybe_unused]] ServerContext* ctx,
    const StatsRequest* req,
    ServerWriter<StatsResponse>* writer) {
    get_or_clear_stats(*req, true, [&writer](const StatsResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
