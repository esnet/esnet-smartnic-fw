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
    struct TableEccIoData {
        void* handle;
        const char* table_name;
    };

    enum TableEccCounterId {
        CORRECTED_SINGLE_BIT_ERRORS,
        DETECTED_DOUBLE_BIT_ERRORS,
    };

    struct TableEccLatchData {
        bool valid;
        uint32_t corrected_single_bit_errors;
        uint32_t detected_double_bit_errors;
    };

    static void table_ecc_latch_metrics(const struct stats_block_spec* bspec, void* data) {
        TableEccIoData* iod = (typeof(iod))bspec->io.data.ptr;
        TableEccLatchData* ld = (typeof(ld))data;

        ld->valid = snp4_table_ecc_counters_read(
            iod->handle, iod->table_name,
            &ld->corrected_single_bit_errors,
            &ld->detected_double_bit_errors);
    }

    static void table_ecc_read_metric([[maybe_unused]] const struct stats_block_spec* bspec,
                                      const struct stats_metric_spec* mspec,
                                      uint64_t* value,
                                      void* data) {
        TableEccLatchData* ld = (typeof(ld))data;
        if (!ld->valid) {
            *value = 0;
            return;
        }

        switch ((TableEccCounterId)mspec->io.offset) {
        case TableEccCounterId::CORRECTED_SINGLE_BIT_ERRORS:
            *value = ld->corrected_single_bit_errors;
            break;

        case TableEccCounterId::DETECTED_DOUBLE_BIT_ERRORS:
            *value = ld->detected_double_bit_errors;
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
#define TABLE_ECC_NCOUNTERS 2
#define TABLE_ECC_NLABELS 2

struct InitTableEcc {
    const struct snp4_info_pipeline* pipeline;
    const struct snp4_info_table* info;
    struct stats_block_spec* bspec;
    struct stats_label_spec* labels;
    struct stats_metric_spec* mspecs;
};

static void init_table_ecc_metric(InitTableEcc* tecc, TableEccCounterId cid) {
    auto mspec = &tecc->mspecs[cid];
    auto labels = &tecc->labels[cid * TABLE_ECC_NLABELS];

    switch (cid) {
    case TableEccCounterId::CORRECTED_SINGLE_BIT_ERRORS:
        mspec->name = "corrected_single_bit_errors";
        break;

    case TableEccCounterId::DETECTED_DOUBLE_BIT_ERRORS:
        mspec->name = "detected_double_bit_errors";
        break;
    }

    mspec->labels = labels;
    mspec->nlabels = TABLE_ECC_NLABELS;
    mspec->type = stats_metric_type_COUNTER;
    mspec->flags = STATS_METRIC_FLAG_MASK(CLEAR_ON_READ);
    mspec->io.offset = cid;

    labels->key = "pipeline";
    labels->value = tecc->pipeline->name;
    labels += 1;

    labels->key = "mode";
    switch (tecc->info->mode) {
#define CASE_MODE(_mode) case SNP4_INFO_TABLE_MODE_##_mode: labels->value = #_mode; break
        CASE_MODE(BCAM);
        CASE_MODE(STCAM);
        CASE_MODE(TCAM);
        CASE_MODE(DCAM);
        CASE_MODE(TINY_BCAM);
        CASE_MODE(TINY_TCAM);
#undef CASE_MODE
    }
    labels += 1;
}

//--------------------------------------------------------------------------------------------------
static bool is_table_ecc_supported(enum snp4_info_table_mode mode) {
    switch (mode) {
    case SNP4_INFO_TABLE_MODE_BCAM:
    case SNP4_INFO_TABLE_MODE_STCAM:
    case SNP4_INFO_TABLE_MODE_TCAM:
    case SNP4_INFO_TABLE_MODE_TINY_BCAM:
    case SNP4_INFO_TABLE_MODE_TINY_TCAM:
        return true;

    // ECC counters not supported.
    case SNP4_INFO_TABLE_MODE_DCAM:
        break;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::init_table_ecc(Device* dev, DevicePipeline* pipeline) {
    const auto pi = &pipeline->info;
    unsigned int total_ntables = 0;
    for (auto ti = pi->tables; ti < &pi->tables[pi->num_tables]; ++ti) {
        if (is_table_ecc_supported(ti->mode)) {
            total_ntables += 1;
        }
    }

    SERVER_LOG_LINE_INIT(table_ecc, INFO,
        "Initializing " << total_ntables << " table ECC counters of pipeline ID " <<
        pipeline->id << " on device " << dev->bus_id);
    if (total_ntables == 0) {
        pipeline->stats.table_ecc = NULL;
        return;
    }

    ostringstream zone_name;
    zone_name << "pipeline" << pipeline->id;

    auto stats = new DeviceStats;
    stats->zone_name = zone_name.str();

    InitTableEcc table_ecc[total_ntables];
    memset(table_ecc, 0, sizeof(table_ecc));

    struct stats_block_spec bspecs[total_ntables];
    memset(bspecs, 0, sizeof(bspecs));

    unsigned int total_nmetrics = total_ntables * TABLE_ECC_NCOUNTERS;
    struct stats_metric_spec mspecs[total_nmetrics];
    memset(mspecs, 0, sizeof(mspecs));

    struct stats_label_spec labels[total_nmetrics * TABLE_ECC_NLABELS];
    memset(labels, 0, sizeof(labels));

    auto mspecs_base = mspecs;
    auto labels_base = labels;
    unsigned int tidx = 0;
    for (auto ti = pi->tables; ti < &pi->tables[pi->num_tables]; ++ti) {
        if (!is_table_ecc_supported(ti->mode)) {
            continue;
        }

        auto tecc = &table_ecc[tidx];
        tecc->pipeline = pi;
        tecc->info = ti;
        tecc->bspec = &bspecs[tidx];

        tecc->mspecs = mspecs_base;
        mspecs_base += TABLE_ECC_NCOUNTERS;

        tecc->labels = labels_base;
        labels_base += TABLE_ECC_NCOUNTERS * TABLE_ECC_NLABELS;

        init_table_ecc_metric(tecc, TableEccCounterId::CORRECTED_SINGLE_BIT_ERRORS);
        init_table_ecc_metric(tecc, TableEccCounterId::DETECTED_DOUBLE_BIT_ERRORS);

        auto io_data = new TableEccIoData{
            .handle = pipeline->handle,
            .table_name = ti->name,
        };
        stats->io_data.push_back(io_data);

        tecc->bspec->name = ti->name;
        tecc->bspec->metrics = tecc->mspecs;
        tecc->bspec->nmetrics = TABLE_ECC_NCOUNTERS;
        tecc->bspec->latch_metrics = table_ecc_latch_metrics;
        tecc->bspec->read_metric = table_ecc_read_metric;
        tecc->bspec->io.data.ptr = io_data;
        tecc->bspec->latch.data_size = sizeof(TableEccLatchData);

        tidx += 1;

        SERVER_LOG_LINE_INIT(table_ecc, INFO,
            "Setup for ECC counters on table '" << ti->name << "' of pipeline ID " <<
            pipeline->id << " on device " << dev->bus_id);
    }

    struct stats_zone_spec zspec = {
        .name = stats->zone_name.c_str(),
        .blocks = bspecs,
        .nblocks = total_ntables,
    };

    stats->zone = stats_zone_alloc(dev->stats.domains[DeviceStatsDomain::COUNTERS], &zspec);
    if (stats->zone == NULL) {
        SERVER_LOG_LINE_INIT(table_ecc, ERROR,
            "Failed to alloc table ECC stats zone for pipeline ID " << pipeline->id <<
            " on device " << dev->bus_id);
        exit(EXIT_FAILURE);
    }

    pipeline->stats.table_ecc = stats;
    SERVER_LOG_LINE_INIT(table_ecc, INFO,
        "Completed init for table ECC counters of pipeline ID " << pipeline->id <<
        " on device " << dev->bus_id);
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::deinit_table_ecc(DevicePipeline* pipeline) {
    if (pipeline->stats.table_ecc != NULL) {
        auto stats = pipeline->stats.table_ecc;
        pipeline->stats.table_ecc = NULL;

        stats_zone_free(stats->zone);

        while (!stats->io_data.empty()) {
            auto io_data = (TableEccIoData*)stats->io_data.back();
            stats->io_data.pop_back();
            delete io_data;
        }

        delete stats;
    }
}
