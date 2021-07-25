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

#include <string>
#include <vector>

#include "data_manager/data_manager_interface.h"
#include "dictionary/suppression_dictionary.h"
#include "absl/strings/string_view.h"

namespace mozc {

class ConverterInterface;
class PredictorInterface;
class UserDataManagerInterface;

// Builds and manages a set of modules that are necessary for conversion,
// prediction and rewrite. For example, a typical implementation of this
// interface would hold the dictionary shared among converters and predictors as
// well as Kana-Kanji converter/predictor, etc.
class EngineInterface {
 public:
  EngineInterface(const EngineInterface &) = delete;
  EngineInterface &operator=(const EngineInterface &) = delete;

  virtual ~EngineInterface() = default;

  // Returns a reference to a converter. The returned instance is managed by the
  // engine class and should not be deleted by callers.
  virtual ConverterInterface *GetConverter() const = 0;

  // Returns a reference to a predictor. The returned instance is managed by the
  // engine class and should not be deleted by callers.
  virtual PredictorInterface *GetPredictor() const = 0;

  // Returns a reference to the suppression dictionary. The returned instance is
  // managed by the engine class and should not be deleted by callers.
  virtual dictionary::SuppressionDictionary *GetSuppressionDictionary() = 0;

  // Reloads internal data, e.g., user dictionary, etc.
  virtual bool Reload() = 0;

  // Gets a user data manager.
  virtual UserDataManagerInterface *GetUserDataManager() = 0;

  // Gets the version of underlying data set.
  virtual absl::string_view GetDataVersion() const = 0;

  // Gets the data manager.
  virtual const DataManagerInterface *GetDataManager() const = 0;

  // Gets the user POS list.
  virtual std::vector<std::string> GetPOSList() const = 0;

 protected:
  EngineInterface() = default;
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_INTERFACE_H_
