#include <iostream>
#include <assert.h>		// assert
#include <stdio.h>
#include <stdlib.h>		// exit, EXIT_FAILURE

#include <CLI/CLI.hpp>		// CLI

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/security/credentials.h>

#include "sn_p4_v1.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

class SmartnicP4Client {
public:
  SmartnicP4Client(std::shared_ptr<Channel> channel)
    : stub_(SmartnicP4::NewStub(channel)) {
  }

  bool DisplayPipelineInfo() {
    assert(pi_valid);

    std::cout << "Tables: " << pi.tables_size() << std::endl;
    for (auto &table : pi.tables()) {
      std::cout << "\t" << table.name() << std::endl;
      std::cout << "\t\tEndian: " << table_endian_to_string(table.endian()) << std::endl;
      std::cout << "\t\tMatches: " << table.match_specs_size() << std::endl;
      for (auto &match : table.match_specs()) {
	std::cout << "\t\t\t" << std::setw(3) << match.bits() << "   " << match_type_to_string(match.type()) << std::endl;
      }
      std::cout << "\t\tActions: " << table.action_specs_size() << std::endl;
      for (auto &action : table.action_specs()) {
	std::cout << "\t\t\t" << action.name() << ": Params: " << action.parameter_specs_size() << std::endl;
	for (auto &param : action.parameter_specs()) {
	  std::cout << "\t\t\t\t" <<  std::setw(3) << param.bits() << "   " << param.name() << std::endl;
	}
      }
      std::cout << "\t\tMasks: " << table.num_masks() << std::endl;
      std::cout << "\t\tPriority bits: " << table.priority_bits() << " (" << (table.priority_required() ? "Required" : "Optional") << ")" << std::endl;
    }

    return true;
  }

  bool GetPipelineInfo() {
    ClientContext context;
    Status status = stub_->GetPipelineInfo(&context, google::protobuf::Empty(), &pi);
    if (!status.ok()) {
      std::cout << status.error_code() << ": GetPipelineInfo rpc failed: " <<status.error_message() << std::endl;
      return false;
    }
    pi_valid = true;
    return true;
  }

  const PipelineInfo::TableInfo GetTableInfoByName(const std::string table_name, bool * found) {
    assert(pi_valid);

    auto table = std::find_if(pi.tables().begin(), pi.tables().end(),
			      [table_name](const auto& x){ return x.name() == table_name; });
    if (table != pi.tables().end()) {
      // Found the table
      *found = true;
    } else {
      *found = false;
    }
    return *table;
  }

  const PipelineInfo::TableInfo::ActionSpec GetTableActionInfoByName(const PipelineInfo::TableInfo& table, const std::string action_name, bool * found) {
    assert(pi_valid);

    auto action = std::find_if(table.action_specs().begin(), table.action_specs().end(),
			       [action_name](const auto& x){ return x.name() == action_name; });
    if (action != table.action_specs().end()) {
      // Found the action
      *found = true;
    } else {
      *found = false;
    }
    return *action;
  }

  bool ClearAllTables() {
    ClientContext context;
    ClearResponse clr_rsp;
    Status status = stub_->ClearAllTables(&context, google::protobuf::Empty(), &clr_rsp);
    if (!status.ok()) {
      std::cout << status.error_code() << ": ClearAllTables rpc failed: " <<status.error_message() << std::endl;
      return false;
    }

    if (clr_rsp.error_code() != 0) {
      std::cout << "ClearAllTables failed with error_code: " <<
	std::to_string(clr_rsp.error_code()) <<
	"(" << clr_rsp.error_detail() << ")" << std::endl;
      return false;
    }

    return true;
  }

  bool ClearOneTable(std::string table_name) {
    ClientContext context;
    ClearResponse clr_rsp;
    ClearOneTableRequest clr_one;

    bool found;
    auto table = GetTableInfoByName(table_name, &found);
    if (!found) {
      std::cout << "Invalid table name: " << table_name << std::endl;
      return false;
    }

    clr_one.set_table_name(table_name);

    Status status = stub_->ClearOneTable(&context, clr_one, &clr_rsp);
    if (!status.ok()) {
      std::cout << status.error_code() << ": ClearOneTable rpc failed: " <<status.error_message() << std::endl;
      return false;
    }

    if (clr_rsp.error_code() != 0) {
      std::cout << "ClearOneTable failed with error_code: " <<
	std::to_string(clr_rsp.error_code()) <<
	"(" << clr_rsp.error_detail() << ")" << std::endl;
      return false;
    }

    return true;
  }

  bool InsertRule(std::string table_name, std::vector<std::string> matches, std::string action_name, std::vector<std::string> params, uint32_t priority, bool replace) {
    MatchActionRule ma_rule;

    assert(pi_valid);

    bool found;
    auto table = GetTableInfoByName(table_name, &found);
    if (!found) {
      std::cout << "Invalid table name: " << table_name << std::endl;
      return false;
    }

    if (matches.size() != (unsigned)table.match_specs_size()) {
      std::cout << "Incorrect number of matches.  " << matches.size() << " provided, " << table.match_specs_size() << " required." << std::endl;
      return false;
    }

    auto action = GetTableActionInfoByName(table, action_name, &found);
    if (!found) {
      std::cout << "Invalid action name for table: " << action_name << std::endl;
      return false;
    }

    if (params.size() != (unsigned)action.parameter_specs_size()) {
      std::cout << "Incorrect number of parameters.  " << params.size() << " provided, " << action.parameter_specs_size() << " required." << std::endl;
      return false;
    }

    ma_rule.set_table_name(table_name);
    for (auto & match_str : matches) {
      auto match = ma_rule.add_matches();
      size_t p;
      if ((p = match_str.find("&&&")) != std::string::npos) {
	// key&&&mask format
	auto key = match_str.substr(0, p);
	auto mask = match_str.substr(p + 3);

	match->mutable_key_mask()->set_key(key);
	match->mutable_key_mask()->set_mask(mask);
      } else if ((p = match_str.find("/")) != std::string::npos) {
	// key/prefix format
	auto key = match_str.substr(0, p);
	auto prefix = match_str.substr(p + 1);
	auto prefix_len = atoi(prefix.c_str());

	match->mutable_prefix()->set_key(key);
	match->mutable_prefix()->set_prefix_len(prefix_len);
      } else if ((p = match_str.find("-")) != std::string::npos) {
	// lower-upper range format
	auto lower = atoi(match_str.substr(0, p).c_str());
	auto upper = atoi(match_str.substr(p + 1).c_str());

	match->mutable_range()->set_lower(lower);
	match->mutable_range()->set_upper(upper);
      } else {
	// key only format
	match->mutable_key_only()->set_key(match_str);
      }
    }

    ma_rule.set_action_name(action_name);

    for (auto & param_str : params) {
      auto param = ma_rule.add_params();
      param->set_value(param_str);
    }

    ma_rule.set_priority(priority);

    ma_rule.set_replace(replace);

    ClientContext context;
    RuleOperationResponse rsp;
    Status status = stub_->InsertRule(&context, ma_rule, &rsp);
    if (!status.ok()) {
      std::cout << status.error_code() << ": InsertRule rpc failed: " <<status.error_message() << std::endl;
      return false;
    }

    if (rsp.error_code() != 0) {
      std::cout << "InsertRule failed with error_code: " <<
	std::to_string(rsp.error_code()) <<
	"(" << rsp.error_detail() << ")" << std::endl;
      return false;
    }

    return true;
  }

  bool DeleteRule(std::string table_name, std::vector<std::string> matches) {
    MatchOnlyRule mo_rule;

    bool found;
    auto table = GetTableInfoByName(table_name, &found);
    if (!found) {
      std::cout << "Invalid table name: " << table_name << std::endl;
      return false;
    }

    if (matches.size() != (unsigned)table.match_specs_size()) {
      std::cout << "Incorrect number of matches.  " << matches.size() << " provided, " << table.match_specs_size() << " required." << std::endl;
      return false;
    }

    mo_rule.set_table_name(table_name);
    for (auto & match_str : matches) {
      auto match = mo_rule.add_matches();
      size_t p;
      if ((p = match_str.find("&&&")) != std::string::npos) {
	// key&&&mask format
	auto key = match_str.substr(0, p);
	auto mask = match_str.substr(p + 3);

	match->mutable_key_mask()->set_key(key);
	match->mutable_key_mask()->set_mask(mask);
      } else if ((p = match_str.find("/")) != std::string::npos) {
	// key/prefix format
	auto key = match_str.substr(0, p);
	auto prefix = match_str.substr(p + 1);
	auto prefix_len = atoi(prefix.c_str());

	match->mutable_prefix()->set_key(key);
	match->mutable_prefix()->set_prefix_len(prefix_len);
      } else if ((p = match_str.find("-")) != std::string::npos) {
	// lower-upper range format
	auto lower = atoi(match_str.substr(0, p).c_str());
	auto upper = atoi(match_str.substr(p + 1).c_str());

	match->mutable_range()->set_lower(lower);
	match->mutable_range()->set_upper(upper);
      } else {
	// key only format
	match->mutable_key_only()->set_key(match_str);
      }
    }

    ClientContext context;
    RuleOperationResponse rsp;
    Status status = stub_->DeleteRule(&context, mo_rule, &rsp);
    if (!status.ok()) {
      std::cout << status.error_code() << ": DeleteRule rpc failed: " <<status.error_message() << std::endl;
      return false;
    }

    if (rsp.error_code() != 0) {
      std::cout << "DeleteRule failed with error_code: " <<
	std::to_string(rsp.error_code()) <<
	"(" << rsp.error_detail() << ")" << std::endl;
      return false;
    }

    return true;
  }

private:
  std::unique_ptr<SmartnicP4::Stub> stub_;
  bool pi_valid = false;
  PipelineInfo pi;

  const std::string table_endian_to_string(PipelineInfo::TableInfo::Endian e) {
    switch (e) {
    case PipelineInfo_TableInfo_Endian_LITTLE: return "Little";
    case PipelineInfo_TableInfo_Endian_BIG: return "Big";
    case PipelineInfo_TableInfo_Endian_ENDIAN_UNSPECIFIED: return "Unspecified";
    default: return "Invalid";
    }
  }

  const std::string match_type_to_string(PipelineInfo::TableInfo::MatchSpec::MatchType t) {
    switch (t) {
    case PipelineInfo_TableInfo_MatchSpec_MatchType_BITFIELD: return "Bitfield / field_mask";
    case PipelineInfo_TableInfo_MatchSpec_MatchType_CONSTANT: return "Constant / exact";
    case PipelineInfo_TableInfo_MatchSpec_MatchType_PREFIX:   return "Prefix   / lpm";
    case PipelineInfo_TableInfo_MatchSpec_MatchType_RANGE:    return "Range    / range";
    case PipelineInfo_TableInfo_MatchSpec_MatchType_TERNARY:  return "Ternary  / ternary";
    case PipelineInfo_TableInfo_MatchSpec_MatchType_UNUSED:   return "Unused   / unused";
    case PipelineInfo_TableInfo_MatchSpec_MatchType_MATCH_TYPE_UNSPECIFIED: return "Unspecified";
    default: return "Invalid";
    }
  }

};

int main(int argc, char* argv[]) {
  CLI::App app{"Smartnic P4 Client"};

  std::string server = "localhost";
  app.add_option("--server", server, "The name of the server to connect to");

  uint16_t port = 50051;
  app.add_option("--port", port, "The port number to connect to");

  CLI::App* info = app.add_subcommand("info", "Display pipeline information obtained from the server");
  CLI::App* clear_all = app.add_subcommand("clear-all", "Clear the contents of all tables");

  std::string table_name;
  CLI::App* table_clear = app.add_subcommand("table-clear", "Clear the contents of the specified table");
  table_clear->add_option("table", table_name, "Name of the table to operate on")->required();

  std::vector<std::string> match_strings;
  CLI::App* table_delete = app.add_subcommand("table-delete", "Delete a single rule from the specified table");
  table_delete->add_option("table", table_name, "Name of the table to operate on")->required();
  table_delete->add_option("--match", match_strings, "Key and value for an individual key field")->required()->delimiter(' ');

  std::string action_name;
  std::vector<std::string> param_strings;
  uint32_t priority;
  CLI::App* table_insert = app.add_subcommand("table-insert", "Insert a single rule into the specified table");
  table_insert->add_option("table", table_name, "Name of the table to operate on")->required();
  table_insert->add_option("action", action_name, "Name of the action to invoke")->required();
  table_insert->add_option("--match", match_strings, "One or more value-mask pairs used in the key")->required()->delimiter(' ');
  table_insert->add_option("--param", param_strings, "One or more values for action parameters")->required()->delimiter(' ');
  table_insert->add_option("--priority", priority, "Priority for the rule");

  CLI::App* table_update = app.add_subcommand("table-update", "Update a single rule in the specified table");
  table_update->add_option("table", table_name, "Name of the table to operate on")->required();
  table_update->add_option("action", action_name, "Name of the action to invoke")->required();
  table_update->add_option("--match", match_strings, "One or more value-mask pairs used in the key")->required()->delimiter(' ');
  table_update->add_option("--param", param_strings, "One or more values for action parameters")->required()->delimiter(' ');

  std::string file_name;
  CLI::App* p4bm_check = app.add_subcommand("p4bm-check", "Check the syntax of a p4bm simulator rules file");
  p4bm_check->add_option("file", file_name, "File to be checked");

  CLI::App* p4bm_apply = app.add_subcommand("p4bm-apply", "Apply the rules described in a p4bm simulator rules file");
  p4bm_apply->add_option("file", file_name, "File to be applied");

  // Require a subcommand
  app.require_subcommand(1,1);

  // Parse the command line options
  CLI11_PARSE(app, argc, argv);

  // Set up a channel to the remote p4 agent and load the pipeline specification from the agent
  // NOTE: Most methods in the SmartnicP4Client class depend on having an accurate pipeline spec loaded
  SmartnicP4Client snp4(grpc::CreateChannel(server + ":" + std::to_string(port), grpc::InsecureChannelCredentials()));
  if (!snp4.GetPipelineInfo()) {
    std::cout << "Error: Unable to load p4 pipeline structure from remote agent" << std::endl;
    std::exit(EXIT_FAILURE);
    return 1;
  }

  // Process the subcommand
  if (app.got_subcommand(info)) {
    snp4.DisplayPipelineInfo();
  } else if (app.got_subcommand(clear_all)) {
    snp4.ClearAllTables();
  } else if (app.got_subcommand(table_clear)) {
    snp4.ClearOneTable(table_name);
  } else if (app.got_subcommand(table_insert)) {
    snp4.InsertRule(table_name, match_strings, action_name, param_strings, priority, false);
  } else if (app.got_subcommand(table_update)) {
    snp4.InsertRule(table_name, match_strings, action_name, param_strings, priority, true);
  } else if (app.got_subcommand(table_delete)) {
    snp4.DeleteRule(table_name, match_strings);
  } else if (app.got_subcommand(p4bm_check)) {
    std::cout << "Not implemented yet" << std::endl;
  } else if (app.got_subcommand(p4bm_apply)) {
    std::cout << "Not implemented yet" << std::endl;
  } else {
    // Unexpected subcommand
    std::cout << "Unhandled subcommand" << std::endl;
    return 1;
  }

  return 0;
}
