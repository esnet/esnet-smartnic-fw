#include <iostream>
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

  bool GetPipelineInfo() {
    ClientContext context;
    PipelineInfo pi;
    Status status = stub_->GetPipelineInfo(&context, google::protobuf::Empty(), &pi);
    if (!status.ok()) {
      std::cout << status.error_code() << ": GetPipelineInfo rpc failed: " <<status.error_message() << std::endl;
      return false;
    }

    std::cout << "Number of tables: " << pi.tables_size() << std::endl;
    for (auto &table : pi.tables()) {
      std::cout << "\tName: " << table.name() << std::endl;
    }

    return true;
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
  PipelineInfo pi;
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
  table_delete->add_option("--match", match_strings, "Key and value for an individual key field")->required()->delimiter(',');

  std::string action_name;
  std::vector<std::string> param_strings;
  uint32_t priority;
  CLI::App* table_insert = app.add_subcommand("table-insert", "Insert a single rule into the specified table");
  table_insert->add_option("table", table_name, "Name of the table to operate on")->required();
  table_insert->add_option("action", action_name, "Name of the action to invoke")->required();
  table_insert->add_option("--match", match_strings, "One or more value-mask pairs used in the key")->required()->delimiter(',');
  table_insert->add_option("--param", param_strings, "One or more values for action parameters")->required()->delimiter(',');
  table_insert->add_option("--priority", priority, "Priority for the rule");

  CLI::App* table_update = app.add_subcommand("table-update", "Update a single rule in the specified table");
  table_update->add_option("table", table_name, "Name of the table to operate on")->required();
  table_update->add_option("action", action_name, "Name of the action to invoke")->required();
  table_update->add_option("--match", match_strings, "One or more value-mask pairs used in the key")->required()->delimiter(',');
  table_update->add_option("--param", param_strings, "One or more values for action parameters")->required()->delimiter(',');

  std::string file_name;
  CLI::App* p4bm_check = app.add_subcommand("p4bm-check", "Check the syntax of a p4bm simulator rules file");
  p4bm_check->add_option("file", file_name, "File to be checked");

  CLI::App* p4bm_apply = app.add_subcommand("p4bm-apply", "Apply the rules described in a p4bm simulator rules file");
  p4bm_apply->add_option("file", file_name, "File to be applied");

  // Require a subcommand
  app.require_subcommand(1,1);

  CLI11_PARSE(app, argc, argv);

  SmartnicP4Client snp4(grpc::CreateChannel(server + ":" + std::to_string(port), grpc::InsecureChannelCredentials()));

  if (app.got_subcommand(info)) {
    std::cout << "------ Get PipelineInfo -------" << std::endl;
    snp4.GetPipelineInfo();
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
