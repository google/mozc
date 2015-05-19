// Copyright 2010-2013, Google Inc.
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
#include "converter/connector_base.h"
#include "converter/converter.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segmenter_base.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suffix_dictionary_token.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary.h"
#include "engine/engine_interface.h"
#include "engine/user_data_manager_interface.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/suggestion_filter.h"
#include "prediction/user_history_predictor.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

using mozc::dictionary::DictionaryImpl;
using mozc::dictionary::SystemDictionary;
using mozc::dictionary::ValueDictionary;

namespace {

class UserDataManagerImpl : public UserDataManagerInterface {
 public:
  explicit UserDataManagerImpl(PredictorInterface *predictor,
                               RewriterInterface *rewriter)
      : predictor_(predictor), rewriter_(rewriter) {}
  ~UserDataManagerImpl();

  virtual bool Sync();
  virtual bool Reload();
  virtual bool ClearUserHistory();
  virtual bool ClearUserPrediction();
  virtual bool ClearUnusedUserPrediction();
  virtual bool ClearUserPredictionEntry(const string &key, const string &value);
  virtual bool WaitForSyncerForTest();

 private:
  PredictorInterface *predictor_;
  RewriterInterface *rewriter_;
};

UserDataManagerImpl::~UserDataManagerImpl() {}

bool UserDataManagerImpl::Sync() {
  // TODO(noriyukit): In the current implementation, if rewriter_->Sync() fails,
  // predictor_->Sync() is never called. Check if we should call
  // predictor_->Sync() or not.
  return rewriter_->Sync() && predictor_->Sync();
}

bool UserDataManagerImpl::Reload() {
  // TODO(noriyukit): The same TODO as Sync().
  return rewriter_->Reload() && predictor_->Reload();
}

bool UserDataManagerImpl::ClearUserHistory() {
  rewriter_->Clear();
  return true;
}

bool UserDataManagerImpl::ClearUserPrediction() {
  predictor_->ClearAllHistory();
  return true;
}

bool UserDataManagerImpl::ClearUnusedUserPrediction() {
  predictor_->ClearUnusedHistory();
  return true;
}

bool UserDataManagerImpl::ClearUserPredictionEntry(const string &key,
                                                   const string &value) {
  return predictor_->ClearHistoryEntry(key, value);
}

bool UserDataManagerImpl::WaitForSyncerForTest() {
  return predictor_->WaitForSyncerForTest();
}

}  // namespace

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

  user_dictionary_.reset(
      new UserDictionary(new UserPOS(data_manager->GetUserPOSData()),
                         data_manager->GetPOSMatcher(),
                         suppression_dictionary_.get()));
  CHECK(user_dictionary_.get());

  const char *dictionary_data = NULL;
  int dictionary_size = 0;
  data_manager->GetSystemDictionaryData(&dictionary_data, &dictionary_size);

  dictionary_.reset(new DictionaryImpl(
      SystemDictionary::CreateSystemDictionaryFromImage(
          dictionary_data, dictionary_size),
      ValueDictionary::CreateValueDictionaryFromImage(
          *data_manager->GetPOSMatcher(), dictionary_data, dictionary_size),
      user_dictionary_.get(),
      suppression_dictionary_.get(),
      data_manager->GetPOSMatcher()));
  CHECK(dictionary_.get());

  const SuffixToken *suffix_tokens = NULL;
  size_t suffix_tokens_size = 0;
  data_manager->GetSuffixDictionaryData(&suffix_tokens, &suffix_tokens_size);
  suffix_dictionary_.reset(new SuffixDictionary(suffix_tokens,
                                                suffix_tokens_size));
  CHECK(suffix_dictionary_.get());

  connector_.reset(ConnectorBase::CreateFromDataManager(*data_manager));
  CHECK(connector_.get());

  segmenter_.reset(SegmenterBase::CreateFromDataManager(*data_manager));
  CHECK(segmenter_.get());

  pos_group_.reset(new PosGroup(data_manager->GetPosGroupData()));
  CHECK(pos_group_.get());

  {
    const char *data = NULL;
    size_t size = 0;
    data_manager->GetSuggestionFilterData(&data, &size);
    CHECK(data);
    suggestion_filter_.reset(new SuggestionFilter(data, size));
  }

  immutable_converter_.reset(new ImmutableConverterImpl(
      dictionary_.get(),
      suffix_dictionary_.get(),
      suppression_dictionary_.get(),
      connector_.get(),
      segmenter_.get(),
      data_manager->GetPOSMatcher(),
      pos_group_.get(),
      suggestion_filter_.get()));
  CHECK(immutable_converter_.get());

  // Since predictor and rewriter require a pointer to a converter instace,
  // allocate it first without initialization. It is initialized at the end of
  // this method.
  // TODO(noriyukit): This circular dependency is a bad design as careful
  // handling is necessary to avoid infinite loop. Find more beautiful design
  // and fix it!
  ConverterImpl *converter_impl = new ConverterImpl;
  converter_.reset(converter_impl);  // Involves cast to ConverterInterface*.
  CHECK(converter_.get());

  {
    // Create a predictor with three sub-predictors, dictionary predictor, user
    // history predictor, and extra predictor.
    PredictorInterface *dictionary_predictor =
        new DictionaryPredictor(converter_.get(),
                                immutable_converter_.get(),
                                dictionary_.get(),
                                suffix_dictionary_.get(),
                                connector_.get(),
                                segmenter_.get(),
                                data_manager->GetPOSMatcher(),
                                suggestion_filter_.get());
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

  rewriter_ = new RewriterImpl(converter_impl,
                               data_manager,
                               pos_group_.get(),
                               user_dictionary_.get());
  CHECK(rewriter_);

  converter_impl->Init(data_manager->GetPOSMatcher(),
                       suppression_dictionary_.get(),
                       predictor_,
                       rewriter_,
                       immutable_converter_.get());

  user_data_manager_.reset(new UserDataManagerImpl(predictor_, rewriter_));
}

bool Engine::Reload() {
  if (!user_dictionary_.get()) {
    return true;
  }
  VLOG(1) << "Reloading user dictionary";
  return user_dictionary_->Reload();
}

}  // namespace mozc
