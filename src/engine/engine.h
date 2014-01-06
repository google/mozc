// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_ENGINE_ENGINE_H_
#define MOZC_ENGINE_ENGINE_H_

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "dictionary/pos_group.h"
#include "engine/engine_interface.h"

namespace mozc {

class ConnectorInterface;
class ConverterInterface;
class DataManagerInterface;
class DictionaryInterface;
class ImmutableConverterInterface;
class PredictorInterface;
class RewriterInterface;
class SegmenterInterface;
class SuggestionFilter;
class SuppressionDictionary;
class UserDataManagerInterface;
class UserDictionary;

// Builds and manages a set of modules that are necessary for conversion engine.
class Engine : public EngineInterface {
 public:
  Engine();
  virtual ~Engine();

  // Initializes the object by given a data manager (providing embedded data
  // set) and predictor factory function.
  // Predictor factory is used to select DefaultPredictor and MobilePredictor.
  void Init(const DataManagerInterface *data_manager,
            PredictorInterface *(*predictor_factory)(PredictorInterface *,
                                                     PredictorInterface *));

  virtual ConverterInterface *GetConverter() const { return converter_.get(); }
  virtual PredictorInterface *GetPredictor() const { return predictor_; }
  virtual SuppressionDictionary *GetSuppressionDictionary() {
    return suppression_dictionary_.get();
  }

  virtual bool Reload();

  virtual UserDataManagerInterface *GetUserDataManager() {
    return user_data_manager_.get();
  }

 private:
  scoped_ptr<SuppressionDictionary> suppression_dictionary_;
  scoped_ptr<const ConnectorInterface> connector_;
  scoped_ptr<const SegmenterInterface> segmenter_;
  scoped_ptr<UserDictionary> user_dictionary_;
  scoped_ptr<DictionaryInterface> suffix_dictionary_;
  scoped_ptr<DictionaryInterface> dictionary_;
  scoped_ptr<const PosGroup> pos_group_;
  scoped_ptr<ImmutableConverterInterface> immutable_converter_;
  scoped_ptr<const SuggestionFilter> suggestion_filter_;

  // TODO(noriyukit): Currently predictor and rewriter are created by this class
  // but owned by converter_. Since this class creates these two, it'd be better
  // if Engine class owns these two instances.
  PredictorInterface *predictor_;
  RewriterInterface *rewriter_;

  scoped_ptr<ConverterInterface> converter_;
  scoped_ptr<UserDataManagerInterface> user_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(Engine);
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_H_
