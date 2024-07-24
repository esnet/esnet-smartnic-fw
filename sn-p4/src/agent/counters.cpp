#include "agent.hpp"
#include "device.hpp"

#include <cstdlib>
#include <sstream>
#include <string.h>

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

#include "snp4.h"

using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
extern "C" {
    struct CountersIoData {
        void* handle;
        const struct snp4_info_counter_block* info;
    };

    struct CountersLatchData {
        bool valid;

        // simple: counts  => values[0:num_counters-1]
        // combo:  packets => values[0:num_counters-1]
        //         bytes   => values[num_counters:2*num_counters-1]
        uint64_t values[0]; // Must be last.
    };

    static void counters_latch_metrics(const struct stats_block_spec* bspec, void* data) {
        CountersIoData* iod = (typeof(iod))bspec->io.data.ptr;
        CountersLatchData* ld = (typeof(ld))data;

        switch (iod->info->type) {
        case SNP4_INFO_COUNTER_TYPE_PACKETS_AND_BYTES:
            ld->valid = snp4_counter_block_combo_read(
                iod->handle, iod->info->name,
                &ld->values[0], &ld->values[iod->info->num_counters], iod->info->num_counters);
            break;

        case SNP4_INFO_COUNTER_TYPE_PACKETS:
        case SNP4_INFO_COUNTER_TYPE_BYTES:
        case SNP4_INFO_COUNTER_TYPE_FLAG:
            ld->valid = snp4_counter_block_simple_read(
                iod->handle, iod->info->name, ld->values, iod->info->num_counters);
            break;
        }
    }

    static void counters_read_metric([[maybe_unused]] const struct stats_block_spec* bspec,
                                     const struct stats_metric_spec* mspec,
                                     uint64_t* values,
                                     void* data) {
        CountersLatchData* ld = (typeof(ld))data;
        if (!ld->valid) {
            memset(values, 0, sizeof(uint64_t) * mspec->nelements);
            return;
        }

        uint64_t* lv = &ld->values[mspec->io.offset];
        for (unsigned int n = 0; n < mspec->nelements; ++n) {
            values[n] = lv[n];
        }
    }
}

//--------------------------------------------------------------------------------------------------
#define COUNTER_NLABELS 1
#define COUNTER_NSTRINGS 1

struct InitCountersBlock {
    const struct snp4_info_pipeline* pipeline;
    const struct snp4_info_counter_block* info;
    struct stats_block_spec* bspec;
    struct stats_label_spec* labels;
    struct stats_metric_spec* mspecs;
    unsigned int nmetrics;
    string* strings;
};

static void init_counters_block_metric(InitCountersBlock* blk,
                                       unsigned int idx,
                                       const char* suffix) {
    auto mspec = &blk->mspecs[idx];
    auto labels = &blk->labels[idx * COUNTER_NLABELS];
    auto strs = &blk->strings[idx * COUNTER_NSTRINGS];

    ostringstream name;
    name << blk->info->name;
    if (suffix != NULL) {
        name << '_' << suffix;
    }
    *strs = name.str();
    mspec->name = strs->c_str();
    strs += 1;

    mspec->labels = labels;
    mspec->nlabels = COUNTER_NLABELS;
    mspec->type =
        blk->info->type == SNP4_INFO_COUNTER_TYPE_FLAG ?
        stats_metric_type_FLAG : stats_metric_type_COUNTER;
    mspec->flags =
        STATS_METRIC_FLAG_MASK(CLEAR_ON_READ) |
        STATS_METRIC_FLAG_MASK(ARRAY);
    mspec->nelements = blk->info->num_counters;
    mspec->io.offset = idx * blk->info->num_counters;

    labels->key = "pipeline";
    labels->value = blk->pipeline->name;
    labels += 1;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::init_counters(Device* dev, DevicePipeline* pipeline) {
    const auto pi = &pipeline->info;
    if (pi->num_counter_blocks == 0) {
        pipeline->stats.counters = NULL;
        return;
    }

    ostringstream zone_name;
    zone_name << "pipeline" << pipeline->id;

    auto stats = new DeviceStats;
    stats->zone_name = zone_name.str();

    InitCountersBlock blocks[pi->num_counter_blocks];
    memset(blocks, 0, sizeof(blocks));

    struct stats_block_spec bspecs[pi->num_counter_blocks];
    memset(bspecs, 0, sizeof(bspecs));

    unsigned int total_nmetrics = 0;
    for (auto bidx = 0; bidx < pi->num_counter_blocks; ++bidx) {
        const auto bi = &pi->counter_blocks[bidx];

        auto blk = &blocks[bidx];
        blk->pipeline = pi;
        blk->info = bi;
        blk->bspec = &bspecs[bidx];

        switch (bi->type) {
        case SNP4_INFO_COUNTER_TYPE_PACKETS_AND_BYTES:
            blk->nmetrics = 2;
            break;

        case SNP4_INFO_COUNTER_TYPE_PACKETS:
        case SNP4_INFO_COUNTER_TYPE_BYTES:
        case SNP4_INFO_COUNTER_TYPE_FLAG:
            blk->nmetrics = 1;
            break;
        }
        total_nmetrics += blk->nmetrics;

        blk->strings = new string[blk->nmetrics * COUNTER_NSTRINGS];
        stats->strings.push_back(blk->strings);
    }

    struct stats_metric_spec mspecs[total_nmetrics];
    memset(mspecs, 0, sizeof(mspecs));

    struct stats_label_spec labels[total_nmetrics * COUNTER_NLABELS];
    memset(labels, 0, sizeof(labels));

    auto mspecs_base = mspecs;
    auto labels_base = labels;
    for (auto bidx = 0; bidx < pi->num_counter_blocks; ++bidx) {
        auto blk = &blocks[bidx];

        blk->mspecs = mspecs_base;
        mspecs_base += blk->nmetrics;

        blk->labels = labels_base;
        labels_base += blk->nmetrics * COUNTER_NLABELS;

        switch (blk->info->type) {
        case SNP4_INFO_COUNTER_TYPE_PACKETS_AND_BYTES:
            init_counters_block_metric(blk, 0, "packets");
            init_counters_block_metric(blk, 1, "bytes");
            break;

        case SNP4_INFO_COUNTER_TYPE_PACKETS:
            init_counters_block_metric(blk, 0, NULL);
            break;

        case SNP4_INFO_COUNTER_TYPE_BYTES:
            init_counters_block_metric(blk, 0, NULL);
            break;

        case SNP4_INFO_COUNTER_TYPE_FLAG:
            init_counters_block_metric(blk, 0, NULL);
            break;
        }

        auto io_data = new CountersIoData{
            .handle = pipeline->handle,
            .info = blk->info,
        };
        stats->io_data.push_back(io_data);

        blk->bspec->name = blk->info->name;
        blk->bspec->metrics = blk->mspecs;
        blk->bspec->nmetrics = blk->nmetrics;
        blk->bspec->latch_metrics = counters_latch_metrics;
        blk->bspec->read_metric = counters_read_metric;
        blk->bspec->io.data.ptr = io_data;
        blk->bspec->latch.data_size =
            sizeof(CountersLatchData) +
            blk->nmetrics * blk->info->num_counters * sizeof(uint64_t);
    }

    struct stats_zone_spec zspec = {
        .name = stats->zone_name.c_str(),
        .blocks = bspecs,
        .nblocks = pi->num_counter_blocks,
    };

    stats->zone = stats_zone_alloc(dev->stats.domains[DeviceStatsDomain::COUNTERS], &zspec);
    if (stats->zone == NULL) {
        cerr << "ERROR: Failed to alloc counter stats zone for pipeline ID " << pipeline->id
             << " on device " << dev->bus_id << "."  << endl;
        exit(EXIT_FAILURE);
    }

    pipeline->stats.counters = stats;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::deinit_counters(DevicePipeline* pipeline) {
    if (pipeline->stats.counters != NULL) {
        auto stats = pipeline->stats.counters;
        pipeline->stats.counters = NULL;

        stats_zone_free(stats->zone);

        while (!stats->strings.empty()) {
            auto strings = stats->strings.back();
            stats->strings.pop_back();
            delete[] strings;
        }

        while (!stats->io_data.empty()) {
            auto io_data = (CountersIoData*)stats->io_data.back();
            stats->io_data.pop_back();
            delete io_data;
        }

        delete stats;
    }
}
