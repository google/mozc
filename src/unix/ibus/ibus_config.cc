// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "unix/ibus/ibus_config.h"

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/protobuf/text_format.h"
#include "base/system_util.h"
#include "unix/ibus/main.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

namespace mozc {

constexpr char kIbusConfigFile[] = "ibus_config.textproto";

namespace {
std::string UpdateConfigFile() {
  const std::string engines_file = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), kIbusConfigFile);
  if (FileUtil::FileExists(engines_file)) {
    InputFileStream ifs(engines_file.c_str());
    return ifs.Read();
  } else {
    OutputFileStream ofs(engines_file.c_str());
    ofs << kIbusConfigTextProto;
    ofs.close();
    if (ofs.fail()) {
      LOG(ERROR) << "Failed to write " << engines_file;
    }
    return kIbusConfigTextProto;
  }
}

bool ParseConfig(const std::string &data, ibus::Config &config) {
  if (mozc::protobuf::TextFormat::ParseFromString(data, &config)) {
    return true;
  }
  // Failed to parse the data, fallback to the default setting.
  mozc::protobuf::TextFormat::ParseFromString(kIbusConfigTextProto, &config);
  return false;
}

std::string EscapeXmlValue(const std::string &value) {
  return absl::StrReplaceAll(value, {{"&", "&amp;"},
                                     {"<", "&lt;"},
                                     {">", "&gt;"},
                                     {"\"", "&quot;"},
                                     {"'", "&apos;"}});
}

std::string CreateEnginesXml(const ibus::Config &config) {
  std::string output = "<engines>\n";
  for (const ibus::Engine &engine : config.engines()) {
    absl::StrAppend(
        &output,
        "<engine>\n",
        "  <description>", kEngineDescription, "</description>\n",
        "  <language>", kEngineLanguage, "</language>\n",
        "  <icon>", kEngineIcon, "</icon>\n",
        "  <rank>", engine.rank(), "</rank>\n",
        "  <icon_prop_key>", kEngineIcon_prop_key, "</icon_prop_key>\n",
        "  <symbol>", kEngineSymbol, "</symbol>\n",
        "  <setup>", kEngineSetup, "</setup>\n",
        "  <name>", EscapeXmlValue(engine.name()), "</name>\n",
        "  <longname>", EscapeXmlValue(engine.longname()), "</longname>\n",
        "  <layout>", EscapeXmlValue(engine.layout()), "</layout>\n",
        "  <layout_variant>", EscapeXmlValue(engine.layout_variant()),
        "</layout_variant>\n",
        "  <layout_option>", EscapeXmlValue(engine.layout_option()),
        "</layout_option>\n",
        "</engine>\n");
  }
  absl::StrAppend(&output, "</engines>\n");
  return output;
}
}  // namespace

bool IbusConfig::Initialize() {
  const std::string config_data = UpdateConfigFile();
  const bool valid_user_config = ParseConfig(config_data, config_);

  engine_xml_ = CreateEnginesXml(config_);
  if (!valid_user_config) {
    engine_xml_ += ("<!-- Failed to parse the user config. -->\n"
                    "<!-- Used the default setting instead. -->\n");
  }

  return valid_user_config;
}

const std::string &IbusConfig::GetEnginesXml() const {
  return engine_xml_;
}

const ibus::Config &IbusConfig::GetConfig() const {
  return config_;
}

const std::string &IbusConfig::GetLayout(const std::string& name) const {
  for (const ibus::Engine &engine : config_.engines()) {
    if (engine.name() == name) {
      return engine.layout();
    }
  }
  return default_layout_;
}

bool IbusConfig::IsActiveOnLaunch() const {
  if (config_.has_active_on_launch()) {
    return config_.active_on_launch();
  }

  // The default value is off as IBus team's recommentation.
  // https://github.com/google/mozc/issues/201
  return false;
}
}  // namespace mozc
