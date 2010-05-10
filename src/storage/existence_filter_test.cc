// Copyright 2010, Google Inc.
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

#include <string>
#include <vector>
#include <utility>
#include "base/google.h"
#include "storage/existence_filter.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "strings/strutil.h"

namespace mozc {

void CheckValues(ExistenceFilter* filter, int m, int n) {
  int false_positives = 0;
  for (int i = 0; i < 2 * n; ++i) {
    uint64 hash = Fingerprint(i);
    bool should_exist = ((i%2) == 0);
    bool actual = filter->Exists(hash);
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
  ExistenceFilter *filter = ExistenceFilter::CreateOptimal(m, n);

  for (int i = 0; i < n; ++i) {
    int val = i * 2;
    uint64 hash = Fingerprint(val);
    filter->Insert(hash);
  }

  CheckValues(filter, m, n);

  char *buf = NULL;
  size_t size = 0;
  filter->Write(&buf, &size);
  LOG(INFO) << "write size: " << size;
  ExistenceFilter *filter2 = ExistenceFilter::Read(buf, size);
  CheckValues(filter2, m, n);
  delete filter2;
  delete[] buf;

  delete filter;
}

TEST(ExistenceFilterTest, RunTest) {
  int n = 50000;
  int m = ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.01, 50000);
  RunTest(m, n);
}

TEST(ExistenceFilterTest, MinFilterSizeEstimateTest) {
  EXPECT_EQ(61,
            ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.1, 100));
  EXPECT_EQ(120,
            ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.01, 100));
  EXPECT_EQ(79,
            ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.05, 100));
  EXPECT_EQ(781,
            ExistenceFilter::MinFilterSizeInBytesForErrorRate(0.05, 1000));
}

TEST(ExistenceFilterTest, ReadWriteTest) {
  vector<string> words;
  words.push_back("a");
  words.push_back("b");
  words.push_back("c");

  static const float kErrorRate = 0.0001;
  int num_bytes =
      ExistenceFilter::MinFilterSizeInBytesForErrorRate(kErrorRate,
                                                        words.size());

  scoped_ptr<ExistenceFilter> filter(
      ExistenceFilter::CreateOptimal(num_bytes, words.size()));

  for (int i = 0; i < words.size(); ++i) {
    filter->Insert(Fingerprint(words[i]));
  }

  char *buf = NULL;
  size_t size = 0;
  filter->Write(&buf, &size);
  scoped_ptr<ExistenceFilter> filter_read(
      ExistenceFilter::Read(buf, size));

  for (int i = 0; i < words.size(); ++i) {
    EXPECT_TRUE(filter_read->Exists(Fingerprint(words[i])));
  }

  delete [] buf;
};

TEST(ExistenceFilterTest, InsertAndExistsTest) {
  vector<string> words;
  words.push_back("a");
  words.push_back("b");
  words.push_back("c");
  words.push_back("d");
  words.push_back("e");
  words.push_back("f");
  words.push_back("g");
  words.push_back("h");
  words.push_back("i");

  static const float kErrorRate = 0.0001;
  int num_bytes =
      ExistenceFilter::MinFilterSizeInBytesForErrorRate(kErrorRate,
                                                        words.size());

  scoped_ptr<ExistenceFilter> filter(
      ExistenceFilter::CreateOptimal(num_bytes, words.size()));

  for (int i = 0; i < words.size(); ++i) {
    filter->Insert(Fingerprint(words[i]));
  }

  for (int i = 0; i < words.size(); ++i) {
    EXPECT_TRUE(filter->Exists(Fingerprint(words[i])));
  }
};
}  // namespace mozc
