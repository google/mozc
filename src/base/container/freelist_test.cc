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

#include <utility>
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
  FreeListTest() { Stub::Reset(); }
  ~FreeListTest() override {
    EXPECT_EQ(Stub::constructed(), Stub::destructed());
  }
};

TEST_F(FreeListTest, FreeListEmpty) {
  FreeList<int> free_list(1);
  EXPECT_TRUE(free_list.empty());
  EXPECT_EQ(free_list.size(), 0);
  EXPECT_EQ(free_list.capacity(), 0);
  EXPECT_EQ(free_list.chunk_size(), 1);
  // does not crash.
  free_list.Free();
  EXPECT_TRUE(free_list.empty());
  EXPECT_EQ(free_list.capacity(), 0);
  EXPECT_EQ(Stub::constructed(), 0);
}

TEST_F(FreeListTest, FreeListAllocResetFree) {
  FreeList<Stub> list(7);
  EXPECT_EQ(list.chunk_size(), 7);
  for (int i = 0; i < 10; ++i) {
    Stub *p = list.Alloc();
    EXPECT_FALSE(p->IsUsed());
    p->Use();
  }
  // Check if the allocations were done in increments of the size.
  EXPECT_EQ(list.capacity(), 14);
  EXPECT_EQ(list.size(), 10);
  EXPECT_FALSE(list.empty());
  EXPECT_EQ(Stub::constructed(), 10);
  EXPECT_EQ(Stub::destructed(), 0);

  FreeList<Stub> other(std::move(list));
  EXPECT_EQ(other.size(), 10);
  EXPECT_EQ(other.capacity(), 14);

  // Allocate 10 more objects.
  for (int i = 0; i < 10; ++i) {
    Stub *p = other.Alloc();
    EXPECT_FALSE(p->IsUsed());
  }
  EXPECT_EQ(other.capacity(), 21);
  EXPECT_EQ(other.size(), 20);
  EXPECT_EQ(Stub::constructed(), 20);
  EXPECT_EQ(Stub::destructed(), 0);
  list = std::move(other);
  EXPECT_EQ(list.size(), 20);
  EXPECT_EQ(list.capacity(), 21);
  list.Free();
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(Stub::constructed(), 20);
  // Free() may keep some objects after Free().
  EXPECT_GT(Stub::destructed(), 0);
}

TEST_F(FreeListTest, FreeFirst) {
  FreeList<Stub> list(10);
  list.Free();
  list.Alloc();

  FreeList<Stub> list2(10);
  list2.Free();
  for (int i = 0; i < 11; ++i) {
    list2.Alloc();
  }
}

TEST_F(FreeListTest, ObjectPoolEmpty) {
  ObjectPool<int> pool(1);
  EXPECT_TRUE(pool.empty());
  EXPECT_EQ(pool.capacity(), 0);
  EXPECT_EQ(pool.size(), 0);
  EXPECT_EQ(pool.chunk_size(), 1);
  // does not crash.
  pool.Free();
  EXPECT_TRUE(pool.empty());
  EXPECT_EQ(pool.capacity(), 0);
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
  EXPECT_EQ(Stub::constructed(), 10);
  EXPECT_EQ(Stub::destructed(), 0);
  EXPECT_EQ(pool.capacity(), 12);
  EXPECT_EQ(pool.size(), 10);

  // Callers may release an object. The object is not cleaned up and may be
  // returned by future calls to Alloc() in this case.
  for (Stub *p : in_use) {
    pool.Release(p);
  }
  EXPECT_EQ(Stub::constructed(), 10);
  EXPECT_EQ(Stub::destructed(), 0);
  in_use.clear();
  EXPECT_TRUE(pool.empty());
  EXPECT_EQ(pool.capacity(), 12);

  for (int i = 0; i < 10; ++i) {
    Stub *p = pool.Alloc();
    // The returned order doesn't matter.
    EXPECT_TRUE(p->IsUsed());
  }
  EXPECT_EQ(pool.size(), 10);
  EXPECT_EQ(pool.capacity(), 12);
  EXPECT_EQ(Stub::constructed(), 10);
  EXPECT_EQ(Stub::destructed(), 0);

  // Allocate another 10. These should not be reused objects.
  for (int i = 0; i < 10; ++i) {
    Stub *p = pool.Alloc();
    EXPECT_FALSE(p->IsUsed());
  }
  EXPECT_EQ(pool.size(), 20);
  EXPECT_EQ(pool.capacity(), 21);
  EXPECT_EQ(Stub::constructed(), 20);
  EXPECT_EQ(Stub::destructed(), 0);

  ObjectPool<Stub> other(1);
  using std::swap;
  swap(pool, other);
  EXPECT_EQ(pool.chunk_size(), 1);
  EXPECT_TRUE(pool.empty());
  EXPECT_FALSE(other.empty());
  EXPECT_EQ(other.chunk_size(), 3);
  pool = std::move(other);

  pool.Free();
  EXPECT_EQ(Stub::constructed(), 20);
  // ObjectPool may keep some objects after Free.
  EXPECT_GT(Stub::destructed(), 0);
  EXPECT_TRUE(pool.empty());

  for (int i = 0; i < pool.chunk_size(); ++i) {
    pool.Alloc();
  }
}

}  // namespace
}  // namespace mozc
