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

#include "storage/existence_filter.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/hash.h"
#include "base/logging.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/status/statusor.h"

namespace mozc {
namespace storage {
namespace {

void CheckValues(const ExistenceFilter &filter, int m, int n) {
  int false_positives = 0;
  for (int i = 0; i < 2 * n; ++i) {
    uint64_t hash = Hash::Fingerprint(i);
    bool should_exist = ((i % 2) == 0);
    bool actual = filter.Exists(hash);
    if (should_exist) {
      CHECK(actual) << " Value = " << i;
    } else {
      if (actual) {
        ++false_positives;
      }
    }
  }

  LOG(INFO) << "false_positives: " << false_positives;
}

void RunTest(int m, int n) {
  LOG(INFO) << "Test " << m << " " << n;
  ExistenceFilter filter = ExistenceFilter::CreateOptimal(m, n);

  for (int i = 0; i < n; ++i) {
    int val = i * 2;
    uint64_t hash = Hash::Fingerprint(val);
    filter.Insert(hash);
  }

  CheckValues(filter, m, n);

  const std::string buf = filter.Write();
  LOG(INFO) << "write size: " << buf.size();
  absl::StatusOr<ExistenceFilter> filter2 = ExistenceFilter::Read(buf);
  EXPECT_OK(filter2);
  CheckValues(*filter2, m, n);
}

}  // namespace

TEST(ExistenceFilterTest, RunTest) {
  int n = 50000;
  int m = ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.01, 50000);
  RunTest(m, n);
}

TEST(ExistenceFilterTest, MinFilterSizeEstimateTest) {
  EXPECT_EQ(ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.1, 100), 61);
  EXPECT_EQ(ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.01, 100), 120);
  EXPECT_EQ(ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.05, 100), 79);
  EXPECT_EQ(ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.05, 1000), 781);
}

TEST(ExistenceFilterTest, ReadWriteTest) {
  const std::vector<std::string> words = {"a", "b", "c"};

  static constexpr float kErrorRate = 0.0001;
  int num_bytes = ExistenceFilter::MinFilterSizeInBytesForErrorRate(
      kErrorRate, words.size());

  ExistenceFilter filter(
      ExistenceFilter::CreateOptimal(num_bytes, words.size()));

  for (int i = 0; i < words.size(); ++i) {
    filter.Insert(Hash::Fingerprint(words[i]));
  }

  const std::string buf = filter.Write();
  absl::StatusOr<ExistenceFilter> filter_read(ExistenceFilter::Read(buf));
  EXPECT_OK(filter_read);

  for (const std::string &word : words) {
    EXPECT_TRUE(filter_read->Exists(Hash::Fingerprint(word)));
  }
}

TEST(ExistenceFilterTest, InsertAndExistsTest) {
  const std::vector<std::string> words = {"a", "b", "c", "d", "e",
                                          "f", "g", "h", "i"};

  static constexpr float kErrorRate = 0.0001;
  int num_bytes = ExistenceFilter::MinFilterSizeInBytesForErrorRate(
      kErrorRate, words.size());

  ExistenceFilter filter(
      ExistenceFilter::CreateOptimal(num_bytes, words.size()));

  for (const std::string &word : words) {
    filter.Insert(Hash::Fingerprint(word));
  }

  for (const std::string &word : words) {
    EXPECT_TRUE(filter.Exists(Hash::Fingerprint(word)));
  }
}

}  // namespace storage
}  // namespace mozc
