// Copyright 2010-2012, Google Inc.
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

#include "engine/engine.h"

#include "base/base.h"
#include "base/logging.h"
#include "converter/converter.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_dictionary.h"
#include "engine/engine_interface.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

using mozc::dictionary::DictionaryImpl;

Engine::Engine() {}
Engine::~Engine() {}

void Engine::Init(
    const DataManagerInterface *data_manager,
    PredictorInterface *(*predictor_factory)(PredictorInterface *,
                                             PredictorInterface *,
                                             PredictorInterface *)) {
  CHECK(data_manager);
  CHECK(predictor_factory);

  suppression_dictionary_.reset(new SuppressionDictionary);
  CHECK(suppression_dictionary_.get());

#ifdef __native_client__
  // On NaCl, we don't support user dictionary at the moment. Note: passing NULL
  // is safe for user dictionary.
  user_dictionary_.reset(NULL);
#else
  user_dictionary_.reset(new UserDictionary(data_manager->GetUserPOS(),
                                            data_manager->GetPOSMatcher(),
                                            suppression_dictionary_.get()));
  CHECK(user_dictionary_.get());
#endif  // __native_client__

  dictionary_.reset(new DictionaryImpl(data_manager->CreateSystemDictionary(),
                                       data_manager->CreateValueDictionary(),
                                       user_dictionary_.get(),
                                       suppression_dictionary_.get(),
                                       data_manager->GetPOSMatcher()));
  CHECK(dictionary_.get());

  immutable_converter_.reset(new ImmutableConverterImpl(
      dictionary_.get(),
      data_manager->GetSuffixDictionary(),
      suppression_dictionary_.get(),
      data_manager->GetConnector(),
      data_manager->GetSegmenter(),
      data_manager->GetPOSMatcher(),
      data_manager->GetPosGroup()));
  CHECK(immutable_converter_.get());

  {
    // Create a predictor with three sub-predictors, dictionary predictor, user
    // history predictor, and extra predictor.
    PredictorInterface *dictionary_predictor =
        new DictionaryPredictor(immutable_converter_.get(),
                                dictionary_.get(),
                                data_manager->GetSuffixDictionary(),
                                data_manager);
    CHECK(dictionary_predictor);

    PredictorInterface *user_history_predictor =
        new UserHistoryPredictor(dictionary_.get(),
                                 data_manager->GetPOSMatcher(),
                                 suppression_dictionary_.get());
    CHECK(user_history_predictor);

    PredictorInterface *extra_predictor = NULL;

    predictor_ = (*predictor_factory)(dictionary_predictor,
                                      user_history_predictor,
                                      extra_predictor);
    CHECK(predictor_);
  }

  ConverterImpl *converter_impl = new ConverterImpl;  // Not initialized yet!
  converter_.reset(converter_impl);
  CHECK(converter_.get());

  rewriter_ = new RewriterImpl(converter_impl, data_manager);
  CHECK(rewriter_);

  converter_impl->Init(data_manager->GetPOSMatcher(),
                       data_manager->GetPosGroup(),
                       predictor_,
                       rewriter_,
                       immutable_converter_.get());
}

bool Engine::Reload() {
  if (!user_dictionary_.get()) {
    return true;
  }
  VLOG(1) << "Reloading user dictionary";
  return user_dictionary_->Reload();
}

}  // namespace mozc
