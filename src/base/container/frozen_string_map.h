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

#ifndef MOZC_BASE_CONTAINER_FROZEN_STRING_MAP_H_
#define MOZC_BASE_CONTAINER_FROZEN_STRING_MAP_H_

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {
namespace frozen_string_map_internal {

// Non-constexpr functions to be called on build errors. Calling them fails the
// constant evaluation (i.e. the build) with the function name in the
// diagnostics.
inline void EmptyKeyIsNotSupported() {}
inline void TooLongKeyIsNotSupported() {}
inline void DuplicateKeyFound() {}
inline void InvalidNumberOfEntries() {}
inline void PerfectHashSearchFailed() {}

// Converts a (possibly signed) char to its byte value.
constexpr uint64_t Byte(char c) { return static_cast<uint8_t>(c); }

// Reads bytes as a little-endian integer. The byte-wise expression is usable in
// constant expressions regardless of the host endianness, and is folded into a
// single load instruction by the optimizer at runtime.
constexpr uint64_t Load64(const char* p) {
  return Byte(p[0]) | (Byte(p[1]) << 8) | (Byte(p[2]) << 16) |
         (Byte(p[3]) << 24) | (Byte(p[4]) << 32) | (Byte(p[5]) << 40) |
         (Byte(p[6]) << 48) | (Byte(p[7]) << 56);
}

constexpr uint64_t Load32(const char* p) {
  return Byte(p[0]) | (Byte(p[1]) << 8) | (Byte(p[2]) << 16) |
         (Byte(p[3]) << 24);
}

// Reads a little-endian length prefix of the given width. The loop is fully
// unrolled and folded into a single load instruction by the optimizer.
template <size_t kPrefixBytes>
constexpr size_t LoadLength(const char* p) {
  uint64_t length = 0;
  for (size_t i = 0; i < kPrefixBytes; ++i) {
    length |= Byte(p[i]) << (8 * i);
  }
  return static_cast<size_t>(length);
}

// One mixing round: folds the chunk 'v' into the state 'h'. Since 'multiplier'
// is odd, the round is invertible in 'v' for a fixed state, so keys hashed as
// a single chunk never collide with each other, for any multiplier.
constexpr uint64_t Mix(uint64_t h, uint64_t v, uint64_t multiplier) {
  h = (h ^ v) * multiplier;
  h ^= h >> 32;
  return h;
}

// Packs a key of 1 to 3 bytes and its length into a 26-bit value. The packing
// is injective over the whole class: byte positions 0 to n-1 hold every key
// byte, and bits 24-25 hold the length, so no two distinct short keys pack to
// the same value.
constexpr uint64_t PackShortKey(absl::string_view key) {
  const size_t n = key.size();
  return Byte(key[0]) | (Byte(key[n >> 1]) << 8) | (Byte(key[n - 1]) << 16) |
         (uint64_t{n} << 24);
}

// A cheap 64-bit string hash: the key is consumed in (possibly overlapping)
// 8-byte chunks, each folded into the state with one multiply-xorshift round.
// The mixing is intentionally weak; it only needs to be collision-free over the
// fixed key set of a FrozenStringMap, which the consteval builder verifies.
// Because 'multiplier' participates in the length seed and in every round,
// retrying with a different multiplier re-randomizes every cross-length and
// multi-chunk collision, and keys of at most 8 bytes of the same length are
// hashed injectively under every multiplier, so the builder's retry loop can
// always separate colliding key sets. Lookup correctness for arbitrary input is
// guaranteed by the final key comparison, not by the hash quality.
constexpr uint64_t HashKey(absl::string_view key, uint64_t multiplier) {
  const size_t n = key.size();
  const char* p = key.data();
  if (n < 4) {
    if (n == 0) {
      return 0;
    }
    // Keys of at most 3 bytes, including the length, fit in 26 bits, so one
    // multiply-xorshift round is enough: PackShortKey is injective over the
    // whole class (no two short keys can ever collide), and a difference
    // between two keys starts below bit 26, from where the multiplication
    // propagates it into both the bucket and the offset bits in a
    // multiplier-dependent way. Longer keys need the two rounds below because
    // their differences can be confined to the top bits, which a single round
    // cannot spread.
    uint64_t h = PackShortKey(key) * multiplier;
    h ^= h >> 32;
    return h;
  }
  // Seeding the state with the length keeps keys of different lengths apart.
  // The multiplication makes the separation multiplier-dependent; a plain
  // 'multiplier ^ n' seed would let the length cancel against the low bits of
  // the first key byte, e.g. "aaaa" vs "aaaaa", for every multiplier.
  uint64_t h = n * multiplier;
  if (n >= 8) {
    for (size_t offset = 0; offset + 8 < n; offset += 8) {
      h = Mix(h, Load64(p + offset), multiplier);
    }
    h = Mix(h, Load64(p + n - 8), multiplier);
  } else {
    h = Mix(h, Load32(p) | (Load32(p + n - 4) << 32), multiplier);
  }
  // The final round is not just avalanche polish: multiplication only
  // propagates differences upward, so without a second multiply a difference
  // confined to the top byte of the last chunk (e.g. same-length keys differing
  // only in the last byte) could never reach the bucket and offset bits, and no
  // multiplier retry could separate such keys.
  h *= multiplier;
  h ^= h >> 32;
  return h;
}

// Matches the entry list type CreateFrozenStringMap accepts: a std::array of
// key-value std::pairs, returned by value.
template <typename T>
inline constexpr bool IsEntryArray = false;
template <typename V, size_t N>
inline constexpr bool
    IsEntryArray<std::array<std::pair<absl::string_view, V>, N>> = true;

}  // namespace frozen_string_map_internal

// Compile-time generated read-only map that is backed by a perfect hash table
// (CHD: compress, hash, and displace). A lookup computes one cheap hash, reads
// one displacement, and compares the key with exactly one slot; there is no
// probing. This is usually faster than both mozc::FlatMap (binary search) and
// absl::flat_hash_map lookups, at the cost of a sparser table: the number of
// slots is 1.5 times the number of entries, rounded up to the next power of two
// (i.e. the load factor is between ~1/3 and ~3/4).
//
// All key bytes are copied at compile time into a length-prefixed pool owned by
// the map, and slots refer to the pool by 32-bit references. The map is thus
// fully self-contained: the source strings need not be kept alive, and for
// string literals the compiler does not even have to emit them. The width of
// the length prefix is chosen by CreateFrozenStringMap from the longest key.
//
// Keys of at most 3 bytes are stored inline in the slot references as the
// packed value the hash consumes, so a short-key lookup compares one integer
// and performs no pool access at all. Reference bits not required by the pool
// offset hold a hash fingerprint of the stored key, so most lookups of missing
// keys are also rejected without a pool access. When a value is as wide as a
// reference, the two are packed side by side into one slot, so a hit finds its
// value on the cache line that held the compared reference.
//
// Restrictions:
//  - The key type is fixed to absl::string_view, and keys must be non-empty.
//  - The total size of all keys must be less than 2 GiB.
//  - V must be default-constructible and copyable in constant expressions.
//  - Whether a perfect hash is found is verified at compile time; in the
//    unlikely case the multiplier search fails for a particular key set, the
//    build fails with 'PerfectHashSearchFailed' in the diagnostics.
//  - Building a map with thousands of entries may exceed the compiler's default
//    constant evaluation step limit; raise it if needed (e.g. /constexpr:steps
//    for MSVC, -fconstexpr-steps= for Clang).
//
// Consider calling CreateFrozenStringMap instead of the constructor, so you
// don't have to manually specify the layout parameters after V.
template <typename V, size_t kNumSlots, size_t kKeyPoolSize,
          size_t kPrefixBytes>
  requires std::semiregular<V>
class FrozenStringMap final {
  // Slot references: 32-bit, laid out as [tag][fingerprint][offset] or, for a
  // tagged inline short key, [tag][packed key]; see the constants below.
  using OffsetT = uint32_t;

  static_assert(std::has_single_bit(kNumSlots));
  static_assert(kNumSlots <= (size_t{1} << 16));
  static_assert(kPrefixBytes == 1 || kPrefixBytes == 2 || kPrefixBytes == 4);
  // The top bit of a reference is the inline-short-key tag; the rest must still
  // cover the pool offsets.
  static_assert(kKeyPoolSize - 1 <= std::numeric_limits<OffsetT>::max() >> 1);

 public:
  consteval explicit FrozenStringMap(
      absl::Span<const std::pair<absl::string_view, V>> entries) {
    if (entries.empty() || kNumSlots < entries.size()) {
      frozen_string_map_internal::InvalidNumberOfEntries();
    }
    // Scratch arrays are sized kNumSlots because entries.size() is not a
    // constant expression here; the slack only costs constant evaluation steps,
    // never runtime.
    std::array<absl::string_view, kNumSlots> keys{};
    for (size_t i = 0; i < entries.size(); ++i) {
      if (entries[i].first.empty()) {
        // Empty slots use the empty key as the sentinel.
        frozen_string_map_internal::EmptyKeyIsNotSupported();
      }
      if (entries[i].first.size() >= (uint64_t{1} << (8 * kPrefixBytes))) {
        // The key length must fit in the length prefix in key_pool_.
        frozen_string_map_internal::TooLongKeyIsNotSupported();
      }
      keys[i] = entries[i].first;
    }
    std::sort(keys.begin(), keys.begin() + entries.size());
    for (size_t i = 1; i < entries.size(); ++i) {
      if (keys[i - 1] == keys[i]) {
        frozen_string_map_internal::DuplicateKeyFound();
      }
    }
    // Pack the keys into the pool. Offset 0 is the reserved zero length prefix
    // that every empty slot points at. Short keys are referenced by their
    // tagged packed value instead of the pool offset; they are still packed
    // into the pool so that ForEach can hand out persistent string_views.
    std::array<OffsetT, kNumSlots> entry_refs{};
    size_t pos = kPrefixBytes;
    for (size_t i = 0; i < entries.size(); ++i) {
      const absl::string_view key = entries[i].first;
      if (key.size() < 4) {
        entry_refs[i] = static_cast<OffsetT>(
            frozen_string_map_internal::PackShortKey(key) | kInlineTag);
      } else {
        entry_refs[i] = static_cast<OffsetT>(pos);
      }
      for (size_t j = 0; j < kPrefixBytes; ++j) {
        key_pool_[pos++] = static_cast<char>((key.size() >> (8 * j)) & 0xFF);
      }
      std::copy_n(key.begin(), key.size(), key_pool_.begin() + pos);
      pos += key.size();
    }
    for (int attempt = 0; attempt < 64; ++attempt) {
      // The seed 0x9E3779B97F4A7C15 is the fractional part of the golden ratio
      // scaled by 2^64, i.e. the Fibonacci hashing constant, and the stride
      // 0x2545F4914F6CDD1D is the xorshift64* multiplier; any well-mixed
      // constants would do. '| 1' keeps every candidate odd, i.e. invertible
      // mod 2^64, which the mixing rounds rely on.
      const uint64_t multiplier =
          (uint64_t{0x9E3779B97F4A7C15} +
           static_cast<uint64_t>(attempt) * uint64_t{0x2545F4914F6CDD1D}) |
          1;
      if (TryBuild(entries, entry_refs, multiplier)) {
        multiplier_ = multiplier;
        return;
      }
    }
    frozen_string_map_internal::PerfectHashSearchFailed();
  }

  // Finds the value associated with the given key, or nullptr if not found.
  constexpr const V* FindOrNull(absl::string_view key) const {
    if (key.empty()) {
      return nullptr;
    }
    const uint64_t h = frozen_string_map_internal::HashKey(key, multiplier_);
    const size_t index =
        ((h >> 32) + displacements_[h & (kNumSlots - 1)]) & (kNumSlots - 1);
    if (key.size() < 4) {
      // One integer comparison, no pool access: the tagged packed value
      // identifies a short key exactly.
      const OffsetT ref = static_cast<OffsetT>(
          frozen_string_map_internal::PackShortKey(key) | kInlineTag);
      return RefAt(index) == ref ? ValueAt(index) : nullptr;
    }
    // A mismatch in the bits above the offset - a different fingerprint, or the
    // tag of an inline short key, which cannot equal a key of 4 or more bytes -
    // resolves to the zero-length sentinel without touching the pool, and the
    // comparison below fails.
    const OffsetT ref = RefAt(index);
    const size_t offset = ((uint64_t{ref} ^ Fingerprint(h)) >> kOffsetBits) == 0
                              ? ref & kOffsetMask
                              : 0;
    return PoolKeyAt(offset) == key ? ValueAt(index) : nullptr;
  }

  // Calls 'f(key, value)' for every entry, in an unspecified order.
  template <typename F>
  constexpr void ForEach(F&& f) const {
    for (size_t i = 0; i < kNumSlots; ++i) {
      if (RefAt(i) != 0) {
        f(StoredKey(i), *ValueAt(i));
      }
    }
  }

 private:
  // Storage for the references and the values: one array of packed
  // (reference, value) slots when that is free of padding, so that a hit finds
  // its value on the cache line that held the reference it just compared, and
  // two parallel arrays otherwise, where interleaving would pad each slot.
  static constexpr bool kPackedSlots =
      sizeof(V) == sizeof(OffsetT) && alignof(V) <= alignof(OffsetT);
  struct Slot {
    OffsetT ref = 0;
    V value{};
  };
  struct PackedStorage {
    std::array<Slot, kNumSlots> slots{};
  };
  struct SplitStorage {
    std::array<OffsetT, kNumSlots> refs{};
    std::array<V, kNumSlots> values{};
  };
  static_assert(!kPackedSlots || sizeof(Slot) == sizeof(OffsetT) + sizeof(V));

  constexpr OffsetT RefAt(size_t index) const {
    if constexpr (kPackedSlots) {
      return storage_.slots[index].ref;
    } else {
      return storage_.refs[index];
    }
  }

  constexpr const V* ValueAt(size_t index) const {
    if constexpr (kPackedSlots) {
      return &storage_.slots[index].value;
    } else {
      return &storage_.values[index];
    }
  }

  consteval void SetSlot(size_t index, OffsetT ref, const V& value) {
    if constexpr (kPackedSlots) {
      storage_.slots[index] = Slot{ref, value};
    } else {
      storage_.refs[index] = ref;
      storage_.values[index] = value;
    }
  }
  // The tag marking a reference as an inline short key.
  static constexpr OffsetT kInlineTag = OffsetT{1} << (8 * sizeof(OffsetT) - 1);

  // A pool-offset reference is laid out as [tag][fingerprint][offset]: the bits
  // between the offset width required by the pool size and the tag are filled
  // with extra hash bits of the key. A lookup whose fingerprint does not match
  // the slot's resolves to the zero-length sentinel without touching the pool,
  // so most missing keys are rejected one cache line earlier. The fingerprint
  // is free: it lives in reference bits that would otherwise always be zero.
  static constexpr size_t kOffsetBits = std::bit_width(kKeyPoolSize - 1);
  static constexpr size_t kFingerprintBits =
      8 * sizeof(OffsetT) - kOffsetBits - 1;
  static constexpr OffsetT kOffsetMask =
      static_cast<OffsetT>((uint64_t{1} << kOffsetBits) - 1);

  static constexpr OffsetT Fingerprint(uint64_t h) {
    if constexpr (kFingerprintBits == 0) {
      return 0;
    } else {
      // Bits 48 and up of the hash are not consumed by the bucket and the
      // offset parts of the slot index, whose widths are at most 16 bits
      // each.
      return static_cast<OffsetT>(
          ((h >> 48) & ((uint64_t{1} << kFingerprintBits) - 1)) << kOffsetBits);
    }
  }

  // Returns the key stored in the pool at the given offset. For offset 0, the
  // reserved sentinel, the result is the empty string_view, which no valid key
  // can equal.
  constexpr absl::string_view PoolKeyAt(size_t offset) const {
    return absl::string_view(
        key_pool_.data() + offset + kPrefixBytes,
        frozen_string_map_internal::LoadLength<kPrefixBytes>(key_pool_.data() +
                                                             offset));
  }

  // Returns the key stored in the given slot, resolving tagged references by
  // walking the pool for the matching short key (the packed value is injective,
  // so the match is unique). The walk makes this linear in the pool size, which
  // is fine for the enumeration and diagnostics use cases this backs;
  // FindOrNull never calls it.
  constexpr absl::string_view StoredKey(size_t index) const {
    const OffsetT ref = RefAt(index);
    if ((ref & kInlineTag) != 0) {
      for (size_t pos = kPrefixBytes; pos < kKeyPoolSize;) {
        const absl::string_view key = PoolKeyAt(pos);
        if (key.size() < 4 && (frozen_string_map_internal::PackShortKey(key) |
                               kInlineTag) == ref) {
          return key;
        }
        pos += kPrefixBytes + key.size();
      }
      // Unreachable: every tagged reference has its key in the pool.
    }
    return PoolKeyAt(ref & kOffsetMask);
  }

  consteval bool TryBuild(
      absl::Span<const std::pair<absl::string_view, V>> entries,
      const std::array<OffsetT, kNumSlots>& entry_refs, uint64_t multiplier) {
    displacements_ = {};
    storage_ = {};

    std::array<uint64_t, kNumSlots> hashes{};
    std::array<uint32_t, kNumSlots> bucket_sizes{};
    for (size_t i = 0; i < entries.size(); ++i) {
      hashes[i] =
          frozen_string_map_internal::HashKey(entries[i].first, multiplier);
      ++bucket_sizes[hashes[i] & (kNumSlots - 1)];
    }

    // Group the entry indices by bucket (counting sort), so that each bucket's
    // items are a contiguous range of 'items_by_bucket'.
    std::array<uint32_t, kNumSlots + 1> bucket_begin{};
    for (size_t b = 0; b < kNumSlots; ++b) {
      bucket_begin[b + 1] = bucket_begin[b] + bucket_sizes[b];
    }
    std::array<uint32_t, kNumSlots> items_by_bucket{};
    {
      std::array<uint32_t, kNumSlots> cursors{};
      for (size_t i = 0; i < entries.size(); ++i) {
        const size_t b = hashes[i] & (kNumSlots - 1);
        items_by_bucket[bucket_begin[b] + cursors[b]++] =
            static_cast<uint32_t>(i);
      }
    }

    // Process the non-empty buckets in decreasing size order: large buckets are
    // the hardest to place, so give them the emptiest table.
    std::array<uint32_t, kNumSlots> bucket_order{};
    size_t num_buckets = 0;
    for (size_t b = 0; b < kNumSlots; ++b) {
      if (bucket_sizes[b] > 0) {
        bucket_order[num_buckets++] = static_cast<uint32_t>(b);
      }
    }
    // Ties are broken by the bucket index so that the comparator is a total
    // order, to work around the fact that std::sort is unstable.
    std::sort(bucket_order.begin(), bucket_order.begin() + num_buckets,
              [&](uint32_t x, uint32_t y) {
                return bucket_sizes[x] != bucket_sizes[y]
                           ? bucket_sizes[x] > bucket_sizes[y]
                           : x < y;
              });

    std::array<bool, kNumSlots> occupied{};
    for (size_t k = 0; k < num_buckets; ++k) {
      const uint32_t bucket = bucket_order[k];
      const uint32_t begin = bucket_begin[bucket];
      const uint32_t end = bucket_begin[bucket + 1];
      size_t d = 0;
      for (; d < kNumSlots; ++d) {
        bool ok = true;
        for (uint32_t i = begin; i < end; ++i) {
          if (occupied[((hashes[items_by_bucket[i]] >> 32) + d) &
                       (kNumSlots - 1)]) {
            ok = false;
            break;
          }
        }
        if (ok) break;
      }
      if (d == kNumSlots) {
        return false;
      }
      displacements_[bucket] = static_cast<uint16_t>(d);
      for (uint32_t i = begin; i < end; ++i) {
        const uint32_t item = items_by_bucket[i];
        const size_t pos = ((hashes[item] >> 32) + d) & (kNumSlots - 1);
        // If two keys in this bucket collide on the same slot, no displacement
        // can separate them; retry with another multiplier.
        if (occupied[pos]) {
          return false;
        }
        occupied[pos] = true;
        OffsetT ref = entry_refs[item];
        if (entries[item].first.size() >= 4) {
          ref |= Fingerprint(hashes[item]);
        }
        SetSlot(pos, ref, entries[item].second);
      }
    }
    return true;
  }

  uint64_t multiplier_ = 0;
  std::array<uint16_t, kNumSlots> displacements_{};
  // References to the keys: pool offsets, or, when the top bit is set, inline
  // packed short keys. Empty slots hold reference 0, which resolves to the
  // reserved zero length prefix of the pool.
  std::conditional_t<kPackedSlots, PackedStorage, SplitStorage> storage_{};
  // All key bytes, packed as a kPrefixBytes-wide little-endian length prefix
  // followed by the key bytes; the pool starts with the reserved zero length
  // prefix that empty slots point at.
  std::array<char, kKeyPoolSize> key_pool_{};
};

// Creates a FrozenStringMap from a captureless lambda that returns a std::array
// of key-value std::pairs. The entries are passed as a template argument rather
// than a function argument because the sizes of the internal arrays
// (e.g. the key pool) depend on the entry values, and consteval function
// parameters are not constant expressions.
//
// Example:
//
//   constexpr auto kMap = CreateFrozenStringMap<[] {
//     return std::to_array<std::pair<absl::string_view, int>>({
//         {"one", 1},
//         {"two", 2},
//         {"three", 3},
//     });
//   }>();
//
// Declare the variable as auto and use CreateFrozenStringMap. The actual type
// is complex and explicitly declaring it would leak the number of slots.
template <auto kGetEntries>
  requires(frozen_string_map_internal::IsEntryArray<
           std::remove_cvref_t<decltype(kGetEntries())>>)
consteval auto CreateFrozenStringMap() {
  constexpr auto entries = kGetEntries();
  using V = typename decltype(entries)::value_type::second_type;
  // 1.5x slots keep the load factor at or below ~3/4, which the displacement
  // search handles reliably, while capping the table overhead at ~3x.
  constexpr size_t kNumSlots =
      std::bit_ceil(entries.size() + entries.size() / 2);
  // The narrowest length prefix that fits the longest key, so that small maps
  // do not pay for the support of long keys.
  constexpr size_t kPrefixBytes = [&] {
    size_t max_length = 0;
    for (const auto& [key, value] : entries) {
      max_length = std::max(max_length, key.size());
    }
    return max_length <= 0xFF ? 1 : max_length <= 0xFFFF ? 2 : 4;
  }();
  constexpr size_t kKeyPoolSize = [&] {
    size_t size = kPrefixBytes;  // The reserved zero length prefix.
    for (const auto& [key, value] : entries) {
      size += kPrefixBytes + key.size();
    }
    return size;
  }();
  return FrozenStringMap<V, kNumSlots, kKeyPoolSize, kPrefixBytes>(entries);
}

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_FROZEN_STRING_MAP_H_
