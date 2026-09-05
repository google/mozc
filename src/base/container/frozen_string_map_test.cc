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

#include "base/container/frozen_string_map.h"

#include <array>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

constexpr auto kMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, int>>({
      {"one", 1},
      {"two", 2},
      {"three", 3},
      {"four", 4},
      {"five", 5},
  });
}>();

// The map is usable in constant expressions.
static_assert(*kMap.FindOrNull("one") == 1);
static_assert(*kMap.FindOrNull("five") == 5);
static_assert(kMap.FindOrNull("six") == nullptr);
static_assert(kMap.FindOrNull("") == nullptr);

TEST(FrozenStringMapTest, FindOrNull) {
  // Use runtime strings so that lookups cannot be constant-folded.
  EXPECT_EQ(*kMap.FindOrNull(std::string("one")), 1);
  EXPECT_EQ(*kMap.FindOrNull(std::string("two")), 2);
  EXPECT_EQ(*kMap.FindOrNull(std::string("three")), 3);
  EXPECT_EQ(*kMap.FindOrNull(std::string("four")), 4);
  EXPECT_EQ(*kMap.FindOrNull(std::string("five")), 5);

  EXPECT_EQ(kMap.FindOrNull(std::string("")), nullptr);
  EXPECT_EQ(kMap.FindOrNull(std::string("on")), nullptr);
  EXPECT_EQ(kMap.FindOrNull(std::string("onee")), nullptr);
  EXPECT_EQ(kMap.FindOrNull(std::string("six")), nullptr);
}

// Keys that are prone to hash collisions: short numeric keys whose byte
// patterns overlap ("2" vs "12"), long keys differing only around the 8th
// byte (動詞カ行五段 vs 動詞ガ行五段), and long keys differing only in the
// middle. The builder must find a collision-free multiplier for all of them.
constexpr auto kHardMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, int>>({
      {"1", 1},
      {"2", 2},
      {"12", 12},
      {"21", 21},
      {"122", 122},
      {"221", 221},
      {"1221", 1221},
      {"2112", 2112},
      {"動詞カ行五段", 1000},
      {"動詞ガ行五段", 1001},
      {"名詞サ変則あり", 1002},
      {"名詞サ変則なし", 1003},
      {"ながいながいキーのまんなかがちがう:あ:ながいながいキー", 2000},
      {"ながいながいキーのまんなかがちがう:い:ながいながいキー", 2001},
  });
}>();

TEST(FrozenStringMapTest, HardKeys) {
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("1")), 1);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("2")), 2);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("12")), 12);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("21")), 21);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("122")), 122);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("221")), 221);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("1221")), 1221);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("2112")), 2112);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("動詞カ行五段")), 1000);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("動詞ガ行五段")), 1001);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("名詞サ変則あり")), 1002);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string("名詞サ変則なし")), 1003);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string(
                "ながいながいキーのまんなかがちがう:あ:ながいながいキー")),
            2000);
  EXPECT_EQ(*kHardMap.FindOrNull(std::string(
                "ながいながいキーのまんなかがちがう:い:ながいながいキー")),
            2001);

  EXPECT_EQ(kHardMap.FindOrNull(std::string("112")), nullptr);
  EXPECT_EQ(kHardMap.FindOrNull(std::string("動詞サ行五段")), nullptr);
  EXPECT_EQ(kHardMap.FindOrNull(std::string(
                "ながいながいキーのまんなかがちがう:う:ながいながいキー")),
            nullptr);
}

// Key sets that used to defeat the multiplier retry loop of an earlier hash
// design: 8-byte keys (whose first and last 8-byte loads coincide), 5-7-byte
// keys differing only in the low bits of the last byte, and keys that are a
// prefix of another key ("aaaa" vs "aaaaa"). The builder must find a
// collision-free multiplier for all of them.
constexpr auto kFormerCollisionMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, int>>({
      {"notebook", 1},
      {"keyboard", 2},
      {"password", 3},
      {"username", 4},
      {"settings", 5},
      {"commands", 6},
      {"distance", 7},
      {"abcda", 10},
      {"abcde", 11},
      {"abcdefa", 12},
      {"abcdefc", 13},
      {"aaaa", 14},
      {"aaaaa", 15},
  });
}>();

TEST(FrozenStringMapTest, FormerCollisionKeys) {
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("notebook")), 1);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("keyboard")), 2);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("password")), 3);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("username")), 4);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("settings")), 5);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("commands")), 6);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("distance")), 7);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("abcda")), 10);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("abcde")), 11);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("abcdefa")), 12);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("abcdefc")), 13);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("aaaa")), 14);
  EXPECT_EQ(*kFormerCollisionMap.FindOrNull(std::string("aaaaa")), 15);

  EXPECT_EQ(kFormerCollisionMap.FindOrNull(std::string("michigan")), nullptr);
  EXPECT_EQ(kFormerCollisionMap.FindOrNull(std::string("abcdb")), nullptr);
  EXPECT_EQ(kFormerCollisionMap.FindOrNull(std::string("aaaaaa")), nullptr);
}

constexpr auto kStringMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, absl::string_view>>({
      {"まん", "万"},
      {"おく", "億"},
      {"ちょう", "兆"},
  });
}>();

TEST(FrozenStringMapTest, StringViewValues) {
  EXPECT_EQ(*kStringMap.FindOrNull(std::string("まん")), "万");
  EXPECT_EQ(*kStringMap.FindOrNull(std::string("おく")), "億");
  EXPECT_EQ(*kStringMap.FindOrNull(std::string("ちょう")), "兆");
  EXPECT_EQ(kStringMap.FindOrNull(std::string("けい")), nullptr);

  // ForEach over a map whose values are too wide to be packed beside the
  // slot references.
  size_t count = 0;
  kStringMap.ForEach([&](absl::string_view key, absl::string_view value) {
    EXPECT_EQ(*kStringMap.FindOrNull(key), value);
    ++count;
  });
  EXPECT_EQ(count, 3);
}

// A value type narrower than a slot reference selects the split storage,
// where packing would pad each slot.
enum class TinyValue : unsigned char { kZero, kOne, kTwo };

constexpr auto kTinyValueMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, TinyValue>>({
      {"zero", TinyValue::kZero},
      {"one", TinyValue::kOne},
      {"two", TinyValue::kTwo},
  });
}>();

TEST(FrozenStringMapTest, TinyValues) {
  EXPECT_EQ(*kTinyValueMap.FindOrNull(std::string("zero")), TinyValue::kZero);
  EXPECT_EQ(*kTinyValueMap.FindOrNull(std::string("one")), TinyValue::kOne);
  EXPECT_EQ(*kTinyValueMap.FindOrNull(std::string("two")), TinyValue::kTwo);
  EXPECT_EQ(kTinyValueMap.FindOrNull(std::string("three")), nullptr);
}

// Keys are arbitrary byte sequences: NUL bytes and non-ASCII bytes must
// round-trip through the inline short-key packing and the key pool, and a
// key of NUL bytes must not be confused with the empty-slot sentinel.
constexpr auto kBinaryKeyMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, int>>({
      {absl::string_view("\0", 1), 1},
      {absl::string_view("\0\0", 2), 2},
      {"\xff", 3},
      {"\xff\xfe\xfd\xfc\xfb", 5},
      {absl::string_view("a\0b", 3), 6},
  });
}>();

static_assert(*kBinaryKeyMap.FindOrNull(absl::string_view("\0", 1)) == 1);
static_assert(kBinaryKeyMap.FindOrNull("") == nullptr);

TEST(FrozenStringMapTest, BinaryKeys) {
  EXPECT_EQ(*kBinaryKeyMap.FindOrNull(std::string("\0", 1)), 1);
  EXPECT_EQ(*kBinaryKeyMap.FindOrNull(std::string("\0\0", 2)), 2);
  EXPECT_EQ(*kBinaryKeyMap.FindOrNull(std::string("\xff")), 3);
  EXPECT_EQ(*kBinaryKeyMap.FindOrNull(std::string("\xff\xfe\xfd\xfc\xfb")), 5);
  EXPECT_EQ(*kBinaryKeyMap.FindOrNull(std::string("a\0b", 3)), 6);

  EXPECT_EQ(kBinaryKeyMap.FindOrNull(std::string("\0\0\0", 3)), nullptr);
  EXPECT_EQ(kBinaryKeyMap.FindOrNull(std::string("\xfe")), nullptr);
  EXPECT_EQ(kBinaryKeyMap.FindOrNull(std::string("a")), nullptr);

  size_t count = 0;
  kBinaryKeyMap.ForEach([&](absl::string_view key, int value) {
    EXPECT_EQ(*kBinaryKeyMap.FindOrNull(key), value);
    ++count;
  });
  EXPECT_EQ(count, 5);
}

// Keys around the 8-byte chunking boundaries of the hash: exactly 8, 15, 16,
// and 17 bytes, and pairs differing only in the bytes covered by the
// overlapping tail load or the middle chunk.
constexpr auto kChunkBoundaryMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, int>>({
      {"01234567", 8},
      {"0123456789abcde", 15},
      {"0123456789abcdef", 16},
      {"0123456789abcdefg", 17},
      {"0123456789abcdeX", 116},  // The 16-byte key with another last byte.
      {"0123456X89abcdef", 216},  // The 16-byte key with another 8th byte.
  });
}>();

TEST(FrozenStringMapTest, ChunkBoundaryKeys) {
  EXPECT_EQ(*kChunkBoundaryMap.FindOrNull(std::string("01234567")), 8);
  EXPECT_EQ(*kChunkBoundaryMap.FindOrNull(std::string("0123456789abcde")), 15);
  EXPECT_EQ(*kChunkBoundaryMap.FindOrNull(std::string("0123456789abcdef")), 16);
  EXPECT_EQ(*kChunkBoundaryMap.FindOrNull(std::string("0123456789abcdefg")),
            17);
  EXPECT_EQ(*kChunkBoundaryMap.FindOrNull(std::string("0123456789abcdeX")),
            116);
  EXPECT_EQ(*kChunkBoundaryMap.FindOrNull(std::string("0123456X89abcdef")),
            216);

  EXPECT_EQ(kChunkBoundaryMap.FindOrNull(std::string("0123456789abcdeY")),
            nullptr);
  EXPECT_EQ(kChunkBoundaryMap.FindOrNull(std::string("0123456789abcdefgh")),
            nullptr);
}

// A map whose keys are all at most 3 bytes: every slot reference is either
// tagged or empty, and lookups of longer keys must be rejected through the
// tag check.
constexpr auto kAllShortMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, int>>({
      {"a", 1},
      {"b", 2},
      {"ab", 3},
      {"abc", 4},
  });
}>();

TEST(FrozenStringMapTest, AllShortKeys) {
  EXPECT_EQ(*kAllShortMap.FindOrNull(std::string("a")), 1);
  EXPECT_EQ(*kAllShortMap.FindOrNull(std::string("b")), 2);
  EXPECT_EQ(*kAllShortMap.FindOrNull(std::string("ab")), 3);
  EXPECT_EQ(*kAllShortMap.FindOrNull(std::string("abc")), 4);

  EXPECT_EQ(kAllShortMap.FindOrNull(std::string("c")), nullptr);
  EXPECT_EQ(kAllShortMap.FindOrNull(std::string("ba")), nullptr);
  EXPECT_EQ(kAllShortMap.FindOrNull(std::string("abcd")), nullptr);
  EXPECT_EQ(kAllShortMap.FindOrNull(std::string("abcdefghijklmnop")), nullptr);
}

TEST(FrozenStringMapTest, ForEach) {
  int sum = 0;
  size_t count = 0;
  kMap.ForEach([&](absl::string_view key, int value) {
    EXPECT_EQ(*kMap.FindOrNull(key), value);
    sum += value;
    ++count;
  });
  EXPECT_EQ(count, 5);
  EXPECT_EQ(sum, 1 + 2 + 3 + 4 + 5);
}

// ForEach is usable in constant expressions.
static_assert([] {
  size_t count = 0;
  kMap.ForEach([&](absl::string_view, int) { ++count; });
  return count == 5;
}());

TEST(FrozenStringMapTest, ForEachWithShortKeys) {
  // kHardMap contains keys of 1 to 3 bytes, which are stored inline in the
  // slot references; ForEach must still hand out the full key bytes.
  int sum = 0;
  size_t count = 0;
  kHardMap.ForEach([&](absl::string_view key, int value) {
    EXPECT_EQ(*kHardMap.FindOrNull(key), value) << key;
    sum += value;
    ++count;
  });
  EXPECT_EQ(count, 14);
  EXPECT_EQ(sum, 1 + 2 + 12 + 21 + 122 + 221 + 1221 + 2112 + 1000 + 1001 +
                     1002 + 1003 + 2000 + 2001);
}

// A key longer than 255 bytes makes CreateFrozenStringMap select a two-byte
// length prefix at build time.
constexpr absl::string_view kLongKey =
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "01234567890123456789012345678901234567890123456789";  // 260 bytes.
static_assert(kLongKey.size() == 260);

constexpr auto kLongKeyMap = CreateFrozenStringMap<[] {
  return std::to_array<std::pair<absl::string_view, int>>({
      {"short", 1},
      {kLongKey, 2},
  });
}>();

TEST(FrozenStringMapTest, LongKeys) {
  EXPECT_EQ(*kLongKeyMap.FindOrNull(std::string("short")), 1);
  EXPECT_EQ(*kLongKeyMap.FindOrNull(std::string(kLongKey)), 2);
  EXPECT_EQ(kLongKeyMap.FindOrNull(std::string(kLongKey).substr(1)), nullptr);
  EXPECT_EQ(kLongKeyMap.FindOrNull(std::string(kLongKey) + "0"), nullptr);
}

TEST(FrozenStringMapTest, DirectConstruction) {
  // Calling the constructor with explicit layout parameters, as
  // CreateFrozenStringMap does after computing them from the entries.
  constexpr FrozenStringMap<int, 4, 8, 1> kDirect(
      std::to_array<std::pair<absl::string_view, int>>({
          {"a", 1},
          {"bb", 2},
      }));
  EXPECT_EQ(*kDirect.FindOrNull(std::string("a")), 1);
  EXPECT_EQ(*kDirect.FindOrNull(std::string("bb")), 2);
  EXPECT_EQ(kDirect.FindOrNull(std::string("b")), nullptr);
}

TEST(FrozenStringMapTest, WidePrefixes) {
  // Two- and four-byte length prefixes, as CreateFrozenStringMap selects for
  // maps whose longest key exceeds 255 or 65535 bytes, work regardless of
  // the actual key lengths.
  constexpr auto kEntries = std::to_array<std::pair<absl::string_view, int>>({
      {"a", 1},
      {"bb", 2},
      {"cccc", 3},
  });
  constexpr FrozenStringMap<int, 4, 15, 2> kPrefix2(kEntries);
  EXPECT_EQ(*kPrefix2.FindOrNull(std::string("a")), 1);
  EXPECT_EQ(*kPrefix2.FindOrNull(std::string("bb")), 2);
  EXPECT_EQ(*kPrefix2.FindOrNull(std::string("cccc")), 3);
  EXPECT_EQ(kPrefix2.FindOrNull(std::string("dddd")), nullptr);

  constexpr FrozenStringMap<int, 4, 23, 4> kPrefix4(kEntries);
  EXPECT_EQ(*kPrefix4.FindOrNull(std::string("a")), 1);
  EXPECT_EQ(*kPrefix4.FindOrNull(std::string("bb")), 2);
  EXPECT_EQ(*kPrefix4.FindOrNull(std::string("cccc")), 3);
  EXPECT_EQ(kPrefix4.FindOrNull(std::string("dddd")), nullptr);
}

TEST(FrozenStringMapTest, SingleEntry) {
  constexpr auto kSingle = CreateFrozenStringMap<[] {
    return std::to_array<std::pair<absl::string_view, int>>({{"only", 42}});
  }>();
  EXPECT_EQ(*kSingle.FindOrNull(std::string("only")), 42);
  EXPECT_EQ(kSingle.FindOrNull(std::string("only!")), nullptr);
  EXPECT_EQ(kSingle.FindOrNull(std::string("")), nullptr);
}

}  // namespace
}  // namespace mozc
