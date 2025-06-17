#include "agent.hpp"
#include "device.hpp"

#include <cstdlib>
#include <gmp.h>

#include <grpc/grpc.h>
#include "sn_p4_v2.grpc.pb.h"

using namespace grpc;
using namespace sn_p4::v2;
using namespace std;

//--------------------------------------------------------------------------------------------------
static const struct snp4_info_register_block*
register_block_get_info(const DevicePipeline* pipeline, const string& block_name) {
    const auto pi = &pipeline->info;
    for (auto idx = 0; idx < pi->num_register_blocks; ++idx) {
        const auto rb = &pi->register_blocks[idx];
        if (rb->name == block_name) {
            return rb;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
ErrorCode SmartnicP4Impl::register_block_get_index(const struct snp4_info_register_block* binfo,
                                                   const RegisterId& id,
                                                   unsigned int* index,
                                                   ServerDebugFlag debug_flag) {
    const auto alias = id.alias();
    if (!alias.empty()) {
        bool matched = false;
        for (unsigned int idx = 0; idx < binfo->num_aliases; ++idx) {
            if (binfo->aliases[idx] != NULL && alias == binfo->aliases[idx]) {
                *index = idx;
                matched = true;
                break;
            }
        }

        if (!matched) {
            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                "Invalid register alias '" << alias <<
                "' for block '" << binfo->name << "'");
            return ErrorCode::EC_INVALID_REGISTER_ALIAS;
        }
    } else {
        *index = id.index();
        if (*index >= binfo->num_registers) {
            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                "Register index " << id.index() <<
                " out of range for block '" << binfo->name << "'");
            return ErrorCode::EC_REGISTER_INDEX_OUT_OF_RANGE;
        }
    }

    return ErrorCode::EC_OK;
}

//--------------------------------------------------------------------------------------------------
static ErrorCode register_decode_value(const struct snp4_info_register_block* binfo,
                                       const string& value,
                                       mpz_t& data) {
    if (mpz_set_str(data, value.c_str(), 0) != 0) {
        return ErrorCode::EC_REGISTER_VALUE_INVALID_FORMAT;
    }

    size_t n_bits = mpz_sizeinbase(data, 2);
    if (n_bits > binfo->width) {
        return ErrorCode::EC_REGISTER_VALUE_EXCEEDS_BIT_WIDTH;
    }

    return ErrorCode::EC_OK;
}

//--------------------------------------------------------------------------------------------------
static void register_encode_value(string& value, const mpz_t& data) {
    char* str = mpz_get_str(NULL, 16, data);
    value = "0x";
    value += str;
    free(str);
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::clear_registers(
    const RegistersRequest& req,
    function<void(const RegistersResponse&)> write_resp) {
    const auto debug_flag = ServerDebugFlag::DEBUG_FLAG_REGISTERS;
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        RegistersResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);

        SERVER_LOG_IF_DEBUG(debug_flag, ERROR, "Invalid device ID " << dev_id);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    SERVER_LOG_IF_DEBUG(debug_flag, INFO,
        "---> Clear Request:" << endl << req.DebugString());

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_pipeline_id = 0;
        int end_pipeline_id = dev->pipelines.size() - 1;
        int pipeline_id = req.pipeline_id(); // 0-based index. -1 means all pipelines.
        if (pipeline_id > end_pipeline_id) {
            RegistersResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PIPELINE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);

            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                "Invalid pipeline ID " << pipeline_id << " on device ID " << dev_id);
            continue;
        }

        if (pipeline_id > -1) {
            begin_pipeline_id = pipeline_id;
            end_pipeline_id = pipeline_id;
        }

        for (pipeline_id = begin_pipeline_id; pipeline_id <= end_pipeline_id; ++pipeline_id) {
            const auto pipeline = dev->pipelines[pipeline_id];
            RegistersResponse resp;
            auto err = ErrorCode::EC_OK;

            size_t num_slices = req.slices_size();
            if (num_slices == 0) { // Empty slice list means clear all blocks.
                for (auto idx = 0; idx < pipeline->info.num_register_blocks; ++idx) {
                    const auto rb = &pipeline->info.register_blocks[idx];
                    if (!snp4_register_block_reset(pipeline->handle, rb->name)) {
                        err = ErrorCode::EC_FAILED_CLEAR_REGISTER;
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Failed to clear all registers of block '" << rb->name <<
                            "' [block " << idx+1 << "/" << pipeline->info.num_register_blocks <<
                            "] in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        break;
                    }
                }
            } else { // Non-empty slice list means only clear selected blocks.
                unsigned int slice_idx = 1;
                for (auto slice : req.slices()) {
                    const auto first_id = slice.first_id();
                    const auto block_name = first_id.block_name();
                    const auto binfo = register_block_get_info(pipeline, block_name);
                    if (binfo == NULL) {
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Invalid register block name '" << block_name <<
                            "' [slice " << slice_idx << "/" << num_slices <<
                            "] in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        err = ErrorCode::EC_INVALID_REGISTER_BLOCK_NAME;
                        break;
                    }

                    unsigned int index = 0;
                    err = register_block_get_index(binfo, first_id, &index, debug_flag);
                    if (err != ErrorCode::EC_OK) {
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Failed to determine register index in block '" << block_name <<
                            "' [slice " << slice_idx << "/" << num_slices <<
                            "] in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        break;
                    }

                    size_t count = slice.count();
                    if (count == 0) {
                        count = binfo->num_registers - index;
                    }

                    if (!snp4_register_reset(pipeline->handle, block_name.c_str(), index, count)) {
                        err = ErrorCode::EC_FAILED_CLEAR_REGISTER;
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Failed to clear " << count <<
                            " registers from index " << index <<
                            " of block '" << block_name <<
                            "' [slice " << slice_idx << "/" << num_slices <<
                            "] in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        break;
                    }

                    SERVER_LOG_IF_DEBUG(debug_flag, INFO,
                        "Cleared " << count <<
                        " registers from index " << index <<
                        " of block '" << block_name <<
                        "' [slice " << slice_idx << "/" << num_slices <<
                        "] in pipeline ID " << pipeline_id <<
                        " on device ID " << dev_id);
                    slice_idx += 1;
                }
            }

            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_pipeline_id(pipeline_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_clear_registers(
    const RegistersRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    clear_registers(req, [&rdwr](const RegistersResponse& resp) -> void {
        BatchResponse bresp;
        auto reg = bresp.mutable_registers();
        reg->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_CLEAR);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::ClearRegisters(
    [[maybe_unused]] ServerContext* ctx,
    const RegistersRequest* req,
    ServerWriter<RegistersResponse>* writer) {
    clear_registers(*req, [&writer](const RegistersResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}


//--------------------------------------------------------------------------------------------------
static ErrorCode read_registers(
    void* snp4_handle,
    const struct snp4_info_register_block* binfo,
    unsigned int index,
    size_t count,
    bool do_clear,
    bool non_zero,
    RegistersResponse& resp) {
    auto err = ErrorCode::EC_OK;

    mpz_t values[count];
    for (unsigned int n = 0; n < count; ++n) {
        mpz_init(values[n]);
    }

    if (!snp4_register_read(snp4_handle, binfo->name, index, count, values)) {
        err = ErrorCode::EC_FAILED_READ_REGISTER;
        goto free_values;
    }

    if (do_clear && !snp4_register_reset(snp4_handle, binfo->name, index, count)) {
        err = ErrorCode::EC_FAILED_READ_AND_CLEAR_REGISTER;
        goto free_values;
    }

    {
        auto s = resp.add_slices();
        s->set_count(count);

        auto fid = s->mutable_first_id();
        fid->set_block_name(binfo->name);
        fid->set_index(index);
        if (index < binfo->num_aliases && binfo->aliases[index] != NULL) {
            fid->set_alias(binfo->aliases[index]);
        }

        mpz_t zero;
        mpz_init_set_ui(zero, 0);
        for (unsigned int n = 0; n < count; ++n) {
            if (non_zero && mpz_cmp(values[n], zero) == 0) {
                continue;
            }

            unsigned int idx = index + n;
            auto r = s->add_registers();

            auto rid = r->mutable_id();
            rid->set_block_name(binfo->name);
            rid->set_index(idx);
            if (idx < binfo->num_aliases && binfo->aliases[idx] != NULL) {
                rid->set_alias(binfo->aliases[idx]);
            }

            auto v = r->mutable_value();
            register_encode_value(*v, values[n]);
        }
        mpz_clear(zero);
    }

free_values:
    for (unsigned int n = 0; n < count; ++n) {
        mpz_clear(values[n]);
    }

    return err;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::get_registers(
    const RegistersRequest& req,
    function<void(const RegistersResponse&)> write_resp) {
    auto debug_flag = ServerDebugFlag::DEBUG_FLAG_REGISTERS;
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        RegistersResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);

        SERVER_LOG_IF_DEBUG(debug_flag, ERROR, "Invalid device ID " << dev_id);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    SERVER_LOG_IF_DEBUG(debug_flag, INFO,
        "---> Get Request:" << endl << req.DebugString());

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_pipeline_id = 0;
        int end_pipeline_id = dev->pipelines.size() - 1;
        int pipeline_id = req.pipeline_id(); // 0-based index. -1 means all pipelines.
        if (pipeline_id > end_pipeline_id) {
            RegistersResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PIPELINE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);

            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                "Invalid pipeline ID " << pipeline_id << " on device ID " << dev_id);
            continue;
        }

        if (pipeline_id > -1) {
            begin_pipeline_id = pipeline_id;
            end_pipeline_id = pipeline_id;
        }

        for (pipeline_id = begin_pipeline_id; pipeline_id <= end_pipeline_id; ++pipeline_id) {
            const auto pipeline = dev->pipelines[pipeline_id];
            RegistersResponse resp;
            auto err = ErrorCode::EC_OK;

            bool do_clear = req.clear_on_read();
            bool non_zero = req.non_zero();
            size_t num_slices = req.slices_size();
            if (num_slices == 0) { // Empty slice list means get all blocks.
                for (auto idx = 0; idx < pipeline->info.num_register_blocks; ++idx) {
                    const auto binfo = &pipeline->info.register_blocks[idx];
                    err = read_registers(pipeline->handle, binfo, 0, binfo->num_registers,
                                         do_clear, non_zero, resp);
                    if (err != ErrorCode::EC_OK) {
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Failed to read all registers of block '" << binfo->name <<
                            "' in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        break;
                    }

                    SERVER_LOG_IF_DEBUG(debug_flag, INFO,
                        "Read all " << binfo->num_registers <<
                        " registers from block '" << binfo->name <<
                        "' in pipeline ID " << pipeline_id <<
                        " on device ID " << dev_id);
                }
            } else { // Non-empty slice list means only clear selected blocks.
                unsigned int slice_idx = 1;
                for (auto slice : req.slices()) {
                    const auto first_id = slice.first_id();
                    const auto block_name = first_id.block_name();
                    const auto binfo = register_block_get_info(pipeline, block_name);
                    if (binfo == NULL) {
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Invalid register block name '" << block_name <<
                            "' [slice " << slice_idx << "/" << num_slices <<
                            "] in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        err = ErrorCode::EC_INVALID_REGISTER_BLOCK_NAME;
                        break;
                    }

                    unsigned int index = 0;
                    err = register_block_get_index(binfo, first_id, &index, debug_flag);
                    if (err != ErrorCode::EC_OK) {
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Failed to determine register index in block '" << block_name <<
                            "' [slice " << slice_idx << "/" << num_slices <<
                            "] in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        break;
                    }

                    size_t count = slice.count();
                    if (count == 0) {
                        count = binfo->num_registers - index;
                    }

                    err = read_registers(pipeline->handle, binfo, index, count,
                                         do_clear, non_zero, resp);
                    if (err != ErrorCode::EC_OK) {
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Failed to read " << count <<
                            " registers from index " << index <<
                            " of block '" << block_name <<
                            "' [slice " << slice_idx << "/" << num_slices <<
                            "] in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        break;
                    }

                    SERVER_LOG_IF_DEBUG(debug_flag, INFO,
                        "Read " << count <<
                        " registers from index " << index <<
                        " of block '" << block_name <<
                        "' [slice " << slice_idx << "/" << num_slices <<
                        "] in pipeline ID " << pipeline_id <<
                        " on device ID " << dev_id);
                    slice_idx += 1;
                }
            }

            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_pipeline_id(pipeline_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_get_registers(
    const RegistersRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    get_registers(req, [&rdwr](const RegistersResponse& resp) -> void {
        BatchResponse bresp;
        auto reg = bresp.mutable_registers();
        reg->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_GET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::GetRegisters(
    [[maybe_unused]] ServerContext* ctx,
    const RegistersRequest* req,
    ServerWriter<RegistersResponse>* writer) {
    get_registers(*req, [&writer](const RegistersResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::set_registers(
    const RegistersRequest& req,
    function<void(const RegistersResponse&)> write_resp) {
    auto debug_flag = ServerDebugFlag::DEBUG_FLAG_REGISTERS;
    int begin_dev_id = 0;
    int end_dev_id = devices.size() - 1;
    int dev_id = req.dev_id(); // 0-based index. -1 means all devices.

    if (dev_id > end_dev_id) {
        RegistersResponse resp;
        resp.set_error_code(ErrorCode::EC_INVALID_DEVICE_ID);
        write_resp(resp);

        SERVER_LOG_IF_DEBUG(debug_flag, ERROR, "Invalid device ID " << dev_id);
        return;
    }

    if (dev_id > -1) {
        begin_dev_id = dev_id;
        end_dev_id = dev_id;
    }

    SERVER_LOG_IF_DEBUG(debug_flag, INFO,
        "---> Set Request:" << endl << req.DebugString());

    for (dev_id = begin_dev_id; dev_id <= end_dev_id; ++dev_id) {
        const auto dev = devices[dev_id];

        int begin_pipeline_id = 0;
        int end_pipeline_id = dev->pipelines.size() - 1;
        int pipeline_id = req.pipeline_id(); // 0-based index. -1 means all pipelines.
        if (pipeline_id > end_pipeline_id) {
            RegistersResponse resp;
            resp.set_error_code(ErrorCode::EC_INVALID_PIPELINE_ID);
            resp.set_dev_id(dev_id);
            write_resp(resp);

            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                "Invalid pipeline ID " << pipeline_id << " on device ID " << dev_id);
            continue;
        }

        if (pipeline_id > -1) {
            begin_pipeline_id = pipeline_id;
            end_pipeline_id = pipeline_id;
        }

        for (pipeline_id = begin_pipeline_id; pipeline_id <= end_pipeline_id; ++pipeline_id) {
            const auto pipeline = dev->pipelines[pipeline_id];
            RegistersResponse resp;
            auto err = ErrorCode::EC_OK;

            size_t num_slices = req.slices_size();
            if (num_slices == 0) {
                SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                    "Missing register slices in pipeline ID " << pipeline_id <<
                    " on device ID " << dev_id);
                err = ErrorCode::EC_MISSING_REGISTER_SLICES;
            } else {
                unsigned int slice_idx = 1;
                for (auto slice : req.slices()) {
                    size_t num_regs = slice.registers_size();
                    if (num_regs == 0) {
                        SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                            "Missing registers [slice " << slice_idx << "/" << num_slices <<
                            "] in pipeline ID " << pipeline_id <<
                            " on device ID " << dev_id);
                        err = ErrorCode::EC_MISSING_REGISTERS_IN_SLICE;
                        break;
                    }

                    unsigned int reg_idx = 1;
                    for (auto reg : slice.registers()) {
                        const auto id = reg.id();
                        const auto block_name = id.block_name();
                        const auto binfo = register_block_get_info(pipeline, block_name);
                        if (binfo == NULL) {
                            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                                "Invalid register block name '" << block_name <<
                                "' [slice " << slice_idx << "/" << num_slices <<
                                ", reg " << reg_idx << "/" << num_regs <<
                                "] in pipeline ID " << pipeline_id <<
                                " on device ID " << dev_id);
                            err = ErrorCode::EC_INVALID_REGISTER_BLOCK_NAME;
                            break;
                        }

                        unsigned int index = 0;
                        err = register_block_get_index(binfo, id, &index, debug_flag);
                        if (err != ErrorCode::EC_OK) {
                            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                                "Failed to determine register index in block '" << block_name <<
                                "' [slice " << slice_idx << "/" << num_slices <<
                                ", reg " << reg_idx << "/" << num_regs <<
                                "] in pipeline ID " << pipeline_id <<
                                " on device ID " << dev_id);
                            break;
                        }

                        mpz_t value;
                        mpz_init(value);
                        err = register_decode_value(binfo, reg.value(), value);
                        if (err != ErrorCode::EC_OK) {
                            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                                "Failed to parse register value at index " << index <<
                                " for block '" << block_name <<
                                "' [slice " << slice_idx << "/" << num_slices <<
                                ", reg " << reg_idx << "/" << num_regs <<
                                "] in pipeline ID " << pipeline_id <<
                                " on device ID " << dev_id);
                        } else if (snp4_register_write(pipeline->handle, binfo->name,
                                                       index, 1, &value)) {
                            SERVER_LOG_IF_DEBUG(debug_flag, INFO,
                                "Wrote register at index " << index <<
                                " of block '" << block_name <<
                                "' [slice " << slice_idx << "/" << num_slices <<
                                ", reg " << reg_idx << "/" << num_regs <<
                                "] in pipeline ID " << pipeline_id <<
                                " on device ID " << dev_id);
                        } else {
                            SERVER_LOG_IF_DEBUG(debug_flag, ERROR,
                                "Failed to write register at index " << index <<
                                " of block '" << block_name <<
                                "' [slice " << slice_idx << "/" << num_slices <<
                                ", reg " << reg_idx << "/" << num_regs <<
                                "] in pipeline ID " << pipeline_id <<
                                " on device ID " << dev_id);
                            err = ErrorCode::EC_FAILED_WRITE_REGISTER;
                        }

                        mpz_clear(value);
                        if (err != ErrorCode::EC_OK) {
                            break;
                        }
                        reg_idx += 1;
                    }

                    if (err != ErrorCode::EC_OK) {
                        break;
                    }
                    slice_idx += 1;
                }
            }

            resp.set_error_code(err);
            resp.set_dev_id(dev_id);
            resp.set_pipeline_id(pipeline_id);

            write_resp(resp);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void SmartnicP4Impl::batch_set_registers(
    const RegistersRequest& req,
    ServerReaderWriter<BatchResponse, BatchRequest>* rdwr) {
    set_registers(req, [&rdwr](const RegistersResponse& resp) -> void {
        BatchResponse bresp;
        auto reg = bresp.mutable_registers();
        reg->CopyFrom(resp);
        bresp.set_error_code(ErrorCode::EC_OK);
        bresp.set_op(BatchOperation::BOP_SET);
        rdwr->Write(bresp);
    });
}

//--------------------------------------------------------------------------------------------------
Status SmartnicP4Impl::SetRegisters(
    [[maybe_unused]] ServerContext* ctx,
    const RegistersRequest* req,
    ServerWriter<RegistersResponse>* writer) {
    set_registers(*req, [&writer](const RegistersResponse& resp) -> void {
        writer->Write(resp);
    });
    return Status::OK;
}
