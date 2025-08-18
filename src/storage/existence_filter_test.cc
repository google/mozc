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
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/hash.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace storage {
namespace {

void CheckValues(const ExistenceFilter& filter, int m, int n) {
  int false_positives = 0;
  for (int i = 0; i < 2 * n; ++i) {
    uint64_t hash = Fingerprint(i);
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

std::vector<uint32_t> StringToAlignedBuffer(const absl::string_view str) {
  std::vector<uint32_t> aligned_buf(str.size() / sizeof(uint32_t));
  memcpy(aligned_buf.data(), str.data(), str.size());
  return aligned_buf;
}

void RunTest(int m, int n) {
  LOG(INFO) << "Test " << m << " " << n;
  ExistenceFilterBuilder builder = ExistenceFilterBuilder::CreateOptimal(m, n);

  for (int i = 0; i < n; ++i) {
    int val = i * 2;
    uint64_t hash = Fingerprint(val);
    builder.Insert(hash);
  }

  ExistenceFilter filter = builder.Build();
  CheckValues(filter, m, n);

  const std::string buf = builder.SerializeAsString();
  LOG(INFO) << "write size: " << buf.size();
  const std::vector<uint32_t> aligned_buf = StringToAlignedBuffer(buf);
  absl::StatusOr<ExistenceFilter> filter2 = ExistenceFilter::Read(aligned_buf);
  EXPECT_OK(filter2);
  CheckValues(*filter2, m, n);
}

TEST(ExistenceFilterTest, RunTest) {
  int n = 50000;
  int m = ExistenceFilterBuilder::MinFilterSizeInBytesForErrorRate(0.01, 50000);
  RunTest(m, n);
}

TEST(ExistenceFilterTest, MinFilterSizeEstimateTest) {
  EXPECT_EQ(ExistenceFilterBuilder::MinFilterSizeInBytesForErrorRate(0.1, 100),
            61);
  EXPECT_EQ(ExistenceFilterBuilder::MinFilterSizeInBytesForErrorRate(0.01, 100),
            120);
  EXPECT_EQ(ExistenceFilterBuilder::MinFilterSizeInBytesForErrorRate(0.05, 100),
            79);
  EXPECT_EQ(
      ExistenceFilterBuilder::MinFilterSizeInBytesForErrorRate(0.05, 1000),
      781);
}

TEST(ExistenceFilterTest, ReadWriteTest) {
  constexpr absl::string_view kWords[] = {"a", "b", "c"};

  static constexpr float kErrorRate = 0.0001;
  int num_bytes = ExistenceFilterBuilder::MinFilterSizeInBytesForErrorRate(
      kErrorRate, std::size(kWords));

  ExistenceFilterBuilder builder(
      ExistenceFilterBuilder::CreateOptimal(num_bytes, std::size(kWords)));

  for (const absl::string_view& word : kWords) {
    builder.Insert(Fingerprint(word));
  }

  const std::string buf = builder.SerializeAsString();
  const std::vector<uint32_t> aligned_buf = StringToAlignedBuffer(buf);
  absl::StatusOr<ExistenceFilter> filter_read(
      ExistenceFilter::Read(aligned_buf));
  EXPECT_OK(filter_read);

  for (const absl::string_view& word : kWords) {
    EXPECT_TRUE(filter_read->Exists(Fingerprint(word)));
  }
}

TEST(ExistenceFilterTest, InsertAndExistsTest) {
  const std::vector<std::string> words = {"a", "b", "c", "d", "e",
                                          "f", "g", "h", "i"};

  static constexpr float kErrorRate = 0.0001;
  int num_bytes = ExistenceFilterBuilder::MinFilterSizeInBytesForErrorRate(
      kErrorRate, words.size());

  ExistenceFilterBuilder builder(
      ExistenceFilterBuilder::CreateOptimal(num_bytes, words.size()));

  for (const std::string& word : words) {
    builder.Insert(Fingerprint(word));
  }

  ExistenceFilter filter = builder.Build();

  for (const std::string& word : words) {
    EXPECT_TRUE(filter.Exists(Fingerprint(word)));
  }
}

}  // namespace
}  // namespace storage
}  // namespace mozc
