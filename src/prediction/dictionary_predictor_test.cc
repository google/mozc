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

#include "prediction/dictionary_predictor.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base.h"
#include "base/freelist.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/internal/typing_model.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/connector_base.h"
#include "converter/connector_interface.h"
#include "converter/conversion_request.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "converter/segmenter_base.h"
#include "converter/segmenter_interface.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suffix_dictionary_token.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "prediction/suggestion_filter.h"
#include "session/commands.pb.h"
#include "session/request_test_util.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "transliteration/transliteration.h"

using ::testing::Return;
using ::testing::_;

DECLARE_string(test_tmpdir);
DECLARE_bool(enable_expansion_for_dictionary_predictor);

namespace mozc {
namespace {

DictionaryInterface *CreateSystemDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  const char *data = NULL;
  int size = 0;
  data_manager.GetSystemDictionaryData(&data, &size);
  using mozc::dictionary::SystemDictionary;
  return SystemDictionary::CreateSystemDictionaryFromImage(data, size);
}

DictionaryInterface *CreateSuffixDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  const SuffixToken *tokens = NULL;
  size_t size = 0;
  data_manager.GetSuffixDictionaryData(&tokens, &size);
  return new SuffixDictionary(tokens, size);
}

SuggestionFilter *CreateSuggestionFilter(
    const DataManagerInterface &data_manager) {
  const char *data = NULL;
  size_t size = 0;
  data_manager.GetSuggestionFilterData(&data, &size);
  return new SuggestionFilter(data, size);
}

// Simple immutable converter mock for the realtime conversion test
class ImmutableConverterMock : public ImmutableConverterInterface {
 public:
  ImmutableConverterMock() {
    Segment *segment = segments_.add_segment();
    // "わたしのなまえはなかのです"
    segment->set_key("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae"
                     "\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88\xe3\x81\xaf"
                     "\xe3\x81\xaa\xe3\x81\x8b\xe3\x81\xae\xe3\x81\xa7"
                     "\xe3\x81\x99");
    Segment::Candidate *candidate = segment->add_candidate();
    // "私の名前は中野です"
    candidate->value = "\xe7\xa7\x81\xe3\x81\xae\xe5\x90\x8d\xe5\x89\x8d"
        "\xe3\x81\xaf\xe4\xb8\xad\xe9\x87\x8e\xe3\x81\xa7\xe3\x81\x99";
    // "わたしのなまえはなかのです"
    candidate->key = ("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae"
                      "\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88\xe3\x81\xaf"
                      "\xe3\x81\xaa\xe3\x81\x8b\xe3\x81\xae\xe3\x81\xa7"
                      "\xe3\x81\x99");
    // "わたしの, 私の"
    candidate->inner_segment_boundary.push_back(pair<int, int>(4, 2));
    // "なまえは, 名前は"
    candidate->inner_segment_boundary.push_back(pair<int, int>(4, 3));
    // "なかのです, 中野です"
    candidate->inner_segment_boundary.push_back(pair<int, int>(5, 4));
  }

  virtual bool ConvertForRequest(
      const ConversionRequest &request, Segments *segments) const {
    segments->CopyFrom(segments_);
    return true;
  }

 private:
  Segments segments_;
};

class TestableDictionaryPredictor : public DictionaryPredictor {
  // Test-only subclass: Just changing access levels
 public:
  TestableDictionaryPredictor(
      const ConverterInterface *converter,
      const ImmutableConverterInterface *immutable_converter,
      const DictionaryInterface *dictionary,
      const DictionaryInterface *suffix_dictionary,
      const ConnectorInterface *connector,
      const SegmenterInterface *segmenter,
      const POSMatcher *pos_matcher,
      const SuggestionFilter *suggestion_filter)
      : DictionaryPredictor(converter,
                            immutable_converter,
                            dictionary,
                            suffix_dictionary,
                            connector,
                            segmenter,
                            pos_matcher,
                            suggestion_filter) {}

  using DictionaryPredictor::PredictionTypes;
  using DictionaryPredictor::NO_PREDICTION;
  using DictionaryPredictor::UNIGRAM;
  using DictionaryPredictor::BIGRAM;
  using DictionaryPredictor::REALTIME;
  using DictionaryPredictor::REALTIME_TOP;
  using DictionaryPredictor::SUFFIX;
  using DictionaryPredictor::ENGLISH;
  using DictionaryPredictor::Result;
  using DictionaryPredictor::MakeResult;
  using DictionaryPredictor::AddPredictionToCandidates;
  using DictionaryPredictor::AggregateRealtimeConversion;
  using DictionaryPredictor::AggregateUnigramPrediction;
  using DictionaryPredictor::AggregateBigramPrediction;
  using DictionaryPredictor::AggregateSuffixPrediction;
  using DictionaryPredictor::AggregateEnglishPrediction;
  using DictionaryPredictor::ApplyPenaltyForKeyExpansion;
  using DictionaryPredictor::TYPING_CORRECTION;
  using DictionaryPredictor::AggregateTypeCorrectingPrediction;
};

// Helper class to hold dictionary data and predictor objects.
class MockDataAndPredictor {
 public:
  // Initializes predictor with given dictionary and suffix_dictionary.  When
  // NULL is passed to the first argument |dictionary|, the default
  // DictionaryMock is used. For the second, the default is MockDataManager's
  // suffix dictionary. Note that |dictionary| is owned by this class but
  // |suffix_dictionary| is NOT owned because the current design assumes that
  // suffix dictionary is singleton.
  void Init(const DictionaryInterface *dictionary = NULL,
            const DictionaryInterface *suffix_dictionary = NULL) {
    testing::MockDataManager data_manager;

    pos_matcher_ = data_manager.GetPOSMatcher();
    suppression_dictionary_.reset(new SuppressionDictionary);
    if (!dictionary) {
      dictionary_mock_ = new DictionaryMock;
      dictionary_.reset(dictionary_mock_);
    } else {
      dictionary_mock_ = NULL;
      dictionary_.reset(dictionary);
    }
    if (!suffix_dictionary) {
      suffix_dictionary_.reset(
          CreateSuffixDictionaryFromDataManager(data_manager));
    } else {
      suffix_dictionary_.reset(suffix_dictionary);
    }
    CHECK(suffix_dictionary_.get());

    connector_.reset(ConnectorBase::CreateFromDataManager(data_manager));
    CHECK(connector_.get());

    segmenter_.reset(SegmenterBase::CreateFromDataManager(data_manager));
    CHECK(segmenter_.get());

    pos_group_.reset(new PosGroup(data_manager.GetPosGroupData()));
    suggestion_filter_.reset(CreateSuggestionFilter(data_manager));
    immutable_converter_.reset(
        new ImmutableConverterImpl(dictionary_.get(),
                                   suffix_dictionary_.get(),
                                   suppression_dictionary_.get(),
                                   connector_.get(),
                                   segmenter_.get(),
                                   pos_matcher_,
                                   pos_group_.get(),
                                   suggestion_filter_.get()));
    converter_.reset(new ConverterMock());
    dictionary_predictor_.reset(
        new TestableDictionaryPredictor(converter_.get(),
                                        immutable_converter_.get(),
                                        dictionary_.get(),
                                        suffix_dictionary_.get(),
                                        connector_.get(),
                                        segmenter_.get(),
                                        data_manager.GetPOSMatcher(),
                                        suggestion_filter_.get()));
  }

  const POSMatcher &pos_matcher() const {
    return *pos_matcher_;
  }

  DictionaryMock *mutable_dictionary() {
    return dictionary_mock_;
  }

  ConverterMock *mutable_converter_mock() {
    return converter_.get();
  }

  const TestableDictionaryPredictor *dictionary_predictor() {
    return dictionary_predictor_.get();
  }

 private:
  const POSMatcher *pos_matcher_;
  scoped_ptr<SuppressionDictionary> suppression_dictionary_;
  scoped_ptr<const ConnectorInterface> connector_;
  scoped_ptr<const SegmenterInterface> segmenter_;
  scoped_ptr<const DictionaryInterface> suffix_dictionary_;
  scoped_ptr<const DictionaryInterface> dictionary_;
  DictionaryMock *dictionary_mock_;
  scoped_ptr<const PosGroup> pos_group_;
  scoped_ptr<ImmutableConverterInterface> immutable_converter_;
  scoped_ptr<ConverterMock> converter_;
  scoped_ptr<const SuggestionFilter> suggestion_filter_;
  scoped_ptr<TestableDictionaryPredictor> dictionary_predictor_;
};

class CallCheckDictionary : public DictionaryInterface {
 public:
  CallCheckDictionary() {}
  virtual ~CallCheckDictionary() {}

  MOCK_CONST_METHOD1(HasValue,
                     bool(const StringPiece));
  MOCK_CONST_METHOD3(LookupPredictive,
                     Node *(const char *str, int size,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD4(LookupPredictiveWithLimit,
                     Node *(const char *str, int size,
                            const Limit &limit,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD3(LookupPrefix,
                     Node *(const char *str, int size,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD3(LookupExact,
                     Node *(const char *str, int size,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD4(LookupPrefixWithLimit,
                     Node *(const char *str, int size,
                            const Limit &limit,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD3(LookupReverse,
                     Node *(const char *str, int size,
                            NodeAllocatorInterface *allocator));
};

void MakeSegmentsForSuggestion(const string key, Segments *segments) {
  segments->Clear();
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void MakeSegmentsForPrediction(const string key, Segments *segments) {
  segments->Clear();
  segments->set_max_prediction_candidates_size(50);
  segments->set_request_type(Segments::PREDICTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void PrependHistorySegments(const string &key,
                            const string &value,
                            Segments *segments) {
  Segment *seg = segments->push_front_segment();
  seg->set_segment_type(Segment::HISTORY);
  seg->set_key(key);
  Segment::Candidate *c = seg->add_candidate();
  c->key = key;
  c->content_key = key;
  c->value = value;
  c->content_value = value;
}

class MockTypingModel : public mozc::composer::TypingModel {
 public:
  MockTypingModel() : TypingModel(NULL, 0, NULL, 0, NULL) {}
  ~MockTypingModel() {}
  int GetCost(const StringPiece key) const {
    return 10;
  }
};

}  // namespace

class DictionaryPredictorTest : public ::testing::Test {
 public:
  DictionaryPredictorTest() :
      default_composer_(NULL, &default_request_),
      default_conversion_request_(&default_composer_, &default_request_),
      default_expansion_flag_(
          FLAGS_enable_expansion_for_dictionary_predictor) {
  }

  virtual ~DictionaryPredictorTest() {
    FLAGS_enable_expansion_for_dictionary_predictor = default_expansion_flag_;
  }

 protected:
  virtual void SetUp() {
    FLAGS_enable_expansion_for_dictionary_predictor = false;
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetConfig(&config_backup_);
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
  }

  virtual void TearDown() {
    FLAGS_enable_expansion_for_dictionary_predictor = false;
    config::ConfigHandler::SetConfig(config_backup_);
  }

  static void AddWordsToMockDic(DictionaryMock *mock) {
    // "ぐーぐるあ"
    const char kGoogleA[] = "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B"
        "\xE3\x81\x82";

    const char kGoogleAdsenseHiragana[] = "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90"
        "\xE3\x82\x8B\xE3\x81\x82\xE3\x81\xA9"
        "\xE3\x81\x9B\xE3\x82\x93\xE3\x81\x99";
    // "グーグルアドセンス"
    const char kGoogleAdsenseKatakana[] = "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0"
        "\xE3\x83\xAB\xE3\x82\xA2\xE3\x83\x89"
        "\xE3\x82\xBB\xE3\x83\xB3\xE3\x82\xB9";
    mock->AddLookupPredictive(kGoogleA, kGoogleAdsenseHiragana,
                              kGoogleAdsenseKatakana,
                              Node::DEFAULT_ATTRIBUTE);

    // "ぐーぐるあどわーず"
    const char kGoogleAdwordsHiragana[] =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B"
        "\xE3\x81\x82\xE3\x81\xA9\xE3\x82\x8F\xE3\x83\xBC\xE3\x81\x9A";
    const char kGoogleAdwordsKatakana[] =
        "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB"
        "\xE3\x82\xA2\xE3\x83\x89\xE3\x83\xAF\xE3\x83\xBC\xE3\x82\xBA";
    mock->AddLookupPredictive(kGoogleA, kGoogleAdwordsHiragana,
                              kGoogleAdwordsKatakana,
                              Node::DEFAULT_ATTRIBUTE);

    // "ぐーぐる"
    const char kGoogle[] = "\xE3\x81\x90\xE3\x83\xBC"
        "\xE3\x81\x90\xE3\x82\x8B";
    mock->AddLookupPredictive(kGoogle, kGoogleAdsenseHiragana,
                              kGoogleAdsenseKatakana,
                              Node::DEFAULT_ATTRIBUTE);
    mock->AddLookupPredictive(kGoogle, kGoogleAdwordsHiragana,
                              kGoogleAdwordsKatakana,
                              Node::DEFAULT_ATTRIBUTE);

    // "グーグル"
    const char kGoogleKatakana[] = "\xE3\x82\xB0\xE3\x83\xBC"
        "\xE3\x82\xB0\xE3\x83\xAB";
    mock->AddLookupPrefix(kGoogle, kGoogleKatakana,
                          kGoogleKatakana,
                          Node::DEFAULT_ATTRIBUTE);

    // "あどせんす"
    const char kAdsense[] = "\xE3\x81\x82\xE3\x81\xA9\xE3\x81\x9B"
        "\xE3\x82\x93\xE3\x81\x99";
    const char kAdsenseKatakana[] = "\xE3\x82\xA2\xE3\x83\x89"
        "\xE3\x82\xBB\xE3\x83\xB3\xE3\x82\xB9";
    mock->AddLookupPrefix(kAdsense, kAdsenseKatakana,
                          kAdsenseKatakana,
                          Node::DEFAULT_ATTRIBUTE);

    // "てすと"
    const char kTestHiragana[] = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";

    // "テスト"
    const char kTestKatakana[] = "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";

    mock->AddLookupPrefix(kTestHiragana, kTestHiragana,
                          kTestKatakana, Node::DEFAULT_ATTRIBUTE);

    // "かぷりちょうざ"
    const char kWrongCapriHiragana[] = "\xE3\x81\x8B\xE3\x81\xB7\xE3\x82\x8A"
        "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x96";

    // "かぷりちょーざ"
    const char kRightCapriHiragana[] = "\xE3\x81\x8B\xE3\x81\xB7\xE3\x82\x8A"
        "\xE3\x81\xA1\xE3\x82\x87\xE3\x83\xBC\xE3\x81\x96";

    // "カプリチョーザ"
    const char kCapriKatakana[] = "\xE3\x82\xAB\xE3\x83\x97\xE3\x83\xAA"
        "\xE3\x83\x81\xE3\x83\xA7\xE3\x83\xBC\xE3\x82\xB6";

    mock->AddLookupPrefix(kWrongCapriHiragana,
                          kRightCapriHiragana,
                          kCapriKatakana,
                          Node::SPELLING_CORRECTION);

    mock->AddLookupPredictive(kWrongCapriHiragana,
                              kRightCapriHiragana,
                              kCapriKatakana,
                              Node::SPELLING_CORRECTION);

    // "で"
    const char kDe[] = "\xE3\x81\xA7";

    mock->AddLookupPrefix(kDe, kDe, kDe, Node::DEFAULT_ATTRIBUTE);

    // "ひろすえ/広末"
    const char kHirosueHiragana[] = "\xE3\x81\xB2\xE3\x82\x8D"
        "\xE3\x81\x99\xE3\x81\x88";
    const char kHirosue[] = "\xE5\xBA\x83\xE6\x9C\xAB";

    mock->AddLookupPrefix(kHirosueHiragana,
                          kHirosueHiragana,
                          kHirosue, Node::DEFAULT_ATTRIBUTE);

    // "ゆーざー"
    const char kYuzaHiragana[] =
        "\xe3\x82\x86\xe3\x83\xbc\xe3\x81\x96\xe3\x83\xbc";
    // "ユーザー"
    const char kYuza[] = "\xe3\x83\xa6\xe3\x83\xbc\xe3\x82\xb6\xe3\x83\xbc";
    // For dictionary suggestion
    mock->AddLookupPredictive(
        kYuzaHiragana, kYuzaHiragana, kYuza,
        (Node::USER_DICTIONARY | Node::NO_VARIANTS_EXPANSION));
    // For realtime conversion
    mock->AddLookupPrefix(
        kYuzaHiragana, kYuzaHiragana, kYuza,
        (Node::USER_DICTIONARY | Node::NO_VARIANTS_EXPANSION));

    // Some English entries
    mock->AddLookupPredictive("conv", "converge", "converge",
                              Node::DEFAULT_ATTRIBUTE);
    mock->AddLookupPredictive("conv", "converged", "converged",
                              Node::DEFAULT_ATTRIBUTE);
    mock->AddLookupPredictive("conv", "convergent", "convergent",
                              Node::DEFAULT_ATTRIBUTE);
    mock->AddLookupPredictive("con", "contraction", "contraction",
                              Node::DEFAULT_ATTRIBUTE);
    mock->AddLookupPredictive("con", "control", "control",
                              Node::DEFAULT_ATTRIBUTE);
  }

  MockDataAndPredictor *CreateDictionaryPredictorWithMockData() {
    MockDataAndPredictor *ret = new MockDataAndPredictor;
    ret->Init();
    AddWordsToMockDic(ret->mutable_dictionary());
    return ret;
  }

  void GenerateKeyEvents(const string &text, vector<commands::KeyEvent> *keys) {
    keys->clear();

    const char *begin = text.data();
    const char *end = text.data() + text.size();
    size_t mblen = 0;

    while (begin < end) {
      commands::KeyEvent key;
      const char32 w = Util::UTF8ToUCS4(begin, end, &mblen);
      if (Util::GetCharacterSet(w) == Util::ASCII) {
        key.set_key_code(*begin);
      } else {
        key.set_key_code('?');
        key.set_key_string(string(begin, mblen));
      }
      begin += mblen;
      keys->push_back(key);
    }
  }

  void InsertInputSequence(const string &text, composer::Composer *composer) {
    vector<commands::KeyEvent> keys;
    GenerateKeyEvents(text, &keys);

    for (size_t i = 0; i < keys.size(); ++i) {
      composer->InsertCharacterKeyEvent(keys[i]);
    }
  }

  void InsertInputSequenceForProbableKeyEvent(const string &text,
                                              const uint32 *corrected_key_codes,
                                              composer::Composer *composer) {
    vector<commands::KeyEvent> keys;
    GenerateKeyEvents(text, &keys);

    for (size_t i = 0; i < keys.size(); ++i) {
      if (keys[i].key_code() != corrected_key_codes[i]) {
        commands::KeyEvent::ProbableKeyEvent *probable_key_event;

        probable_key_event = keys[i].add_probable_key_event();
        probable_key_event->set_key_code(keys[i].key_code());
        probable_key_event->set_probability(0.9f);

        probable_key_event = keys[i].add_probable_key_event();
        probable_key_event->set_key_code(corrected_key_codes[i]);
        probable_key_event->set_probability(0.1f);
      }
      composer->InsertCharacterKeyEvent(keys[i]);
    }
  }

  void ExpansionForUnigramTestHelper(bool use_expansion) {
    config::Config config;
    config.set_use_dictionary_suggest(true);
    config.set_use_realtime_conversion(false);
    config::ConfigHandler::SetConfig(config);

    composer::Table table;
    table.LoadFromFile("system://romanji-hiragana.tsv");
    scoped_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor;
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(check_dictionary, NULL);
    const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
    NodeAllocator allocator;

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      composer::Composer composer(&table, &default_request());
      InsertInputSequence("gu-g", &composer);
      const ConversionRequest request(&composer, &default_request());
      Segment *segment = segments.add_segment();
      CHECK(segment);
      string query;
      composer.GetQueryForPrediction(&query);
      segment->set_key(query);

      if (use_expansion) {
        EXPECT_CALL(*check_dictionary, LookupPredictiveWithLimit(_, _, _, _));
      } else {
        EXPECT_CALL(*check_dictionary, LookupPredictive(_, _, _));
      }

      vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateUnigramPrediction(
          TestableDictionaryPredictor::UNIGRAM,
          request, &segments, &allocator, &results);
    }
  }

  void ExpansionForBigramTestHelper(bool use_expansion) {
    config::Config config;
    config.set_use_dictionary_suggest(true);
    config.set_use_realtime_conversion(false);
    config::ConfigHandler::SetConfig(config);

    composer::Table table;
    table.LoadFromFile("system://romanji-hiragana.tsv");
    scoped_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor;
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(check_dictionary, NULL);
    const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
    NodeAllocator allocator;

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      // History segment's key and value should be in the dictionary
      Segment *segment = segments.add_segment();
      CHECK(segment);
      segment->set_segment_type(Segment::HISTORY);
      // "ぐーぐる"
      segment->set_key("\xe3\x81\x90\xe3\x83\xbc\xe3\x81\x90\xe3\x82\x8b");
      Segment::Candidate *cand = segment->add_candidate();
      // "ぐーぐる"
      cand->key = "\xe3\x81\x90\xe3\x83\xbc\xe3\x81\x90\xe3\x82\x8b";
      // "ぐーぐる"
      cand->content_key = "\xe3\x81\x90\xe3\x83\xbc\xe3\x81\x90\xe3\x82\x8b";
      // "グーグル"
      cand->value = "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab";
      // "グーグル"
      cand->content_value = "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab";

      segment = segments.add_segment();
      CHECK(segment);

      composer::Composer composer(&table, &default_request());
      InsertInputSequence("m", &composer);
      const ConversionRequest conversion_request(&composer, &default_request());
      string query;
      composer.GetQueryForPrediction(&query);
      segment->set_key(query);

      Node return_node_for_history;
      // "ぐーぐる"
      return_node_for_history.key =
          "\xe3\x81\x90\xe3\x83\xbc\xe3\x81\x90\xe3\x82\x8b";
      // "グーグル"
      return_node_for_history.value =
          "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab";
      return_node_for_history.lid = 1;
      return_node_for_history.rid = 1;
      // history key and value should be in the dictionary
      EXPECT_CALL(*check_dictionary, LookupPrefix(_, _, _))
          .WillOnce(Return(&return_node_for_history));
      if (use_expansion) {
        EXPECT_CALL(*check_dictionary, LookupPredictiveWithLimit(_, _, _, _));
      } else {
        EXPECT_CALL(*check_dictionary, LookupPredictive(_, _, _));
      }

      vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateBigramPrediction(
          TestableDictionaryPredictor::BIGRAM,
          conversion_request, &segments, &allocator, &results);
    }
  }

  void ExpansionForSuffixTestHelper(bool use_expansion) {
    config::Config config;
    config.set_use_dictionary_suggest(true);
    config.set_use_realtime_conversion(false);
    config::ConfigHandler::SetConfig(config);

    composer::Table table;
    table.LoadFromFile("system://romanji-hiragana.tsv");
    scoped_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor.
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(NULL, check_dictionary);
    const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
    NodeAllocator allocator;

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      Segment *segment = segments.add_segment();
      CHECK(segment);

      composer::Composer composer(&table, &default_request());
      InsertInputSequence("des", &composer);
      const ConversionRequest conversion_request(&composer, &default_request());
      string query;
      composer.GetQueryForPrediction(&query);
      segment->set_key(query);

      if (use_expansion) {
        EXPECT_CALL(*check_dictionary, LookupPredictiveWithLimit(_, _, _, _));
      } else {
        EXPECT_CALL(*check_dictionary, LookupPredictive(_, _, _));
      }

      vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateSuffixPrediction(
          TestableDictionaryPredictor::SUFFIX,
          conversion_request, &segments, &allocator, &results);
    }
  }

  bool FindCandidateByValue(
      const Segment &segment,
      const string &value) {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      const Segment::Candidate &c = segment.candidate(i);
      if (c.value == value) {
        return true;
      }
    }
    return false;
  }

  bool FindResultByValue(
      const vector<TestableDictionaryPredictor::Result> &results,
      const string &value) {
    for (size_t i = 0; i < results.size(); ++i) {
      if (results[i].node->value == value) {
        return true;
      }
    }
    return false;
  }

  void AggregateEnglishPredictionTestHelper(
      transliteration::TransliterationType input_mode,
      const char *key, const char *expected_prefix,
      const char *expected_values[], size_t expected_values_size) {
    scoped_ptr<MockDataAndPredictor> data_and_predictor(
        CreateDictionaryPredictorWithMockData());
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    composer::Table table;
    table.LoadFromFile("system://romanji-hiragana.tsv");
    composer::Composer composer(&table, &default_request());
    composer.SetInputMode(input_mode);
    InsertInputSequence(key, &composer);

    Segments segments;
    MakeSegmentsForPrediction(key, &segments);

    vector<TestableDictionaryPredictor::Result> results;
    NodeAllocator allocator;
    const ConversionRequest conversion_request(&composer, &default_request());
    predictor->AggregateEnglishPrediction(
        TestableDictionaryPredictor::ENGLISH,
        conversion_request, &segments, &allocator, &results);

    set<string> values;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::ENGLISH, results[i].types);
      EXPECT_TRUE(Util::StartsWith(results[i].node->value, expected_prefix))
          << results[i].node->value
          << " doesn't start with " << expected_prefix;
      values.insert(results[i].node->value);
    }
    for (size_t i = 0; i < expected_values_size; ++i) {
      EXPECT_TRUE(values.find(expected_values[i]) != values.end())
          << expected_values[i] << " isn't in the results";
    }
  }

  const commands::Request &default_request() const {
    return default_request_;
  }

  const composer::Composer &default_composer() const {
    return default_composer_;
  }

  const ConversionRequest &default_conversion_request() const {
    return default_conversion_request_;
  }

  void AggregateTypeCorrectingTestHelper(
      const char *key,
      const uint32 *corrected_key_codes,
      const char *expected_values[],
      size_t expected_values_size) {
    commands::Request qwerty_request;
    qwerty_request.set_special_romanji_table(
        commands::Request::QWERTY_MOBILE_TO_HIRAGANA);

    scoped_ptr<MockDataAndPredictor> data_and_predictor(
        CreateDictionaryPredictorWithMockData());
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    composer::Table table;
    table.LoadFromFile("system://qwerty_mobile-hiragana.tsv");
    table.typing_model_ = Singleton<MockTypingModel>::get();
    composer::Composer composer(&table, &qwerty_request);
    InsertInputSequenceForProbableKeyEvent(key, corrected_key_codes, &composer);

    Segments segments;
    MakeSegmentsForPrediction(key, &segments);

    vector<TestableDictionaryPredictor::Result> results;
    NodeAllocator allocator;
    allocator.set_max_nodes_size(1000);
    const ConversionRequest conversion_request(&composer, &qwerty_request);
    predictor->AggregateTypeCorrectingPrediction(
        TestableDictionaryPredictor::TYPING_CORRECTION,
        conversion_request, &segments, &allocator, &results);

    set<string> values;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::TYPING_CORRECTION,
                results[i].types);
      values.insert(results[i].node->value);
    }
    for (size_t i = 0; i < expected_values_size; ++i) {
      EXPECT_TRUE(values.find(expected_values[i]) != values.end())
          << expected_values[i] << " isn't in the results";
    }
  }

 private:
  config::Config config_backup_;
  const commands::Request default_request_;
  const composer::Composer default_composer_;
  const ConversionRequest default_conversion_request_;
  const bool default_expansion_flag_;
  scoped_ptr<ImmutableConverterInterface> immutable_converter_;
};

TEST_F(DictionaryPredictorTest, OnOffTest) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // turn off
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(false);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  // "ぐーぐるあ"
  MakeSegmentsForSuggestion
      ("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82",
       &segments);
  EXPECT_FALSE(predictor->PredictForRequest(default_conversion_request(),
                                            &segments));

  // turn on
  // "ぐーぐるあ"
  config.set_use_dictionary_suggest(true);
  config::ConfigHandler::SetConfig(config);
  MakeSegmentsForSuggestion
      ("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82",
       &segments);
  EXPECT_TRUE(predictor->PredictForRequest(default_conversion_request(),
                                           &segments));

  // empty query
  MakeSegmentsForSuggestion("", &segments);
  EXPECT_FALSE(predictor->PredictForRequest(default_conversion_request(),
                                            &segments));
}

TEST_F(DictionaryPredictorTest, PartialSuggestion) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  {
    // Set up mock converter.
    Segments segments;
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "Realtime top result";
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(true);
  config::ConfigHandler::SetConfig(config);
  // turn on mobile mode
  commands::Request request;
  request.set_mixed_conversion(true);

  // "ぐーぐるあ"
  segments.Clear();
  segments.set_max_prediction_candidates_size(10);
  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  Segment *seg = segments.add_segment();
  seg->set_key("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82");
  seg->set_segment_type(Segment::FREE);
  EXPECT_TRUE(predictor->PredictForRequest(default_conversion_request(),
                                           &segments));
}

TEST_F(DictionaryPredictorTest, BigramTest) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config::ConfigHandler::SetConfig(config);

  // "あ"
  MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);

  // history is "グーグル"
  PrependHistorySegments("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B",
                         "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB",
                         &segments);

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  // "グーグルアドセンス" will be returned.
  EXPECT_TRUE(predictor->PredictForRequest(default_conversion_request(),
                                           &segments));
}

TEST_F(DictionaryPredictorTest, BigramTestWithZeroQuery) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config::ConfigHandler::SetConfig(config);
  commands::Request request;
  request.set_zero_query_suggestion(true);
  const ConversionRequest conversion_request(&default_composer(), &request);

  // current query is empty
  MakeSegmentsForSuggestion("", &segments);

  // history is "グーグル"
  PrependHistorySegments("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B",
                         "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB",
                         &segments);

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
}

// Check that previous candidate never be shown at the current candidate.
TEST_F(DictionaryPredictorTest, Regression3042706) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config::ConfigHandler::SetConfig(config);

  // "だい"
  MakeSegmentsForSuggestion("\xE3\x81\xA0\xE3\x81\x84", &segments);

  // history is "きょうと/京都"
  PrependHistorySegments("\xE3\x81\x8D\xE3\x82\x87"
                         "\xE3\x81\x86\xE3\x81\xA8",
                         "\xE4\xBA\xAC\xE9\x83\xBD",
                         &segments);

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  EXPECT_TRUE(predictor->PredictForRequest(default_conversion_request(),
                                           &segments));
  EXPECT_EQ(2, segments.segments_size());   // history + current
  for (int i = 0; i < segments.segment(1).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(1).candidate(i);
    // "京都"
    EXPECT_FALSE(Util::StartsWith(candidate.content_value,
                                  "\xE4\xBA\xAC\xE9\x83\xBD"));
    // "だい"
    EXPECT_TRUE(Util::StartsWith(candidate.content_key,
                                 "\xE3\x81\xA0\xE3\x81\x84"));
  }
}

TEST_F(DictionaryPredictorTest, GetPredictionTypes) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  composer::Composer composer(NULL, &default_request());
  const ConversionRequest conversion_request(&composer, &default_request());

  // empty segments
  {
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));
  }

  // normal segments
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3"
                              "\x81\xA8\xE3\x81\xA0\xE3\x82\x88",
                              &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    segments.set_request_type(Segments::CONVERSION);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));
  }

  // short key
  {
    // "てす"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99",
                              &segments);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    // on prediction mode, return UNIGRAM
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));
  }

  // zipcode-like key
  {
    MakeSegmentsForSuggestion("0123", &segments);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));
  }

  // History is short => UNIGRAM
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                              "\xE3\x81\xA0\xE3\x82\x88", &segments);
    PrependHistorySegments("A", "A", &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));
  }

  // both History and current segment are long => UNIGRAM|BIGRAM
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                              "\xE3\x81\xA0\xE3\x82\x88", &segments);
    PrependHistorySegments("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                           "\xE3\x81\xA0\xE3\x82\x88", "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));
  }

  // Current segment is short => BIGRAM
  {
    MakeSegmentsForSuggestion("A", &segments);
    // "てすとだよ"
    PrependHistorySegments("\xE3\x81\xA6\xE3\x81\x99"
                           "\xE3\x81\xA8\xE3\x81\xA0\xE3\x82\x88",
                           "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::BIGRAM,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));
  }

  // Typing correction type shouldn't be appended.
  {
    // "ｐはよう"
    MakeSegmentsForSuggestion(
        "\xEF\xBD\x90\xE3\x81\xAF\xE3\x82\x88\xE3\x81\x86", &segments);
    EXPECT_FALSE(
        DictionaryPredictor::TYPING_CORRECTION
        & DictionaryPredictor::GetPredictionTypes(conversion_request,
                                                  segments));
  }

  // Input mode is HALF_ASCII or FULL_ASCII => ENGLISH
  {
    const bool orig_use_dictionary_suggest = config.use_dictionary_suggest();
    config.set_use_dictionary_suggest(true);
    config::ConfigHandler::SetConfig(config);

    MakeSegmentsForSuggestion("hel", &segments);

    composer.SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(
        DictionaryPredictor::ENGLISH,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    composer.SetInputMode(transliteration::FULL_ASCII);
    EXPECT_EQ(
        DictionaryPredictor::ENGLISH,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    // When dictionary suggest is turned off, English prediction should be
    // disabled.
    config.set_use_dictionary_suggest(false);
    config::ConfigHandler::SetConfig(config);

    composer.SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    composer.SetInputMode(transliteration::FULL_ASCII);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    config.set_use_dictionary_suggest(true);
    config::ConfigHandler::SetConfig(config);

    segments.set_request_type(Segments::PARTIAL_SUGGESTION);
    composer.SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(
        DictionaryPredictor::ENGLISH | DictionaryPredictor::REALTIME,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    composer.SetInputMode(transliteration::FULL_ASCII);
    EXPECT_EQ(
        DictionaryPredictor::ENGLISH | DictionaryPredictor::REALTIME,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    config.set_use_dictionary_suggest(false);
    config::ConfigHandler::SetConfig(config);

    composer.SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(
        DictionaryPredictor::REALTIME,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    composer.SetInputMode(transliteration::FULL_ASCII);
    EXPECT_EQ(
        DictionaryPredictor::REALTIME,
        DictionaryPredictor::GetPredictionTypes(conversion_request, segments));

    config.set_use_dictionary_suggest(orig_use_dictionary_suggest);
    config::ConfigHandler::SetConfig(config);
  }
}

TEST_F(DictionaryPredictorTest, GetPredictionTypesTestWithTypingCorrection) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config.set_use_typing_correction(true);
  config::ConfigHandler::SetConfig(config);

  composer::Composer composer(NULL, &default_request());
  const ConversionRequest conversion_request(&composer, &default_request());

  // "ｐはよう"
  MakeSegmentsForSuggestion(
      "\xEF\xBD\x90\xE3\x81\xAF\xE3\x82\x88\xE3\x81\x86", &segments);
  EXPECT_EQ(
      DictionaryPredictor::UNIGRAM | DictionaryPredictor::TYPING_CORRECTION,
      DictionaryPredictor::GetPredictionTypes(conversion_request, segments));
}

TEST_F(DictionaryPredictorTest, GetPredictionTypesTestWithZeroQuerySuggestion) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);
  commands::Request request;
  request.set_zero_query_suggestion(true);

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  composer::Composer composer(NULL, &request);
  const ConversionRequest conversion_request(&composer, &request);

  // empty segments
  {
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        predictor->GetPredictionTypes(conversion_request, segments));
  }

  // normal segments
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3"
                              "\x81\xA8\xE3\x81\xA0\xE3\x82\x88",
                              &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        predictor->GetPredictionTypes(conversion_request, segments));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        predictor->GetPredictionTypes(conversion_request, segments));

    segments.set_request_type(Segments::CONVERSION);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        predictor->GetPredictionTypes(conversion_request, segments));
  }

  // short key
  {
    // "て"
    MakeSegmentsForSuggestion("\xE3\x81\xA6",
                              &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        predictor->GetPredictionTypes(conversion_request, segments));

    // on prediction mode, return UNIGRAM
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        predictor->GetPredictionTypes(conversion_request, segments));
  }

  // History is short => UNIGRAM
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                              "\xE3\x81\xA0\xE3\x82\x88", &segments);
    PrependHistorySegments("A", "A", &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM | DictionaryPredictor::SUFFIX,
        predictor->GetPredictionTypes(conversion_request, segments));
  }

  // both History and current segment are long => UNIGRAM|BIGRAM
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                              "\xE3\x81\xA0\xE3\x82\x88", &segments);
    PrependHistorySegments("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                           "\xE3\x81\xA0\xE3\x82\x88", "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM |
        DictionaryPredictor::SUFFIX,
        predictor->GetPredictionTypes(conversion_request, segments));
  }

  {
    MakeSegmentsForSuggestion("A", &segments);
    // "てすとだよ"
    PrependHistorySegments("\xE3\x81\xA6\xE3\x81\x99"
                           "\xE3\x81\xA8\xE3\x81\xA0\xE3\x82\x88",
                           "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::BIGRAM | DictionaryPredictor::UNIGRAM |
        DictionaryPredictor::SUFFIX,
        predictor->GetPredictionTypes(conversion_request, segments));
  }

  {
    MakeSegmentsForSuggestion("", &segments);
    // "て"
    PrependHistorySegments("\xE3\x81\xA6",
                           "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::SUFFIX,
        predictor->GetPredictionTypes(conversion_request, segments));
  }

  {
    MakeSegmentsForSuggestion("A", &segments);
    // "て"
    PrependHistorySegments("\xE3\x81\xA6",
                           "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM | DictionaryPredictor::SUFFIX,
        predictor->GetPredictionTypes(conversion_request, segments));
  }

  {
    MakeSegmentsForSuggestion("", &segments);
    // "てすとだよ"
    PrependHistorySegments("\xE3\x81\xA6\xE3\x81\x99"
                           "\xE3\x81\xA8\xE3\x81\xA0\xE3\x82\x88",
                           "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::BIGRAM | DictionaryPredictor::SUFFIX,
        predictor->GetPredictionTypes(conversion_request, segments));
  }
}

TEST_F(DictionaryPredictorTest, AggregateUnigramPrediction) {
  Segments segments;
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // "ぐーぐるあ"
  const char kKey[] = "\xE3\x81\x90\xE3\x83\xBC"
      "\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82";

  MakeSegmentsForSuggestion(kKey, &segments);

  vector<DictionaryPredictor::Result> results;
  NodeAllocator allocator;
  const ConversionRequest conversion_request;

  predictor->AggregateUnigramPrediction(
      DictionaryPredictor::BIGRAM,
      conversion_request, &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor->AggregateUnigramPrediction(
      DictionaryPredictor::REALTIME,
      conversion_request, &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor->AggregateUnigramPrediction(
      DictionaryPredictor::UNIGRAM,
      conversion_request, &segments, &allocator, &results);
  EXPECT_FALSE(results.empty());

  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[i].types);
    EXPECT_TRUE(Util::StartsWith(results[i].node->key, kKey));
  }

  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, AggregateBigramPrediction) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  NodeAllocator allocator;
  const ConversionRequest conversion_request;

  {
    Segments segments;

    // "あ"
    MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);

    // history is "グーグル"
    const char kHistoryKey[] =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
    const char kHistoryValue[] =
        "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB";

    PrependHistorySegments(kHistoryKey, kHistoryValue,
                           &segments);

    vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(
        DictionaryPredictor::UNIGRAM,
        conversion_request, &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateBigramPrediction(
        DictionaryPredictor::REALTIME,
        conversion_request, &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateBigramPrediction(
        DictionaryPredictor::BIGRAM,
        conversion_request, &segments, &allocator, &results);
    EXPECT_FALSE(results.empty());

    for (size_t i = 0; i < results.size(); ++i) {
      // "グーグルアドセンス", "グーグル", "アドセンス"
      // are in the dictionary.
      if (results[i].node->value == "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0"
          "\xE3\x83\xAB\xE3\x82\xA2\xE3\x83\x89"
          "\xE3\x82\xBB\xE3\x83\xB3\xE3\x82\xB9") {
        EXPECT_EQ(DictionaryPredictor::BIGRAM, results[i].types);
      } else {
        EXPECT_EQ(DictionaryPredictor::NO_PREDICTION, results[i].types);
      }
      EXPECT_TRUE(Util::StartsWith(results[i].node->key, kHistoryKey));
      EXPECT_TRUE(Util::StartsWith(results[i].node->value, kHistoryValue));
    }

    EXPECT_EQ(1, segments.conversion_segments_size());
  }

  {
    Segments segments;

    // "と"
    MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);

    // "てす"
    const char kHistoryKey[] = "\xE3\x81\xA6\xE3\x81\x99";
    // "テス"
    const char kHistoryValue[] = "\xE3\x83\x86\xE3\x82\xB9";

    PrependHistorySegments(kHistoryKey, kHistoryValue,
                           &segments);

    vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(
        DictionaryPredictor::BIGRAM,
        conversion_request, &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSize) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  Segments segments;

  // GetRealtimeCandidateMaxSize has some heuristics so here we test following
  // conditions.
  // - The result must be equal or less than kMaxSize;
  // - If mixed_conversion is the same, the result of SUGGESTION is
  //        equal or less than PREDICTION.
  // - If mixed_conversion is the same, the result of PARTIAL_SUGGESTION is
  //        equal or less than PARTIAL_PREDICTION.
  // - Partial version has equal or greater than non-partial version.

  const size_t kMaxSize = 100;

  // non-partial, non-mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, prediction_no_mixed);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, suggestion_no_mixed);
  EXPECT_LE(suggestion_no_mixed, prediction_no_mixed);

  // non-partial, mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, prediction_mixed);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, suggestion_mixed);

  // partial, non-mixed-conversion
  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  const size_t partial_prediction_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, partial_prediction_no_mixed);

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  const size_t partial_suggestion_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, partial_suggestion_no_mixed);
  EXPECT_LE(partial_suggestion_no_mixed, partial_prediction_no_mixed);

  // partial, mixed-conversion
  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  const size_t partial_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, partial_prediction_mixed);

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  const size_t partial_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, partial_suggestion_mixed);
  EXPECT_LE(partial_suggestion_mixed, partial_prediction_mixed);

  EXPECT_GE(partial_prediction_no_mixed, prediction_no_mixed);
  EXPECT_GE(partial_prediction_mixed, prediction_mixed);
  EXPECT_GE(partial_suggestion_no_mixed, suggestion_no_mixed);
  EXPECT_GE(partial_suggestion_mixed, suggestion_mixed);
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSizeForMixed) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  Segments segments;
  Segment *segment = segments.add_segment();

  const size_t kMaxSize = 100;

  // for short key, try to provide many results as possible
  segment->set_key("short");
  segments.set_request_type(Segments::SUGGESTION);
  const size_t short_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, short_suggestion_mixed);

  segments.set_request_type(Segments::PREDICTION);
  const size_t short_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, short_prediction_mixed);

  // for long key, provide few results
  segment->set_key("long_request_key");
  segments.set_request_type(Segments::SUGGESTION);
  const size_t long_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, long_suggestion_mixed);
  EXPECT_GT(short_suggestion_mixed, long_suggestion_mixed);

  segments.set_request_type(Segments::PREDICTION);
  const size_t long_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, long_prediction_mixed);
  EXPECT_GT(kMaxSize, long_prediction_mixed + long_suggestion_mixed);
  EXPECT_GT(short_prediction_mixed, long_prediction_mixed);
}

TEST_F(DictionaryPredictorTest, AggregateRealtimeConversion) {
  testing::MockDataManager data_manager;
  scoped_ptr<const DictionaryInterface> dictionary(new DictionaryMock);
  scoped_ptr<ConverterMock> converter(new ConverterMock);
  scoped_ptr<ImmutableConverterInterface> immutable_converter(
      new ImmutableConverterMock);
  scoped_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  scoped_ptr<const ConnectorInterface> connector(
      ConnectorBase::CreateFromDataManager(data_manager));
  scoped_ptr<const SegmenterInterface> segmenter(
      SegmenterBase::CreateFromDataManager(data_manager));
  scoped_ptr<const SuggestionFilter> suggestion_filter(
      CreateSuggestionFilter(data_manager));
  scoped_ptr<TestableDictionaryPredictor> predictor(
      new TestableDictionaryPredictor(converter.get(),
                                      immutable_converter.get(),
                                      dictionary.get(),
                                      suffix_dictionary.get(),
                                      connector.get(),
                                      segmenter.get(),
                                      data_manager.GetPOSMatcher(),
                                      suggestion_filter.get()));

  // "わたしのなまえはなかのです"
  const char kKey[] =
      "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97"
      "\xE3\x81\xAE\xE3\x81\xAA\xE3\x81\xBE"
      "\xE3\x81\x88\xE3\x81\xAF\xE3\x81\xAA"
      "\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99";

  // Set up mock converter
  {
    // Make segments like:
    // "わたしの"    | "なまえは" | "なかのです"
    // "Watashino" | "Namaeha" | "Nakanodesu"
    Segments segments;

    // ("わたしの", "Watashino")
    Segment *segment = segments.add_segment();
    segment->set_key("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE");
    segment->add_candidate()->value = "Watashino";

    // ("なまえは", "Namaeha")
    segment = segments.add_segment();
    segment->set_key("\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF");
    segment->add_candidate()->value = "Namaeha";

    // ("なかのです", "Nakanodesu")
    segment = segments.add_segment();
    segment->set_key(
        "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99");
    segment->add_candidate()->value = "Nakanodesu";

    converter->SetStartConversionForRequest(&segments, true);
  }

  // A test case with use_actual_converter_for_realtime_conversion being false,
  // i.e., realtime conversion result is generated by ImmutableConverterMock.
  {
    Segments segments;
    NodeAllocator allocator;

    MakeSegmentsForSuggestion(kKey, &segments);

    vector<TestableDictionaryPredictor::Result> results;
    ConversionRequest request;
    request.set_use_actual_converter_for_realtime_conversion(false);

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::UNIGRAM, request,
        &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::BIGRAM, request,
        &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::REALTIME, request,
        &segments, &allocator, &results);

    ASSERT_EQ(1, results.size());
    EXPECT_EQ(TestableDictionaryPredictor::REALTIME, results[0].types);
    EXPECT_EQ(kKey, results[0].node->key);
    EXPECT_EQ(3, results[0].inner_segment_boundary.size());
  }

  // A test case with use_actual_converter_for_realtime_conversion being true,
  // i.e., realtime conversion result is generated by ConverterMock.
  {
    Segments segments;
    NodeAllocator allocator;

    MakeSegmentsForSuggestion(kKey, &segments);

    vector<TestableDictionaryPredictor::Result> results;
    ConversionRequest request;
    request.set_use_actual_converter_for_realtime_conversion(true);

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::UNIGRAM, request,
        &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::BIGRAM, request,
        &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::REALTIME, request,
        &segments, &allocator, &results);

    // When |request.use_actual_converter_for_realtime_conversion| is true, the
    // extra label REALTIME_TOP is expected to be added.
    ASSERT_EQ(2, results.size());
    bool realtime_top_found = false;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::REALTIME |
                TestableDictionaryPredictor::REALTIME_TOP, results[i].types);
      if (results[i].node->key == kKey &&
          results[i].node->value == "WatashinoNamaehaNakanodesu" &&
          results[i].inner_segment_boundary.size() == 3) {
        realtime_top_found = true;
        break;
      }
    }
    EXPECT_TRUE(realtime_top_found);
  }
}

namespace {

struct SimpleSuffixToken {
  const char *key;
  const char *value;
};

const SimpleSuffixToken kSuffixTokens[] = {
  //  { "いか",   "以下" }
  { "\xE3\x81\x84\xE3\x81\x8B",   "\xE4\xBB\xA5\xE4\xB8\x8B" }
};

class TestSuffixDictionary : public DictionaryInterface {
 public:
  TestSuffixDictionary() {}
  virtual ~TestSuffixDictionary() {}

  virtual bool HasValue(const StringPiece value) const {
    return false;
  }

  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const {
    string input_key(str, size);
    Node *result = NULL;
    for (size_t i = 0; i < arraysize(kSuffixTokens); ++i) {
      const SimpleSuffixToken *token = &kSuffixTokens[i];
      DCHECK(token);
      DCHECK(token->key);
      if (!input_key.empty() && !Util::StartsWith(token->key, input_key)) {
        continue;
      }
      Node *node = allocator->NewNode();
      DCHECK(node);
      node->Init();
      node->wcost = 1000;
      node->key = token->key;
      node->value = token->value;
      node->lid = 0;
      node->rid = 0;
      node->bnext = result;
      result = node;
    }
    return result;
  }

  virtual Node *LookupPredictiveWithLimit(
      const char *str, int size,
      const Limit &limit,
      NodeAllocatorInterface *allocator) const {
    return NULL;
  }

  virtual Node *LookupPrefixWithLimit(const char *str, int size,
                                      const Limit &limit,
                                      NodeAllocatorInterface *allocator) const {
    return NULL;
  }

  virtual Node *LookupPrefix(const char *str, int size,
                             NodeAllocatorInterface *allocator) const {
    return NULL;
  }

  virtual Node *LookupExact(const char *str, int size,
                            NodeAllocatorInterface *allocator) const {
    return NULL;
  }

  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const {
    return NULL;
  }
};
}  // namespace

TEST_F(DictionaryPredictorTest, GetUnigramCandidateCutoffThreshold) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  Segments segments;

  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction =
      predictor->GetUnigramCandidateCutoffThreshold(segments);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion =
      predictor->GetUnigramCandidateCutoffThreshold(segments);
  EXPECT_LE(suggestion, prediction);
}

TEST_F(DictionaryPredictorTest, AggregateSuffixPrediction) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor);
  data_and_predictor->Init(NULL, new TestSuffixDictionary());

  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  NodeAllocator allocator;

  const ConversionRequest conversion_request;
  Segments segments;

  // "あ"
  MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);

  // history is "グーグル"
  const char kHistoryKey[] =
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
  const char kHistoryValue[] =
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB";

  PrependHistorySegments(kHistoryKey, kHistoryValue,
                         &segments);

  vector<DictionaryPredictor::Result> results;

  // Since SuffixDictionary only returns when key is "い".
  // result should be empty.
  predictor->AggregateSuffixPrediction(
      DictionaryPredictor::SUFFIX,
      conversion_request, &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  results.clear();
  segments.mutable_conversion_segment(0)->set_key("");
  predictor->AggregateSuffixPrediction(
      DictionaryPredictor::SUFFIX,
      conversion_request, &segments, &allocator, &results);
  EXPECT_FALSE(results.empty());

  results.clear();
  predictor->AggregateSuffixPrediction(
      DictionaryPredictor::UNIGRAM,
      conversion_request, &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor->AggregateSuffixPrediction(
      DictionaryPredictor::REALTIME,
      conversion_request, &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor->AggregateSuffixPrediction(
      DictionaryPredictor::BIGRAM,
      conversion_request, &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());
}

TEST_F(DictionaryPredictorTest, AggregateEnglishPrediction) {
  // Input mode: HALF_ASCII, Key: lower case
  //   => Prediction should be in half-width lower case.
  {
    const char *kExpectedValues[] = {
      "converge", "converged", "convergent",
    };
    AggregateEnglishPredictionTestHelper(
        transliteration::HALF_ASCII, "conv", "conv",
        kExpectedValues, arraysize(kExpectedValues));
  }
  // Input mode: HALF_ASCII, Key: upper case
  //   => Prediction should be in half-width upper case.
  {
    const char *kExpectedValues[] = {
      "CONVERGE", "CONVERGED", "CONVERGENT",
    };
    AggregateEnglishPredictionTestHelper(
        transliteration::HALF_ASCII, "CONV", "CONV",
        kExpectedValues, arraysize(kExpectedValues));
  }
  // Input mode: HALF_ASCII, Key: capitalized
  //   => Prediction should be half-width and capitalized
  {
    const char *kExpectedValues[] = {
      "Converge", "Converged", "Convergent",
    };
    AggregateEnglishPredictionTestHelper(
        transliteration::HALF_ASCII, "Conv", "Conv",
        kExpectedValues, arraysize(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: lower case
  //   => Prediction should be in full-wdith lower case.
  {
    const char *kExpectedValues[] = {
      // "ｃｏｎｖｅｒｇｅ"
      "\xEF\xBD\x83\xEF\xBD\x8F\xEF\xBD\x8E\xEF\xBD\x96\xEF\xBD\x85"
      "\xEF\xBD\x92\xEF\xBD\x87\xEF\xBD\x85",
      // "ｃｏｎｖｅｒｇｅｄ"
      "\xEF\xBD\x83\xEF\xBD\x8F\xEF\xBD\x8E\xEF\xBD\x96\xEF\xBD\x85"
      "\xEF\xBD\x92\xEF\xBD\x87\xEF\xBD\x85\xEF\xBD\x84",
      // "ｃｏｎｖｅｒｇｅｎｔ"
      "\xEF\xBD\x83\xEF\xBD\x8F\xEF\xBD\x8E\xEF\xBD\x96\xEF\xBD\x85"
      "\xEF\xBD\x92\xEF\xBD\x87\xEF\xBD\x85\xEF\xBD\x8E\xEF\xBD\x94",
    };
    AggregateEnglishPredictionTestHelper(
        transliteration::FULL_ASCII,
        "conv",
        "\xEF\xBD\x83\xEF\xBD\x8F\xEF\xBD\x8E\xEF\xBD\x96",  // "ｃｏｎｖ"
        kExpectedValues, arraysize(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: upper case
  //   => Prediction should be in full-width upper case.
  {
    const char *kExpectedValues[] = {
      // "ＣＯＮＶＥＲＧＥ"
      "\xEF\xBC\xA3\xEF\xBC\xAF\xEF\xBC\xAE\xEF\xBC\xB6\xEF\xBC\xA5"
      "\xEF\xBC\xB2\xEF\xBC\xA7\xEF\xBC\xA5",
      // "ＣＯＮＶＥＲＧＥＤ"
      "\xEF\xBC\xA3\xEF\xBC\xAF\xEF\xBC\xAE\xEF\xBC\xB6\xEF\xBC\xA5"
      "\xEF\xBC\xB2\xEF\xBC\xA7\xEF\xBC\xA5\xEF\xBC\xA4",
      // "ＣＯＮＶＥＲＧＥＮＴ"
      "\xEF\xBC\xA3\xEF\xBC\xAF\xEF\xBC\xAE\xEF\xBC\xB6\xEF\xBC\xA5"
      "\xEF\xBC\xB2\xEF\xBC\xA7\xEF\xBC\xA5\xEF\xBC\xAE\xEF\xBC\xB4",
    };
    AggregateEnglishPredictionTestHelper(
        transliteration::FULL_ASCII,
        "CONV",
        "\xEF\xBC\xA3\xEF\xBC\xAF\xEF\xBC\xAE\xEF\xBC\xB6",  // "ＣＯＮＶ"
        kExpectedValues, arraysize(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: capitalized
  //   => Prediction should be full-width and capitalized
  {
    const char *kExpectedValues[] = {
      // "Ｃｏｎｖｅｒｇｅ"
      "\xEF\xBC\xA3\xEF\xBD\x8F\xEF\xBD\x8E\xEF\xBD\x96\xEF\xBD\x85"
      "\xEF\xBD\x92\xEF\xBD\x87\xEF\xBD\x85",
      // "Ｃｏｎｖｅｒｇｅｄ"
      "\xEF\xBC\xA3\xEF\xBD\x8F\xEF\xBD\x8E\xEF\xBD\x96\xEF\xBD\x85"
      "\xEF\xBD\x92\xEF\xBD\x87\xEF\xBD\x85\xEF\xBD\x84",
      // "Ｃｏｎｖｅｒｇｅｎｔ"
      "\xEF\xBC\xA3\xEF\xBD\x8F\xEF\xBD\x8E\xEF\xBD\x96\xEF\xBD\x85"
      "\xEF\xBD\x92\xEF\xBD\x87\xEF\xBD\x85\xEF\xBD\x8E\xEF\xBD\x94",
    };
    AggregateEnglishPredictionTestHelper(
        transliteration::FULL_ASCII,
        "Conv",
        "\xEF\xBC\xA3\xEF\xBD\x8F\xEF\xBD\x8E\xEF\xBD\x96",  // "Ｃｏｎｖ"
        kExpectedValues, arraysize(kExpectedValues));
  }
}

TEST_F(DictionaryPredictorTest, AggregateTypeCorrectingPrediction) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config.set_use_typing_correction(true);
  config::ConfigHandler::SetConfig(config);

  const char kInputText[] = "gu-huru";
  const uint32 kCorrectedKeyCodes[] = {'g', 'u', '-', 'g', 'u', 'r', 'u'};
  const char *kExpectedValues[] = {
    // "グーグルアドセンス"
    "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0"
    "\xE3\x83\xAB\xE3\x82\xA2\xE3\x83\x89"
    "\xE3\x82\xBB\xE3\x83\xB3\xE3\x82\xB9",
    // "グーグルアドワーズ"
    "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB"
    "\xE3\x82\xA2\xE3\x83\x89\xE3\x83\xAF\xE3\x83\xBC\xE3\x82\xBA",
  };
  AggregateTypeCorrectingTestHelper(kInputText,
                                    kCorrectedKeyCodes,
                                    kExpectedValues,
                                    arraysize(kExpectedValues));
}

TEST_F(DictionaryPredictorTest, AddCostToNodesWcost) {
  Node node1, node2, node3;
  node1.wcost = 10;
  node2.wcost = 20;
  node3.wcost = 30;
  node1.bnext = &node2;
  node2.bnext = &node3;
  node3.bnext = NULL;

  EXPECT_EQ(&node3, DictionaryPredictor::AddCostToNodesWcost(1, &node1));
  EXPECT_EQ(11, node1.wcost);
  EXPECT_EQ(21, node2.wcost);
  EXPECT_EQ(31, node3.wcost);

  EXPECT_EQ(&node3, DictionaryPredictor::AddCostToNodesWcost(1, &node2));
  EXPECT_EQ(11, node1.wcost);
  EXPECT_EQ(22, node2.wcost);
  EXPECT_EQ(32, node3.wcost);

  EXPECT_EQ(&node3, DictionaryPredictor::AddCostToNodesWcost(1, &node3));
  EXPECT_EQ(11, node1.wcost);
  EXPECT_EQ(22, node2.wcost);
  EXPECT_EQ(33, node3.wcost);
}

TEST_F(DictionaryPredictorTest, ZeroQuerySuggestionAfterNumbers) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  const POSMatcher &pos_matcher = data_and_predictor->pos_matcher();
  NodeAllocator allocator;
  const ConversionRequest conversion_request;
  Segments segments;

  {
    MakeSegmentsForSuggestion("", &segments);

    const char kHistoryKey[] = "12";
    const char kHistoryValue[] = "12";
    const char kExpectedValue[] = "\xE6\x9C\x88";  // "月" (month)
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    vector<DictionaryPredictor::Result> results;
    predictor->AggregateSuffixPrediction(
        DictionaryPredictor::SUFFIX,
        conversion_request, &segments, &allocator, &results);
    EXPECT_FALSE(results.empty());

    vector<DictionaryPredictor::Result>::const_iterator target = results.end();
    for (vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->node->value == kExpectedValue) {
        target = it;
        break;
      }
    }
    EXPECT_NE(results.end(), target);
    EXPECT_EQ(target->node->value, kExpectedValue);
    EXPECT_EQ(target->node->lid, pos_matcher.GetCounterSuffixWordId());
    EXPECT_EQ(target->node->rid, pos_matcher.GetCounterSuffixWordId());

    // Make sure number suffixes are not suggested when there is a key
    results.clear();
    MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);  // "あ"
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    predictor->AggregateSuffixPrediction(
        DictionaryPredictor::SUFFIX,
        conversion_request, &segments, &allocator, &results);
    target = results.end();
    for (vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->node->value == kExpectedValue) {
        target = it;
        break;
      }
    }
    EXPECT_EQ(results.end(), target);
  }

  {
    MakeSegmentsForSuggestion("", &segments);

    const char kHistoryKey[] = "66050713";  // A random number
    const char kHistoryValue[] = "66050713";
    const char kExpectedValue[] = "\xE5\x80\x8B";  // "個" (piece)
    PrependHistorySegments(kHistoryKey, kHistoryValue,
                           &segments);
    vector<DictionaryPredictor::Result> results;
    predictor->AggregateSuffixPrediction(
        DictionaryPredictor::SUFFIX,
        conversion_request, &segments, &allocator, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->node->value == kExpectedValue) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_F(DictionaryPredictorTest, TriggerNumberZeroQuerySuggestion) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  const POSMatcher &pos_matcher = data_and_predictor->pos_matcher();
  NodeAllocator allocator;
  const ConversionRequest conversion_request;

  const struct TestCase {
    const char *history_key;
    const char *history_value;
    const char *find_suffix_value;
    bool expected_result;
  } kTestCases[] = {
    // "月"
    { "12", "12", "\xe6\x9c\x88", true },
    // "１２", "月"
    { "12", "\xef\xbc\x91\xef\xbc\x92", "\xe6\x9c\x88", true },
    // "壱拾弐", "月"
    { "12", "\xe5\xa3\xb1\xe6\x8b\xbe\xe5\xbc\x90", "\xe6\x9c\x88", false },
    // "十二", "月"
    { "12", "\xe5\x8d\x81\xe4\xba\x8c", "\xe6\x9c\x88", false },
    // "一二", "月"
    { "12", "\xe4\xb8\x80\xe4\xba\x8c", "\xe6\x9c\x88", false },
    // "Ⅻ", "月"
    { "12", "\xe2\x85\xab", "\xe6\x9c\x88", false },
    // "あか", "月"
    { "\xe3\x81\x82\xe3\x81\x8b", "12", "\xe6\x9c\x88", true },  // T13N
    // "あか", "１２", "月"
    { "\xe3\x81\x82\xe3\x81\x8b", "\xef\xbc\x91\xef\xbc\x92",
      "\xe6\x9c\x88", true },  // T13N
    // "じゅう", "時"
    { "\xe3\x81\x98\xe3\x82\x85\xe3\x81\x86", "10", "\xe6\x99\x82", true },
    // "じゅう", "１０", "時"
    { "\xe3\x81\x98\xe3\x82\x85\xe3\x81\x86", "\xef\xbc\x91\xef\xbc\x90",
      "\xe6\x99\x82", true },
    // "じゅう", "十", "時"
    { "\xe3\x81\x98\xe3\x82\x85\xe3\x81\x86", "\xe5\x8d\x81",
      "\xe6\x99\x82", false },
    // "じゅう", "拾", "時"
    { "\xe3\x81\x98\xe3\x82\x85\xe3\x81\x86", "\xe6\x8b\xbe",
      "\xe6\x99\x82", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    Segments segments;
    MakeSegmentsForSuggestion("", &segments);

    const TestCase &test_case = kTestCases[i];
    PrependHistorySegments(
        test_case.history_key, test_case.history_value, &segments);
    vector<DictionaryPredictor::Result> results;
    predictor->AggregateSuffixPrediction(
        DictionaryPredictor::SUFFIX,
        conversion_request, &segments, &allocator, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->node->value == test_case.find_suffix_value &&
          it->node->lid == pos_matcher.GetCounterSuffixWordId()) {
        found = true;
        break;
      }
    }
    EXPECT_EQ(test_case.expected_result, found);
  }
}

TEST_F(DictionaryPredictorTest, GetHistoryKeyAndValue) {
  Segments segments;
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  MakeSegmentsForSuggestion("test", &segments);

  string key, value;
  EXPECT_FALSE(predictor->GetHistoryKeyAndValue(segments, &key, &value));

  PrependHistorySegments("key", "value", &segments);
  EXPECT_TRUE(predictor->GetHistoryKeyAndValue(segments, &key, &value));
  EXPECT_EQ("key", key);
  EXPECT_EQ("value", value);
}

TEST_F(DictionaryPredictorTest, IsZipCodeRequest) {
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest(""));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest("ABC"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("---"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-3456"));
  // "０１２-０"
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest(
      "\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92\x2d\xef\xbc\x90"));
}

TEST_F(DictionaryPredictorTest, IsAggressiveSuggestion) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // "ただしい",
  // "ただしいけめんにかぎる",
  EXPECT_TRUE(predictor->IsAggressiveSuggestion(
      4,      // query_len
      11,     // key_len
      6000,   // cost
      true,   // is_suggestion
      20));   // total_candidates_size

  // cost <= 4000
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      4,
      11,
      4000,
      true,
      20));

  // not suggestion
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      4,
      11,
      4000,
      false,
      20));

  // total_candidates_size is small
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      4,
      11,
      4000,
      true,
      5));

  // query_length = 5
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      5,
      11,
      6000,
      true,
      20));

  // "それでも",
  // "それでもぼくはやっていない",
  EXPECT_TRUE(predictor->IsAggressiveSuggestion(
      4,
      13,
      6000,
      true,
      20));

  // cost <= 4000
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      4,
      13,
      4000,
      true,
      20));
}

TEST_F(DictionaryPredictorTest, RealtimeConversionStartingWithAlphabets) {
  Segments segments;
  NodeAllocator allocator;
  // turn on real-time conversion
  config::Config config;
  config.set_use_dictionary_suggest(false);
  config.set_use_realtime_conversion(true);
  config::ConfigHandler::SetConfig(config);

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // "PCてすと"
  const char kKey[] = "PC\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
  // "PCテスト"
  const char *kExpectedSuggestionValues[] = {
    "Realtime top result",
    "PC\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88",
  };

  // Set up mock converter for realtime top result.
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kKey);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kExpectedSuggestionValues[0];
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }

  MakeSegmentsForSuggestion(kKey, &segments);

  vector<DictionaryPredictor::Result> results;

  ConversionRequest request;
  request.set_use_actual_converter_for_realtime_conversion(false);
  predictor->AggregateRealtimeConversion(
      DictionaryPredictor::REALTIME, request,
      &segments, &allocator, &results);
  ASSERT_EQ(1, results.size());

  EXPECT_EQ(DictionaryPredictor::REALTIME, results[0].types);
  EXPECT_EQ(kExpectedSuggestionValues[1], results[0].node->value);
  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, RealtimeConversionWithSpellingCorrection) {
  Segments segments;
  NodeAllocator allocator;
  // turn on real-time conversion
  config::Config config;
  config.set_use_dictionary_suggest(false);
  config.set_use_realtime_conversion(true);
  config::ConfigHandler::SetConfig(config);

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // "かぷりちょうざ"
  const char kCapriHiragana[] = "\xE3\x81\x8B\xE3\x81\xB7\xE3\x82\x8A"
      "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x96";

  // Set up mock converter for realtime top result.
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kCapriHiragana);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "Dummy";
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }

  MakeSegmentsForSuggestion(kCapriHiragana, &segments);

  vector<DictionaryPredictor::Result> results;

  ConversionRequest conversion_request;
  conversion_request.set_use_actual_converter_for_realtime_conversion(false);
  predictor->AggregateUnigramPrediction(
      DictionaryPredictor::UNIGRAM,
      conversion_request, &segments, &allocator, &results);
  ASSERT_FALSE(results.empty());
  EXPECT_TRUE(results[0].node->attributes & Node::SPELLING_CORRECTION);

  results.clear();

  // "かぷりちょうざで"
  const char kKeyWithDe[] = "\xE3\x81\x8B\xE3\x81\xB7\xE3\x82\x8A"
      "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x96\xE3\x81\xA7";
  // "カプリチョーザで"
  const char kExpectedSuggestionValueWithDe[] = "\xE3\x82\xAB\xE3\x83\x97"
      "\xE3\x83\xAA\xE3\x83\x81\xE3\x83\xA7\xE3\x83\xBC\xE3\x82\xB6"
      "\xE3\x81\xA7";

  MakeSegmentsForSuggestion(kKeyWithDe, &segments);
  predictor->AggregateRealtimeConversion(
      DictionaryPredictor::REALTIME, conversion_request,
      &segments, &allocator, &results);
  EXPECT_EQ(1, results.size());

  EXPECT_EQ(results[0].types, DictionaryPredictor::REALTIME);
  EXPECT_TRUE(results[0].node->attributes & Node::SPELLING_CORRECTION);
  EXPECT_EQ(kExpectedSuggestionValueWithDe, results[0].node->value);
  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, GetMissSpelledPosition) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  EXPECT_EQ(0, predictor->GetMissSpelledPosition("", ""));

  // EXPECT_EQ(3, predictor->GetMissSpelledPosition(
  //              "れみおめろん",
  //              "レミオロメン"));
  EXPECT_EQ(3, predictor->GetMissSpelledPosition(
      "\xE3\x82\x8C\xE3\x81\xBF\xE3\x81\x8A"
      "\xE3\x82\x81\xE3\x82\x8D\xE3\x82\x93",
      "\xE3\x83\xAC\xE3\x83\x9F\xE3\x82\xAA"
      "\xE3\x83\xAD\xE3\x83\xA1\xE3\x83\xB3"));

  // EXPECT_EQ(5, predictor->GetMissSpelledPosition
  //              "とーとばっく",
  //              "トートバッグ"));
  EXPECT_EQ(5, predictor->GetMissSpelledPosition(
      "\xE3\x81\xA8\xE3\x83\xBC\xE3\x81\xA8\xE3\x81\xB0"
      "\xE3\x81\xA3\xE3\x81\x8F",
      "\xE3\x83\x88\xE3\x83\xBC\xE3\x83\x88\xE3\x83\x90"
      "\xE3\x83\x83\xE3\x82\xB0"));

  // EXPECT_EQ(4, predictor->GetMissSpelledPosition(
  //               "おーすとりらあ",
  //               "オーストラリア"));
  EXPECT_EQ(4, predictor->GetMissSpelledPosition(
      "\xE3\x81\x8A\xE3\x83\xBC\xE3\x81\x99\xE3\x81\xA8"
      "\xE3\x82\x8A\xE3\x82\x89\xE3\x81\x82",
      "\xE3\x82\xAA\xE3\x83\xBC\xE3\x82\xB9\xE3\x83\x88"
      "\xE3\x83\xA9\xE3\x83\xAA\xE3\x82\xA2"));

  // EXPECT_EQ(7, predictor->GetMissSpelledPosition(
  //               "じきそうしょう",
  //               "時期尚早"));
  EXPECT_EQ(7, predictor->GetMissSpelledPosition(
      "\xE3\x81\x98\xE3\x81\x8D\xE3\x81\x9D\xE3\x81\x86"
      "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
      "\xE6\x99\x82\xE6\x9C\x9F\xE5\xB0\x9A\xE6\x97\xA9"));
}

TEST_F(DictionaryPredictorTest, RemoveMissSpelledCandidates) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  FreeList<Node> freelist(64);
  Node *node;

  {
    vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result result;
    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    node->attributes = Node::SPELLING_CORRECTION;
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっぐ";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x90";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バック";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xAF";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    predictor->RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(3, results.size());

    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[0].types);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              results[1].types);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[2].types);
  }

  {
    vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result result;
    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    node->attributes = Node::SPELLING_CORRECTION;
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "てすと";
    // node->value = "テスト";
    node->key = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
    node->value = "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    predictor->RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              results[0].types);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              results[1].types);
  }

  {
    vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result result;
    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    node->attributes = Node::SPELLING_CORRECTION;
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バック";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xAF";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    predictor->RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[0].types);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[1].types);
  }

  {
    vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result result;
    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    node->attributes = Node::SPELLING_CORRECTION;
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バック";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xAF";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    predictor->RemoveMissSpelledCandidates(3, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              results[0].types);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[1].types);
  }
}

TEST_F(DictionaryPredictorTest, LookupKeyValueFromDictionary) {
  Segments segments;
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  NodeAllocator allocator;

  // "てすと/テスト"
  EXPECT_TRUE(NULL != predictor->LookupKeyValueFromDictionary(
      "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8",
      "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88",
      &allocator));

  // "て/テ"
  EXPECT_TRUE(NULL == predictor->LookupKeyValueFromDictionary(
      "\xE3\x81\xA6",
      "\xE3\x83\x86",
      &allocator));
}

TEST_F(DictionaryPredictorTest, UseExpansionForUnigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForUnigramTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForUnigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForUnigramTestHelper(false);
}

TEST_F(DictionaryPredictorTest, UseExpansionForBigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForBigramTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForBigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForBigramTestHelper(false);
}

TEST_F(DictionaryPredictorTest, UseExpansionForSuffixTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForSuffixTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForSuffixTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForSuffixTestHelper(false);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForRomanTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  composer::Table table;
  table.LoadFromFile("system://romanji-hiragana.tsv");
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  NodeAllocator allocator;

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  composer::Composer composer(&table, &default_request());
  InsertInputSequence("ak", &composer);
  const ConversionRequest request(&composer, &default_request());
  Segment *segment = segments.add_segment();
  CHECK(segment);
  {
    string query;
    composer.GetQueryForPrediction(&query);
    segment->set_key(query);
    // "あ"
    EXPECT_EQ("\xe3\x81\x82", query);
  }
  {
    string base;
    set<string> expanded;
    composer.GetQueriesForPrediction(&base, &expanded);
    // "あ"
    EXPECT_EQ("\xe3\x81\x82", base);
    EXPECT_GT(expanded.size(), 5);
  }

  vector<TestableDictionaryPredictor::Result> results;
  Node node1;
  // "あか"
  node1.key = "\xe3\x81\x82\xe3\x81\x8b";
  // "赤"
  node1.value = "\xe8\xb5\xa4";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node1, TestableDictionaryPredictor::UNIGRAM));
  Node node2;
  // "あき"
  node2.key = "\xe3\x81\x82\xe3\x81\x8d";
  // "秋"
  node2.value = "\xe7\xa7\x8b";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node2, TestableDictionaryPredictor::UNIGRAM));
  Node node3;
  // "あかぎ"
  node3.key = "\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x8e";
  // "アカギ"
  node3.value = "\xe3\x82\xa2\xe3\x82\xab\xe3\x82\xae";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node3, TestableDictionaryPredictor::UNIGRAM));

  EXPECT_EQ(3, results.size());
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);

  predictor->ApplyPenaltyForKeyExpansion(segments, &results);

  // no penalties
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForKanaTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  composer::Table table;
  table.LoadFromFile("system://kana.tsv");
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  NodeAllocator allocator;

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  composer::Composer composer(&table, &default_request());
  // "あし"
  InsertInputSequence("\xe3\x81\x82\xe3\x81\x97", &composer);
  const ConversionRequest request(&composer, &default_request());
  Segment *segment = segments.add_segment();
  CHECK(segment);
  {
    string query;
    composer.GetQueryForPrediction(&query);
    segment->set_key(query);
    // "あし"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x97", query);
  }
  {
    string base;
    set<string> expanded;
    composer.GetQueriesForPrediction(&base, &expanded);
    // "あ"
    EXPECT_EQ("\xe3\x81\x82", base);
    EXPECT_EQ(2, expanded.size());
  }

  vector<TestableDictionaryPredictor::Result> results;
  Node node1;
  // "あし"
  node1.key = "\xe3\x81\x82\xe3\x81\x97";
  // "足"
  node1.value = "\xe8\xb6\xb3";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node1, TestableDictionaryPredictor::UNIGRAM));
  Node node2;
  // "あじ"
  node2.key = "\xe3\x81\x82\xe3\x81\x98";
  // "味"
  node2.value = "\xe5\x91\xb3";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node2, TestableDictionaryPredictor::UNIGRAM));
  Node node3;
  // "あした"
  node3.key = "\xe3\x81\x82\xe3\x81\x97\xe3\x81\x9f";
  // "明日"
  node3.value = "\xe6\x98\x8e\xe6\x97\xa5";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node3, TestableDictionaryPredictor::UNIGRAM));
  Node node4;
  // "あじあ"
  node4.key = "\xe3\x81\x82\xe3\x81\x98\xe3\x81\x82";
  // "アジア"
  node4.value = "\xe3\x82\xa2\xe3\x82\xb8\xe3\x82\xa2";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node4, TestableDictionaryPredictor::UNIGRAM));

  EXPECT_EQ(4, results.size());
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
  EXPECT_EQ(0, results[3].cost);

  predictor->ApplyPenaltyForKeyExpansion(segments, &results);

  EXPECT_EQ(0, results[0].cost);
  EXPECT_LT(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
  EXPECT_LT(0, results[3].cost);
}

TEST_F(DictionaryPredictorTest, SetLMCost) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "てすと"
  segment->set_key("\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8");

  vector<TestableDictionaryPredictor::Result> results;
  Node node1;
  // "てすと"
  node1.key = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
  // "てすと"
  node1.value = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node1, TestableDictionaryPredictor::UNIGRAM));
  Node node2;
  // "てすと"
  node2.key = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
  // "テスト"
  node2.value = "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node2, TestableDictionaryPredictor::UNIGRAM));
  Node node3;
  // "てすとてすと"
  node3.key = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8"
      "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
  // "テストテスト"
  node3.value = "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
      "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node3, TestableDictionaryPredictor::UNIGRAM));

  predictor->SetLMCost(segments, &results);

  EXPECT_EQ(3, results.size());
  // "てすと"
  EXPECT_EQ("\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8", results[0].node->value);
  // "テスト"
  EXPECT_EQ("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", results[1].node->value);
  // "テストテスト"
  EXPECT_EQ("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
            "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", results[2].node->value);
  EXPECT_GT(results[2].cost, results[0].cost);
  EXPECT_GT(results[2].cost, results[1].cost);
}

TEST_F(DictionaryPredictorTest, SuggestSpellingCorrection) {
  testing::MockDataManager data_manager;

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  // "あぼがど"
  MakeSegmentsForPrediction(
      "\xe3\x81\x82\xe3\x81\xbc\xe3\x81\x8c\xe3\x81\xa9", &segments);

  NodeAllocator allocator;

  predictor->PredictForRequest(default_conversion_request(), &segments);

  EXPECT_TRUE(FindCandidateByValue(
      segments.conversion_segment(0),
      // "アボカド"
      "\xe3\x82\xa2\xe3\x83\x9c\xe3\x82\xab\xe3\x83\x89"));
}

TEST_F(DictionaryPredictorTest, DoNotSuggestSpellingCorrectionBeforeMismatch) {
  testing::MockDataManager data_manager;

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  // "あぼが"
  MakeSegmentsForPrediction(
      "\xe3\x81\x82\xe3\x81\xbc\xe3\x81\x8c", &segments);

  NodeAllocator allocator;

  predictor->PredictForRequest(default_conversion_request(), &segments);

  EXPECT_FALSE(FindCandidateByValue(
      segments.conversion_segment(0),
      // "アボカド"
      "\xe3\x82\xa2\xe3\x83\x9c\xe3\x82\xab\xe3\x83\x89"));
}

TEST_F(DictionaryPredictorTest, MobileUnigramSuggestion) {
  testing::MockDataManager data_manager;

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  // "とうきょう"
  const char kKey[] =
      "\xe3\x81\xa8\xe3\x81\x86\xe3\x81\x8d\xe3\x82\x87\xe3\x81\x86";

  MakeSegmentsForSuggestion(kKey, &segments);

  NodeAllocator allocator;
  commands::Request request;
  commands::RequestForUnitTest::FillMobileRequest(&request);
  const ConversionRequest conversion_request(&default_composer(), &request);

  vector<TestableDictionaryPredictor::Result> results;
  predictor->AggregateUnigramPrediction(
      TestableDictionaryPredictor::UNIGRAM,
      conversion_request, &segments, &allocator, &results);

  // "東京"
  EXPECT_TRUE(FindResultByValue(results, "\xe6\x9d\xb1\xe4\xba\xac"));

  int prefix_count = 0;
  for (size_t i = 0; i < results.size(); ++i) {
    // "東京"
    if (Util::StartsWith(results[i].node->value, "\xe6\x9d\xb1\xe4\xba\xac")) {
      ++prefix_count;
    }
  }
  // Should not have same prefix candidates a lot.
  EXPECT_LE(prefix_count, 6);
}

TEST_F(DictionaryPredictorTest, MobileZeroQuerySuggestion) {
  testing::MockDataManager data_manager;

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  MakeSegmentsForPrediction("", &segments);

  // "だいがく"
  PrependHistorySegments("\xe3\x81\xa0\xe3\x81\x84\xe3\x81\x8c\xe3\x81\x8f",
                         // "大学"
                         "\xe5\xa4\xa7\xe5\xad\xa6",
                         &segments);

  NodeAllocator allocator;
  commands::Request request;
  commands::RequestForUnitTest::FillMobileRequest(&request);
  const ConversionRequest conversion_request(&default_composer(), &request);

  predictor->PredictForRequest(conversion_request, &segments);

  EXPECT_TRUE(FindCandidateByValue(segments.conversion_segment(0),
                                   // "入試"
                                   "\xe5\x85\xa5\xe8\xa9\xa6"));
  EXPECT_TRUE(FindCandidateByValue(
      segments.conversion_segment(0),
      // "入試センター"
      "\xe5\x85\xa5\xe8\xa9\xa6\xe3\x82\xbb\xe3\x83\xb3"
      "\xe3\x82\xbf\xe3\x83\xbc"));
}

// We are not sure what should we suggest after the end of sentence for now.
// However, we decided to show zero query suggestion rather than stopping
// zero query completely. Users may be confused if they cannot see suggestion
// window only after the certain conditions.
// TODO(toshiyuki): Show useful zero query suggestions after EOS.
TEST_F(DictionaryPredictorTest, DISABLED_MobileZeroQuerySuggestionAfterEOS) {
  testing::MockDataManager data_manager;

  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  commands::Request request;
  commands::RequestForUnitTest::FillMobileRequest(&request);

  const POSMatcher &pos_matcher = data_and_predictor->pos_matcher();

  const struct TestCase {
    const char *key;
    const char *value;
    int rid;
    bool expected_result;
  } kTestcases[] = {
    // "ですよね｡", "ですよね。"
    { "\xe3\x81\xa7\xe3\x81\x99\xe3\x82\x88\xe3\x81\xad\xef\xbd\xa1",
      "\xe3\x81\xa7\xe3\x81\x99\xe3\x82\x88\xe3\x81\xad\xe3\x80\x82",
      pos_matcher.GetEOSSymbolId(),
      false },
    // "｡", "。"
    { "\xef\xbd\xa1",
      "\xe3\x80\x82",
      pos_matcher.GetEOSSymbolId(),
      false },
    // "まるいち", "①"
    { "\xe3\x81\xbe\xe3\x82\x8b\xe3\x81\x84\xe3\x81\xa1",
      "\xe2\x91\xa0",
      pos_matcher.GetEOSSymbolId(),
      false },
    // "そう", "そう"
    { "\xe3\x81\x9d\xe3\x81\x86",
      "\xe3\x81\x9d\xe3\x81\x86",
      pos_matcher.GetGeneralNounId(),
      true },
    // "そう!", "そう！"
    { "\xe3\x81\x9d\xe3\x81\x86\x21",
      "\xe3\x81\x9d\xe3\x81\x86\xef\xbc\x81",
      pos_matcher.GetGeneralNounId(),
      false },
    // "むすめ。", "娘。"
    { "\xe3\x82\x80\xe3\x81\x99\xe3\x82\x81\xe3\x80\x82",
      "\xe5\xa8\x98\xe3\x80\x82",
      pos_matcher.GetUniqueNounId(),
      true },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestcases); ++i) {
    const TestCase &test_case = kTestcases[i];

    Segments segments;
    MakeSegmentsForPrediction("", &segments);

    Segment *seg = segments.push_front_segment();
    seg->set_segment_type(Segment::HISTORY);
    seg->set_key(test_case.key);
    Segment::Candidate *c = seg->add_candidate();
    c->key = test_case.key;
    c->content_key = test_case.key;
    c->value = test_case.value;
    c->content_value = test_case.value;
    c->rid = test_case.rid;

    predictor->PredictForRequest(default_conversion_request(), &segments);
    const bool candidates_inserted =
        segments.conversion_segment(0).candidates_size() > 0;
    EXPECT_EQ(test_case.expected_result, candidates_inserted);
  }
}

TEST_F(DictionaryPredictorTest, PropagateUserDictionaryAttribute) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(true);
  config::ConfigHandler::SetConfig(config);

  {
    segments.Clear();
    segments.set_max_prediction_candidates_size(10);
    segments.set_request_type(Segments::SUGGESTION);
    Segment *seg = segments.add_segment();
    // "ゆーざー"
    seg->set_key("\xe3\x82\x86\xe3\x83\xbc\xe3\x81\x96\xe3\x83\xbc");
    seg->set_segment_type(Segment::FREE);
    EXPECT_TRUE(predictor->PredictForRequest(default_conversion_request(),
                                             &segments));
    EXPECT_EQ(1, segments.conversion_segments_size());
    bool find_yuza_candidate = false;
    for (size_t i = 0;
         i < segments.conversion_segment(0).candidates_size();
         ++i) {
      const Segment::Candidate &cand =
          segments.conversion_segment(0).candidate(i);
      // "ユーザー"
      if (cand.value == "\xe3\x83\xa6\xe3\x83\xbc\xe3\x82\xb6\xe3\x83\xbc" &&
          (cand.attributes & (Segment::Candidate::NO_VARIANTS_EXPANSION |
                              Segment::Candidate::USER_DICTIONARY))) {
        find_yuza_candidate = true;
      }
    }
    EXPECT_TRUE(find_yuza_candidate);
  }

  {
    segments.Clear();
    segments.set_max_prediction_candidates_size(10);
    segments.set_request_type(Segments::SUGGESTION);
    Segment *seg = segments.add_segment();
    // "ゆーざーの"
    seg->set_key(
        "\xe3\x82\x86\xe3\x83\xbc\xe3\x81\x96\xe3\x83\xbc\xe3\x81\xae");
    seg->set_segment_type(Segment::FREE);
    EXPECT_TRUE(predictor->PredictForRequest(default_conversion_request(),
                                             &segments));
    EXPECT_EQ(1, segments.conversion_segments_size());
    bool find_yuza_candidate = false;
    for (size_t i = 0;
         i < segments.conversion_segment(0).candidates_size();
         ++i) {
      const Segment::Candidate &cand =
          segments.conversion_segment(0).candidate(i);
      // "ユーザーの"
      if ((cand.value ==
           "\xe3\x83\xa6\xe3\x83\xbc\xe3\x82\xb6\xe3\x83\xbc\xe3\x81\xae") &&
          (cand.attributes & (Segment::Candidate::NO_VARIANTS_EXPANSION |
                              Segment::Candidate::USER_DICTIONARY))) {
        find_yuza_candidate = true;
      }
    }
    EXPECT_TRUE(find_yuza_candidate);
  }
}

TEST_F(DictionaryPredictorTest, SetDescription) {
  {
    string description;
    DictionaryPredictor::SetDescription(
        TestableDictionaryPredictor::TYPING_CORRECTION, 0, &description);
    // "<入力補正>"
    EXPECT_EQ("<\xE5\x85\xA5\xE5\x8A\x9B\xE8\xA3\x9C\xE6\xAD\xA3>",
              description);

    description.clear();
    DictionaryPredictor::SetDescription(
        0, Segment::Candidate::AUTO_PARTIAL_SUGGESTION, &description);
    // "<部分確定>"
    EXPECT_EQ("<\xE9\x83\xA8\xE5\x88\x86\xE7\xA2\xBA\xE5\xAE\x9A>",
              description);
  }
}

TEST_F(DictionaryPredictorTest, SetDebugDescription) {
  {
    string description;
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::UNIGRAM |
        TestableDictionaryPredictor::ENGLISH;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("Unigram English", description);
  }
  {
    string description = "description";
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::REALTIME |
        TestableDictionaryPredictor::BIGRAM;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("description Bigram Realtime", description);
  }
  {
    string description;
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::BIGRAM |
        TestableDictionaryPredictor::REALTIME |
        TestableDictionaryPredictor::SUFFIX;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("Bigram Realtime Suffix", description);
  }
}

TEST_F(DictionaryPredictorTest, PropagateRealtimeConversionBoundary) {
  testing::MockDataManager data_manager;
  scoped_ptr<const DictionaryInterface> dictionary(new DictionaryMock);
  scoped_ptr<ConverterInterface> converter(new ConverterMock);
  scoped_ptr<ImmutableConverterInterface> immutable_converter(
      new ImmutableConverterMock);
  scoped_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  scoped_ptr<const ConnectorInterface> connector(
      ConnectorBase::CreateFromDataManager(data_manager));
  scoped_ptr<const SegmenterInterface> segmenter(
      SegmenterBase::CreateFromDataManager(data_manager));
  scoped_ptr<const SuggestionFilter> suggestion_filter(
      CreateSuggestionFilter(data_manager));
  scoped_ptr<TestableDictionaryPredictor> predictor(
      new TestableDictionaryPredictor(converter.get(),
                                      immutable_converter.get(),
                                      dictionary.get(),
                                      suffix_dictionary.get(),
                                      connector.get(),
                                      segmenter.get(),
                                      data_manager.GetPOSMatcher(),
                                      suggestion_filter.get()));
  Segments segments;
  // "わたしのなまえはなかのです"
  const char kKey[] =
      "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97"
      "\xE3\x81\xAE\xE3\x81\xAA\xE3\x81\xBE"
      "\xE3\x81\x88\xE3\x81\xAF\xE3\x81\xAA"
      "\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99";
  MakeSegmentsForSuggestion(kKey, &segments);

  vector<TestableDictionaryPredictor::Result> results;
  NodeAllocator allocator;
  predictor->AggregateRealtimeConversion(
      TestableDictionaryPredictor::REALTIME, default_conversion_request(),
      &segments, &allocator, &results);

  // mock results
  EXPECT_EQ(1, results.size());
  predictor->AddPredictionToCandidates(default_conversion_request(),
                                       &segments, &results);
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(1, segments.conversion_segment(0).candidates_size());
  const Segment::Candidate &cand = segments.conversion_segment(0).candidate(0);
  // "わたしのなまえはなかのです"
  EXPECT_EQ("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae\xe3\x81\xaa\xe3"
            "\x81\xbe\xe3\x81\x88\xe3\x81\xaf\xe3\x81\xaa\xe3\x81\x8b\xe3\x81"
            "\xae\xe3\x81\xa7\xe3\x81\x99", cand.key);
  // "私の名前は中野です"
  EXPECT_EQ("\xe7\xa7\x81\xe3\x81\xae\xe5\x90\x8d\xe5\x89\x8d\xe3\x81\xaf\xe4"
            "\xb8\xad\xe9\x87\x8e\xe3\x81\xa7\xe3\x81\x99", cand.value);
  EXPECT_EQ(3, cand.inner_segment_boundary.size());
}

TEST_F(DictionaryPredictorTest, PropagateResultCosts) {
  scoped_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  NodeAllocator allocator;
  vector<TestableDictionaryPredictor::Result> results;
  const int kTestSize = 20;
  for (size_t i = 0; i < kTestSize; ++i) {
    Node *node = allocator.NewNode();
    node->Init();
    node->wcost = i;
    node->cost = i + 100;
    node->key = string(1, 'a' + i);
    node->value = string(1, 'A' + i);
    results.push_back(TestableDictionaryPredictor::MakeResult(
        node, TestableDictionaryPredictor::REALTIME));
    results.back().cost = i + 1000;
  }
  random_shuffle(results.begin(), results.end());

  Segments segments;
  MakeSegmentsForSuggestion("test", &segments);
  segments.set_max_prediction_candidates_size(kTestSize);

  predictor->AddPredictionToCandidates(default_conversion_request(),
                                       &segments, &results);

  EXPECT_EQ(1, segments.conversion_segments_size());
  ASSERT_EQ(kTestSize, segments.conversion_segment(0).candidates_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ(i + 1000, segment.candidate(i).cost);
  }
}
}  // namespace mozc
