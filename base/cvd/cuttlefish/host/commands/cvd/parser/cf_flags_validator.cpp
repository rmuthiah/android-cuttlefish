/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "host/commands/cvd/parser/cf_flags_validator.h"

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "json/json.h"

#include "common/libs/utils/files.h"
#include "common/libs/utils/flags_validator.h"
#include "common/libs/utils/result.h"
#include "host/commands/cvd/parser/cf_configs_common.h"

namespace cuttlefish {
namespace {

using Json::ValueType::arrayValue;
using Json::ValueType::booleanValue;
using Json::ValueType::intValue;
using Json::ValueType::objectValue;
using Json::ValueType::stringValue;
using Json::ValueType::uintValue;

const auto& kRoot = *new ConfigNode{.type = objectValue, .children = {
  {"netsim_bt", ConfigNode{.type = booleanValue}},
  {"netsim_uwb", ConfigNode{.type = booleanValue}},
  {"instances", ConfigNode{.type = arrayValue, .children = {
    {kArrayValidationSentinel, ConfigNode{.type = objectValue, .children = {
        {"@import", ConfigNode{.type = stringValue}},
        {"name", ConfigNode{.type = stringValue}},
        {"vm", ConfigNode{.proto_name = "cuttlefish.cvd.config.Vm"}},
        {"boot", ConfigNode{.proto_name = "cuttlefish.cvd.config.Boot"}},
        {"security", ConfigNode{.proto_name = "cuttlefish.cvd.config.Security"}},
        {"disk", ConfigNode{.proto_name = "cuttlefish.cvd.config.Disk"}},
        {"graphics", ConfigNode{.proto_name = "cuttlefish.cvd.config.Graphics"}},
        {"streaming", ConfigNode{.type = objectValue, .children = {
          {"device_id", ConfigNode{.type = stringValue}},
        }}},
        {"connectivity", ConfigNode{.proto_name = "cuttlefish.cvd.config.Connectivity"}}
      }}},
    }}},
  {"fetch", ConfigNode{.proto_name = "cuttlefish.cvd.config.Fetch"}},
  {"metrics", ConfigNode{.proto_name = "cuttlefish.cvd.config.Metrics"}},
  {"common", ConfigNode{.type = objectValue, .children = {
    {"group_name", ConfigNode{.type = stringValue}},
    {"host_package", ConfigNode{.type = stringValue}},
  }}},
},
};

}  // namespace

Result<void> ValidateCfConfigs(const Json::Value& root) {
  static const auto& kSupportedImportValues =
      *new std::unordered_set<std::string>{"phone", "tablet", "tv", "wearable",
                                           "auto",  "slim",   "go", "foldable"};

  CF_EXPECT(Validate(root, kRoot), "Validation failure in [root object] ->");
  for (const auto& instance : root["instances"]) {
    // TODO(chadreynolds): update `ExtractLaunchTemplates` to return a Result
    // and check import values there, then remove this check
    if (instance.isMember("@import")) {
      const std::string import_value = instance["@import"].asString();
      CF_EXPECTF(kSupportedImportValues.find(import_value) !=
                     kSupportedImportValues.end(),
                 "import value of \"{}\" is not supported", import_value);
    }
    CF_EXPECT(ValidateConfig<std::string>(instance, ValidateSetupWizardMode,
                                          {"vm", "setupwizard_mode"}),
              "Invalid value for setupwizard_mode flag");
  }
  return {};
}

}  // namespace cuttlefish
