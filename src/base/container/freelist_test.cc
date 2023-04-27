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

#include "base/container/freelist.h"

#include <string>
#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace {

class Stub {
 public:
  Stub() { ++constructed_; }
  ~Stub() { ++destructed_; }

  constexpr bool IsUsed() { return used_; }
  constexpr void Use() { used_ = true; }

  static int constructed() { return constructed_; }
  static int destructed() { return destructed_; }
  static void Reset() { constructed_ = destructed_ = 0; }

 private:
  bool used_ = false;
  static int constructed_;
  static int destructed_;
};

int Stub::constructed_;
int Stub::destructed_;

class FreeListTest : public ::testing::Test {
 protected:
  FreeListTest() {
    Stub::Reset();
  }
  ~FreeListTest() override {
    EXPECT_EQ(Stub::constructed(), Stub::destructed());
  }
};

TEST_F(FreeListTest, FreeListSize) {
  FreeList<std::string> free_list(10);

  EXPECT_EQ(free_list.size(), 10);
}

TEST_F(FreeListTest, FreeListEmpty) {
  FreeList<int> free_list(1);
  // does not crash.
  free_list.Free();
  free_list.Reset();
  EXPECT_EQ(Stub::constructed(), 0);
}

TEST_F(FreeListTest, FreeListAllocResetFree) {
  FreeList<Stub> list(7);
  for (int i = 0; i < 10; ++i) {
    Stub *p = list.Alloc();
    EXPECT_FALSE(p->IsUsed());
    p->Use();
  }
  // Check if the allocations were done in increments of the size.
  EXPECT_EQ(Stub::constructed(), 14);
  EXPECT_EQ(Stub::destructed(), 0);

  list.Reset();
  EXPECT_EQ(Stub::constructed(), 14);
  EXPECT_EQ(Stub::destructed(), 0);

  for (int i = 0; i < 10; ++i) {
    Stub *p = list.Alloc();
    EXPECT_TRUE(p->IsUsed());
  }
  // Allocate another 10.
  for (int i = 0; i < 10; ++i) {
    Stub *p = list.Alloc();
    EXPECT_FALSE(p->IsUsed());
  }
  EXPECT_EQ(Stub::constructed(), 21);
  EXPECT_EQ(Stub::destructed(), 0);

  list.Free();
  EXPECT_EQ(Stub::constructed(), 21);
  // Free() may keep some objects after Free().
  EXPECT_GT(Stub::destructed(), 0);
}

TEST_F(FreeListTest, ObjectPoolSize) {
  ObjectPool<std::string> pool(10);

  EXPECT_EQ(pool.size(), 10);
}

TEST_F(FreeListTest, ObjectPoolEmpty) {
  ObjectPool<int> pool(1);
  // does not crash.
  pool.Free();
  EXPECT_EQ(Stub::constructed(), 0);
}

TEST_F(FreeListTest, ObjectPoolAllocReleaseFree) {
  ObjectPool<Stub> pool(3);
  std::vector<Stub *> in_use;

  for (int i = 0; i < 10; ++i) {
    Stub *p = pool.Alloc();
    EXPECT_FALSE(p->IsUsed());
    p->Use();
    in_use.push_back(p);
  }
  // Check if the allocations were done in increments of the size.
  EXPECT_EQ(Stub::constructed(), 12);
  EXPECT_EQ(Stub::destructed(), 0);

  // Callers may release an object. The object is not cleaned up and may be
  // returned by future calls to Alloc() in this case.
  for (Stub *p : in_use) {
    pool.Release(p);
  }
  EXPECT_EQ(Stub::constructed(), 12);
  EXPECT_EQ(Stub::destructed(), 0);
  in_use.clear();

  for (int i = 0; i < 10; ++i) {
    Stub *p = pool.Alloc();
    // The returned order doesn't matter.
    EXPECT_TRUE(p->IsUsed());
  }
  EXPECT_EQ(Stub::constructed(), 12);
  EXPECT_EQ(Stub::destructed(), 0);

  // Allocate another 10. These should not be reused objects.
  for (int i = 0; i < 10; ++i) {
    Stub *p = pool.Alloc();
    EXPECT_FALSE(p->IsUsed());
  }
  EXPECT_EQ(Stub::constructed(), 21);
  EXPECT_EQ(Stub::destructed(), 0);

  pool.Free();
  EXPECT_EQ(Stub::constructed(), 21);
  // ObjectPool may keep some objects after Free.
  EXPECT_GT(Stub::destructed(), 0);
}

}  // namespace
}  // namespace mozc
