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

#ifndef MOZC_UNIX_IBUS_IBUS_CONFIG_H_
#define MOZC_UNIX_IBUS_IBUS_CONFIG_H_

#include <map>
#include <string>

#include "unix/ibus/ibus_config.pb.h"

namespace mozc {

class IbusConfig {
 public:
  IbusConfig() : default_layout_("default") {}
  virtual ~IbusConfig() = default;

  // Disallow implicit constructors.
  IbusConfig(const IbusConfig&) = delete;
  IbusConfig& operator=(const IbusConfig&) = delete;

  bool Initialize();
  const std::string &GetEnginesXml() const;
  const std::string &GetLayout(const std::string &name) const;
  const ibus::Config &GetConfig() const;
  bool IsActiveOnLaunch() const;

  ibus::Engine::CompositionMode GetCompositionMode(
      const std::string &name) const;

 private:
  std::string default_layout_;
  std::string engine_xml_;
  ibus::Config config_;
};

}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_IBUS_CONFIG_H_
