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

#pragma once

#include <functional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <google/protobuf/message.h>
#include "json/json.h"

#include "common/libs/utils/json.h"
#include "common/libs/utils/result.h"
#include "cuttlefish/host/commands/cvd/parser/load_config.pb.h"

namespace cuttlefish {

template <typename T>
Result<void> ValidateConfig(const Json::Value& instance,
                            std::function<Result<void>(const T&)> validator,
                            const std::vector<std::string>& selectors) {
  const int size = selectors.size();
  CF_EXPECT(size > 0, "No keys given for initializing config");
  auto result = GetValue<T>(instance, selectors);
  if (!result.ok()) {
    // Field isn't present, nothing to validate
    return {};
  }
  auto flag_value = *result;
  CF_EXPECTF(validator(flag_value), "Invalid flag value \"{}\"", flag_value);
  return {};
}

template <typename T>
Result<void> InitConfig(Json::Value& root, const T& default_value,
                        const std::vector<std::string>& selectors) {
  const int size = selectors.size();
  CF_EXPECT(size > 0, "No keys given for initializing config");
  int i = 0;
  Json::Value* traverse = &root;
  for (const auto& selector : selectors) {
    if (!traverse->isMember(selector)) {
      if (i == size - 1) {
        (*traverse)[selector] = default_value;
      } else {
        (*traverse)[selector] = Json::Value();
      }
    }
    traverse = &(*traverse)[selector];
    ++i;
  }
  return {};
}

void InitIntConfigSubGroupVector(Json::Value& instances,
                                 const std::string& group,
                                 const std::string& subgroup,
                                 const std::string& json_flag,
                                 int default_value);

template <typename T>
std::string GenerateFlag(const std::string& name, const T& value) {
  return fmt::format("--{}={}", name, value);
}

template <typename T>
std::string GenerateVecFlag(const std::string& name, const T& collection) {
  return fmt::format("--{}={}", name, fmt::join(collection, ","));
}

Result<std::string> GenerateVecFlagFromJson(
    const Json::Value& instances, const std::string& flag_name,
    const std::vector<std::string>& selectors);
Result<std::string> Base64EncodeGflag(
    const Json::Value& instances, const std::string& gflag_name,
    const std::vector<std::string>& selectors);

template <typename T>
std::string GenerateInstanceFlag(const std::string& name,
                                 const cvd::config::Launch& config,
                                 T callback) {
  std::vector<decltype(callback(config.instances()[0]))> values;
  for (const auto& instance : config.instances()) {
    values.emplace_back(callback(instance));
  }
  return GenerateVecFlag(name, values);
}

std::vector<std::string> MergeResults(std::vector<std::string> first_list,
                                      std::vector<std::string> scond_list);

void MergeTwoJsonObjs(Json::Value& dst, const Json::Value& src);

Result<void> Validate(const Json::Value& value, google::protobuf::Message& node);

}  // namespace cuttlefish
