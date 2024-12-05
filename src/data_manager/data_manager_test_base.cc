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

#include "data_manager/data_manager_test_base.h"

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "converter/connector.h"
#include "converter/node.h"
#include "converter/segmenter.h"
#include "data_manager/connection_file_reader.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_matcher.h"
#include "prediction/suggestion_filter.h"
#include "testing/gunit.h"

namespace mozc {
namespace {
using ::mozc::dictionary::PosMatcher;
}  // namespace

DataManagerTestBase::DataManagerTestBase(
    DataManagerInterface *data_manager, const size_t lsize, const size_t rsize,
    IsBoundaryFunc is_boundary, const std::string &connection_txt_file,
    const int expected_resolution, std::vector<std::string> dictionary_files,
    std::vector<std::string> suggestion_filter_files)
    : data_manager_(data_manager),
      lsize_(lsize),
      rsize_(rsize),
      is_boundary_(is_boundary),
      connection_txt_file_(connection_txt_file),
      expected_resolution_(expected_resolution),
      dictionary_files_(std::move(dictionary_files)),
      suggestion_filter_files_(std::move(suggestion_filter_files)) {}

void DataManagerTestBase::SegmenterTest_SameAsInternal() {
  // This test verifies that a segmenter created by MockDataManager provides
  // the expected boundary rule.
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      EXPECT_EQ(is_boundary_(rid, lid), segmenter->IsBoundary(rid, lid))
          << rid << " " << lid;
    }
  }
}

void DataManagerTestBase::SegmenterTest_LNodeTest() {
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));

  // lnode is BOS
  Node lnode, rnode;
  lnode.node_type = Node::BOS_NODE;
  rnode.node_type = Node::NOR_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      lnode.lid = lid;
      EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, false));
      EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_RNodeTest() {
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));

  // rnode is EOS
  Node lnode, rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::EOS_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      lnode.lid = lid;
      EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, false));
      EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_NodeTest() {
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));

  Node lnode, rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      rnode.lid = lid;
      EXPECT_EQ(segmenter->IsBoundary(rid, lid),
                segmenter->IsBoundary(lnode, rnode, false));
      EXPECT_FALSE(segmenter->IsBoundary(lnode, rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_ParticleTest() {
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));
  const PosMatcher pos_matcher(data_manager_->GetPosMatcherData());

  Node lnode, rnode;
  lnode.Init();
  rnode.Init();
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  // "助詞"
  lnode.rid = pos_matcher.GetAcceptableParticleAtBeginOfSegmentId();
  // "名詞,サ変".
  rnode.lid = pos_matcher.GetUnknownId();
  EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, false));

  lnode.attributes |= Node::STARTS_WITH_PARTICLE;
  EXPECT_FALSE(segmenter->IsBoundary(lnode, rnode, false));
}

void DataManagerTestBase::ConnectorTest_RandomValueCheck() {
  auto status_or_connector = Connector::CreateFromDataManager(*data_manager_);
  ASSERT_TRUE(status_or_connector.ok()) << status_or_connector.status();
  auto connector = std::move(status_or_connector).value();

  EXPECT_EQ(connector.GetResolution(), expected_resolution_);
  absl::BitGen gen;
  for (ConnectionFileReader reader(connection_txt_file_); !reader.done();
       reader.Next()) {
    // Randomly sample test entries because connection data have several
    // millions of entries.
    if (!absl::Bernoulli(gen, 1.0 / 100000)) {
      continue;
    }
    const int cost = reader.cost();
    EXPECT_GE(cost, 0);
    const int actual_cost = connector.GetTransitionCost(
        reader.rid_of_left_node(), reader.lid_of_right_node());
    if (cost == Connector::kInvalidCost) {
      EXPECT_EQ(actual_cost, cost);
    } else {
      EXPECT_TRUE(cost == actual_cost ||
                  (cost - cost % expected_resolution_) == actual_cost)
          << "cost: " << cost << ", actual_cost: " << actual_cost;
    }
  }
}

void DataManagerTestBase::SuggestionFilterTest_IsBadSuggestion() {
  constexpr double kErrorRatio = 0.0001;

  // Load embedded suggestion filter (bloom filter)
  SuggestionFilter suggestion_filter =
      SuggestionFilter::CreateOrDie(data_manager_->GetSuggestionFilterData());

  // Load the original suggestion filter from file.
  absl::flat_hash_set<std::string> suggestion_filter_set;

  for (size_t i = 0; i < suggestion_filter_files_.size(); ++i) {
    InputFileStream input(suggestion_filter_files_[i]);
    CHECK(input) << "cannot open: " << suggestion_filter_files_[i];
    std::string line;
    while (std::getline(input, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      Util::LowerString(&line);
      suggestion_filter_set.insert(line);
    }
  }

  LOG(INFO) << "Filter word size:\t" << suggestion_filter_set.size();

  size_t false_positives = 0;
  size_t num_words = 0;
  for (size_t i = 0; i < dictionary_files_.size(); ++i) {
    InputFileStream input(dictionary_files_[i]);
    CHECK(input) << "cannot open: " << dictionary_files_[i];
    std::string line;
    while (std::getline(input, line)) {
      std::vector<std::string> fields =
          absl::StrSplit(line, '\t', absl::SkipEmpty());
      CHECK_GE(fields.size(), 5);
      std::string value = fields[4];
      Util::LowerString(&value);

      const bool true_result =
          (suggestion_filter_set.find(value) != suggestion_filter_set.end());
      const bool bloom_filter_result = suggestion_filter.IsBadSuggestion(value);

      // never emits false negative
      if (true_result) {
        EXPECT_TRUE(bloom_filter_result) << value;
      } else {
        if (bloom_filter_result) {
          ++false_positives;
          LOG(INFO) << value << " is false positive";
        }
      }
      ++num_words;
    }
  }

  const float error_ratio = 1.0 * false_positives / num_words;

  LOG(INFO) << "False positive ratio is " << error_ratio;

  EXPECT_LT(error_ratio, kErrorRatio);
}

void DataManagerTestBase::CounterSuffixTest_ValidateTest() {
  SerializedStringArray suffix_array;
  ASSERT_TRUE(suffix_array.Init(data_manager_->GetCounterSuffixSortedArray()));

  // Check if the array is sorted in ascending order.
  absl::string_view prev_suffix;  // The smallest string.
  for (size_t i = 0; i < suffix_array.size(); ++i) {
    const absl::string_view suffix = suffix_array[i];
    EXPECT_LE(prev_suffix, suffix);
    prev_suffix = suffix;
  }
}

void DataManagerTestBase::RunAllTests() {
  ConnectorTest_RandomValueCheck();
  SegmenterTest_LNodeTest();
  SegmenterTest_NodeTest();
  SegmenterTest_ParticleTest();
  SegmenterTest_RNodeTest();
  SegmenterTest_SameAsInternal();
  SuggestionFilterTest_IsBadSuggestion();
  CounterSuffixTest_ValidateTest();
}

}  // namespace mozc
