/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "host/commands/cvd/server_command/serial_preset.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "common/libs/utils/result.h"
#include "cuttlefish/host/commands/cvd/cvd_server.pb.h"
#include "host/commands/cvd/server_client.h"
#include "host/commands/cvd/server_command/server_handler.h"
#include "host/commands/cvd/server_command/utils.h"
#include "host/commands/cvd/types.h"

// file copied from "../demo_multi_vd.cpp"
namespace cuttlefish {

class SerialPreset : public CvdServerHandler {
 public:
  SerialPreset(CommandSequenceExecutor& executor) : executor_(executor) {}
  ~SerialPreset() = default;

  Result<bool> CanHandle(const RequestWithStdio& request) const override {
    auto invocation = ParseInvocation(request.Message());
    return invocation.command == "experimental" &&
           invocation.arguments.size() >= 1 &&
           Presets().count(invocation.arguments[0]) > 0;
  }

  Result<cvd::Response> Handle(const RequestWithStdio& request) override {
    bool can_handle_request = CF_EXPECT(CanHandle(request));
    CF_EXPECT_EQ(can_handle_request, true);

    auto invocation = ParseInvocation(request.Message());
    CF_EXPECT(invocation.arguments.size() >= 1);
    const auto& presets = Presets();
    auto devices = presets.find(invocation.arguments[0]);
    CF_EXPECT(devices != presets.end(), "could not find preset");

    cvd::Request inner_req_proto = request.Message();
    auto& cmd = *inner_req_proto.mutable_command_request();
    cmd.clear_args();
    cmd.add_args("cvd");
    cmd.add_args("experimental");
    cmd.add_args("serial_launch");
    for (const auto& device : devices->second) {
      cmd.add_args("--device=" + device);
    }
    for (std::size_t i = 1; i < invocation.arguments.size(); i++) {
      cmd.add_args(invocation.arguments[i]);
    }

    RequestWithStdio inner_request =
        RequestWithStdio::InheritIo(std::move(inner_req_proto), request);

    CF_EXPECT(executor_.Execute({std::move(inner_request)}, request.Err()));

    cvd::Response response;
    response.mutable_command_response();
    return response;
  }

  cvd_common::Args CmdList() const override { return {"experimental"}; }
  // not intended to show up in help
  Result<std::string> SummaryHelp() const override { return ""; }
  bool ShouldInterceptHelp() const override { return false; }
  Result<std::string> DetailedHelp(std::vector<std::string>&) const override {
    return "";
  }

 private:
  CommandSequenceExecutor& executor_;

  static std::unordered_map<std::string, std::vector<std::string>> Presets() {
    return {
        {"create_phone_tablet",
         {"git_master/cf_x86_64_phone-userdebug",
          "git_master/cf_x86_64_tablet-userdebug"}},
        {"create_phone_wear",
         {"git_master/cf_x86_64_phone-userdebug", "git_master/cf_gwear_x86"}},
    };
  }
};

std::unique_ptr<CvdServerHandler> NewSerialPreset(
    CommandSequenceExecutor& executor) {
  return std::unique_ptr<CvdServerHandler>(new SerialPreset(executor));
}

}  // namespace cuttlefish
