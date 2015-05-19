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

#include "converter/cached_connector.h"

#include <string>
#include <vector>
#include "base/base.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "converter/connector_interface.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace converter {
namespace {
class TestConnector : public ConnectorInterface {
 public:
  TestConnector(int offset) : offset_(offset) {}
  ~TestConnector() {}

  int GetTransitionCost(uint16 rid, uint16 lid) const {
    uint32 key = (static_cast<uint32>(rid) << 16) | lid;
    return offset_ + key;
  }

  int GetResolution() const {
    return 0;
  }

  int offset_;
};

const int kCacheSize = 256;

class CachedConnectorThread : public Thread {
 public:
  CachedConnectorThread(int offset) : offset_(offset) {}

  void Run() {
    // Create the connector in the new thread
    test_.reset(new TestConnector(offset_));
    cached_.reset(new CachedConnector(test_.get(), kCacheSize));
    // Clear Cache just in case.
    cached_->ClearCache();

    // With TLS, the different cache is used when a new
    // thread is created.
    const int kTrialSize = 100;
    const int kIdSize = 100;
    for (int trial = 0; trial < kTrialSize; ++trial) {
      for (int i = 0; i < kIdSize; ++i) {
        for (int j = 0; j < kIdSize; ++j) {
          EXPECT_EQ(test_->GetTransitionCost(i, j),
                    cached_->GetTransitionCost(i, j)) << offset_;
        }
      }
    }

    // Clear Cache just in case.
    cached_->ClearCache();
  }

 private:
  int offset_;
  scoped_ptr<TestConnector> test_;
  scoped_ptr<CachedConnector> cached_;
};
}  // namespace

class CachedConnectorTest : public testing::Test {
 protected:
  CachedConnectorTest() : test_(0), cached_(&test_, kCacheSize) {}

  void SetUp() {
    // Clear the cache on this thread.  b/5119167.
    cached_.ClearCache();
  }

  void TearDown() {
    // Clear the cache on this thread just in case.
    cached_.ClearCache();
  }

  TestConnector test_;
  CachedConnector cached_;
};

TEST_F(CachedConnectorTest, CacheTest) {
  TestConnector test(0);
  CachedConnector cached(&test, kCacheSize);
  for (int trial = 0; trial < 100; ++trial) {
    for (int i = 0; i < 100; ++i) {
      for (int j = 0; j < 100; ++j) {
        EXPECT_EQ(test.GetTransitionCost(i, j),
                  cached.GetTransitionCost(i, j));
      }
    }
  }
}

TEST_F(CachedConnectorTest, CacheTestWithThread) {
  // Currently each connector has its own cache. So it must be thread safe.
  const int kSize = 10;
  vector<CachedConnectorThread *> threads;
  for (int i = 0; i < kSize; ++i) {
    threads.push_back(new CachedConnectorThread(i));
  }

  for (int i = 0; i < kSize; ++i) {
    threads[i]->Start();
  }

  for (int i = 0; i < kSize; ++i) {
    threads[i]->Join();
  }

  for (int i = 0; i < kSize; ++i) {
    delete threads[i];
  }
}

}  // namespace converter
}  // namespace mozc
