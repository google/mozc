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

#ifndef MOZC_ENGINE_ENGINE_INTERFACE_H_
#define MOZC_ENGINE_ENGINE_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "engine/engine_converter_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/engine_builder.pb.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {

// Builds and manages a set of modules that are necessary for conversion,
// prediction and rewrite. For example, a typical implementation of this
// interface would hold the dictionary shared among converters and predictors as
// well as Kana-Kanji converter/predictor, etc.
class EngineInterface {
 public:
  EngineInterface(const EngineInterface &) = delete;
  EngineInterface &operator=(const EngineInterface &) = delete;

  virtual ~EngineInterface() = default;

  // Creates new session converter.
  // This method is called per input context.
  virtual std::unique_ptr<engine::EngineConverterInterface>
  CreateEngineConverter(const commands::Request &request,
                        const config::Config &config) const = 0;

  // Gets the version of underlying data set.
  virtual absl::string_view GetDataVersion() const = 0;

  // Reloads internal data, e.g., user dictionary, etc.
  // This function may read data from local files.
  // Returns true if successfully reloaded or did nothing.
  virtual bool Reload() { return true; }

  // Synchronizes internal data, e.g., user dictionary, etc.
  // This function may write data into local files.
  // Returns true if successfully synced or did nothing.
  virtual bool Sync() { return true; }

  // Waits for reloader.
  // Returns true if successfully waited or did nothing.
  virtual bool Wait() { return true; }

  // Reloads internal data and wait for reloader.
  // Returns true if successfully reloaded and waited, or did nothing.
  virtual bool ReloadAndWait() { return true; }

  // Clears user history data.
  virtual bool ClearUserHistory() { return true; }

  // Clears user prediction data.
  virtual bool ClearUserPrediction() { return true; }

  // Clears unused user prediction data.
  virtual bool ClearUnusedUserPrediction() { return true; }

  // Gets the user POS list.
  virtual std::vector<std::string> GetPosList() const { return {}; }

  // Maybe reload a new data manager. Returns true if reloaded.
  virtual bool MaybeReloadEngine(EngineReloadResponse *response) {
    return false;
  }
  virtual bool SendEngineReloadRequest(const EngineReloadRequest &request) {
    return false;
  }
  virtual bool SendSupplementalModelReloadRequest(
      const EngineReloadRequest &request) {
    return false;
  }

  // Evaluates user dictionary command.
  virtual bool EvaluateUserDictionaryCommand(
      const user_dictionary::UserDictionaryCommand &command,
      user_dictionary::UserDictionaryCommandStatus *status) {
    return false;
  }

 protected:
  EngineInterface() = default;
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_INTERFACE_H_
