// Copyright 2010-2020, Google Inc.
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

#include "rewriter/single_kanji_rewriter.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "absl/strings/string_view.h"

using mozc::dictionary::POSMatcher;

namespace mozc {
namespace {

// A random access iterator over uint32 array that increments a pointer by N:
// iter     -> array[0]
// iter + 1 -> array[N]
// iter + 2 -> array[2 * N]
// ...
template <size_t N>
class Uint32ArrayIterator
    : public std::iterator<std::random_access_iterator_tag, uint32> {
 public:
  explicit Uint32ArrayIterator(const uint32 *ptr) : ptr_(ptr) {}

  uint32 operator*() const { return *ptr_; }

  uint32 operator[](size_t i) const {
    DCHECK_LT(i, N);
    return ptr_[i];
  }

  void swap(Uint32ArrayIterator &x) {
    using std::swap;
    swap(ptr_, x.ptr_);
  }

  friend void swap(Uint32ArrayIterator &x, Uint32ArrayIterator &y) {
    x.swap(y);
  }

  Uint32ArrayIterator &operator++() {
    ptr_ += N;
    return *this;
  }

  Uint32ArrayIterator operator++(int) {
    const uint32 *tmp = ptr_;
    ptr_ += N;
    return Uint32ArrayIterator(tmp);
  }

  Uint32ArrayIterator &operator--() {
    ptr_ -= N;
    return *this;
  }

  Uint32ArrayIterator operator--(int) {
    const uint32 *tmp = ptr_;
    ptr_ -= N;
    return Uint32ArrayIterator(tmp);
  }

  Uint32ArrayIterator &operator+=(ptrdiff_t n) {
    ptr_ += n * N;
    return *this;
  }

  Uint32ArrayIterator &operator-=(ptrdiff_t n) {
    ptr_ -= n * N;
    return *this;
  }

  friend Uint32ArrayIterator operator+(Uint32ArrayIterator x, ptrdiff_t n) {
    return Uint32ArrayIterator(x.ptr_ + n * N);
  }

  friend Uint32ArrayIterator operator+(ptrdiff_t n, Uint32ArrayIterator x) {
    return Uint32ArrayIterator(x.ptr_ + n * N);
  }

  friend Uint32ArrayIterator operator-(Uint32ArrayIterator x, ptrdiff_t n) {
    return Uint32ArrayIterator(x.ptr_ - n * N);
  }

  friend ptrdiff_t operator-(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return (x.ptr_ - y.ptr_) / N;
  }

  friend bool operator==(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ == y.ptr_;
  }

  friend bool operator!=(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ != y.ptr_;
  }

  friend bool operator<(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ < y.ptr_;
  }

  friend bool operator<=(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ <= y.ptr_;
  }

  friend bool operator>(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ > y.ptr_;
  }

  friend bool operator>=(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ >= y.ptr_;
  }

 private:
  const uint32 *ptr_;
};

// Looks up single kanji list from key (reading).  Returns false if not found.
// The underlying token array, |single_kanji_token_array|, has the following
// format:
//
// +------------------+
// | index of key 0   |
// +------------------+
// | index of value 0 |
// +------------------+
// | index of key 1   |
// +------------------+
// | index of value 1 |
// +------------------+
// | ...              |
//
// Here, each element is of uint32 type.  Each of actual string values are
// stored in |single_kanji_string_array| at its index.
bool LookupKanjiList(absl::string_view single_kanji_token_array,
                     const SerializedStringArray &single_kanji_string_array,
                     const std::string &key,
                     std::vector<std::string> *kanji_list) {
  DCHECK(kanji_list);
  const uint32 *token_array =
      reinterpret_cast<const uint32 *>(single_kanji_token_array.data());
  const size_t token_array_size =
      single_kanji_token_array.size() / sizeof(uint32);

  const Uint32ArrayIterator<2> end(token_array + token_array_size);
  const auto iter =
      std::lower_bound(Uint32ArrayIterator<2>(token_array), end, key,
                       [&single_kanji_string_array](
                           uint32 index, const std::string &target_key) {
                         return single_kanji_string_array[index] < target_key;
                       });
  if (iter == end || single_kanji_string_array[iter[0]] != key) {
    return false;
  }
  const absl::string_view values = single_kanji_string_array[iter[1]];
  Util::SplitStringToUtf8Chars(values, kanji_list);
  return true;
}

// Generates kanji variant description from key.  Does nothing if not found.
// The underlying token array, |variant_token_array|, has the following
// format:
//
// +-------------------------+
// | index of target 0       |
// +-------------------------+
// | index of original 0     |
// +-------------------------+
// | index of variant type 0 |
// +-------------------------+
// | index of target 1       |
// +-------------------------+
// | index of original 1     |
// +-------------------------+
// | index of variant type 1 |
// +-------------------------+
// | ...                     |
//
// Here, each element is of uint32 type.  Actual strings of target and original
// are stored in |variant_string_array|, while strings of variant type are
// stored in |variant_type|.
void GenerateDescription(absl::string_view variant_token_array,
                         const SerializedStringArray &variant_string_array,
                         const SerializedStringArray &variant_type,
                         const std::string &key, std::string *desc) {
  DCHECK(desc);
  const uint32 *token_array =
      reinterpret_cast<const uint32 *>(variant_token_array.data());
  const size_t token_array_size = variant_token_array.size() / sizeof(uint32);

  const Uint32ArrayIterator<3> end(token_array + token_array_size);
  const auto iter = std::lower_bound(
      Uint32ArrayIterator<3>(token_array), end, key,
      [&variant_string_array](uint32 index, const std::string &target_key) {
        return variant_string_array[index] < target_key;
      });
  if (iter == end || variant_string_array[iter[0]] != key) {
    return;
  }
  const absl::string_view original = variant_string_array[iter[1]];
  const uint32 type_id = iter[2];
  DCHECK_LT(type_id, variant_type.size());
  // Format like "XXXのYYY"
  desc->assign(original.data(), original.size());
  desc->append("の");
  desc->append(variant_type[type_id].data(), variant_type[type_id].size());
}

// Add single kanji variants description to existing candidates,
// because if we have candidates with same value, the lower ranked candidate
// will be removed.
void AddDescriptionForExsistingCandidates(
    absl::string_view variant_token_array,
    const SerializedStringArray &variant_string_array,
    const SerializedStringArray &variant_type, Segment *segment) {
  DCHECK(segment);
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    Segment::Candidate *cand = segment->mutable_candidate(i);
    if (!cand->description.empty()) {
      continue;
    }
    GenerateDescription(variant_token_array, variant_string_array, variant_type,
                        cand->value, &cand->description);
  }
}

void FillCandidate(absl::string_view variant_token_array,
                   const SerializedStringArray &variant_string_array,
                   const SerializedStringArray &variant_type,
                   const std::string &key, const std::string &value, int cost,
                   uint16 single_kanji_id, Segment::Candidate *cand) {
  cand->lid = single_kanji_id;
  cand->rid = single_kanji_id;
  cand->cost = cost;
  cand->content_key = key;
  cand->content_value = value;
  cand->key = key;
  cand->value = value;
  cand->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
  cand->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  GenerateDescription(variant_token_array, variant_string_array, variant_type,
                      value, &cand->description);
}

// Insert SingleKanji into segment.
void InsertCandidate(absl::string_view variant_token_array,
                     const SerializedStringArray &variant_string_array,
                     const SerializedStringArray &variant_type,
                     bool is_single_segment, uint16 single_kanji_id,
                     const std::vector<std::string> &kanji_list,
                     Segment *segment) {
  DCHECK(segment);
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return;
  }

  const std::string &candidate_key =
      ((!segment->key().empty()) ? segment->key() : segment->candidate(0).key);

  // Adding 8000 to the single kanji cost
  // Note that this cost does not make no effect.
  // Here we set the cost just in case.
  const int kOffsetCost = 8000;

  // Append single-kanji
  for (size_t i = 0; i < kanji_list.size(); ++i) {
    Segment::Candidate *c = segment->push_back_candidate();
    FillCandidate(variant_token_array, variant_string_array, variant_type,
                  candidate_key, kanji_list[i], kOffsetCost + i,
                  single_kanji_id, c);
  }
}

void InsertNounPrefix(const POSMatcher &pos_matcher, Segment *segment,
                      SerializedDictionary::iterator begin,
                      SerializedDictionary::iterator end) {
  DCHECK(begin != end);

  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return;
  }

  if (segment->segment_type() == Segment::FIXED_VALUE) {
    return;
  }

  const std::string &candidate_key =
      ((!segment->key().empty()) ? segment->key() : segment->candidate(0).key);
  for (auto iter = begin; iter != end; ++iter) {
    const int insert_pos = std::min(
        static_cast<int>(segment->candidates_size()),
        static_cast<int>(iter.cost() + (segment->candidate(0).attributes &
                                        Segment::Candidate::CONTEXT_SENSITIVE)
                             ? 1
                             : 0));
    Segment::Candidate *c = segment->insert_candidate(insert_pos);
    c->lid = pos_matcher.GetNounPrefixId();
    c->rid = pos_matcher.GetNounPrefixId();
    c->cost = 5000;
    c->content_value = std::string(iter.value());
    c->key = candidate_key;
    c->content_key = candidate_key;
    c->value = std::string(iter.value());
    c->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  }
}

}  // namespace

SingleKanjiRewriter::SingleKanjiRewriter(
    const DataManagerInterface &data_manager)
    : pos_matcher_(data_manager.GetPOSMatcherData()) {
  absl::string_view string_array_data;
  absl::string_view variant_type_array_data;
  absl::string_view variant_string_array_data;
  absl::string_view noun_prefix_token_array_data;
  absl::string_view noun_prefix_string_array_data;
  data_manager.GetSingleKanjiRewriterData(
      &single_kanji_token_array_, &string_array_data, &variant_type_array_data,
      &variant_token_array_, &variant_string_array_data,
      &noun_prefix_token_array_data, &noun_prefix_string_array_data);
  // Single Kanji token array is an array of uint32.  Its size must be multiple
  // of 2; see the comment above LookupKanjiList.
  DCHECK_EQ(0, single_kanji_token_array_.size() % (2 * sizeof(uint32)));
  DCHECK(SerializedStringArray::VerifyData(string_array_data));
  single_kanji_string_array_.Set(string_array_data);

  DCHECK(SerializedStringArray::VerifyData(variant_type_array_data));
  variant_type_array_.Set(variant_type_array_data);

  // Variant token array is an array of uint32.  Its size must be multiple
  // of 3; see the comment above GenerateDescription.
  DCHECK_EQ(0, variant_token_array_.size() % (3 * sizeof(uint32)));
  DCHECK(SerializedStringArray::VerifyData(variant_string_array_data));
  variant_string_array_.Set(variant_string_array_data);

  DCHECK(SerializedDictionary::VerifyData(noun_prefix_token_array_data,
                                          noun_prefix_string_array_data));
  noun_prefix_dictionary_.reset(new SerializedDictionary(
      noun_prefix_token_array_data, noun_prefix_string_array_data));
}

SingleKanjiRewriter::~SingleKanjiRewriter() {}

int SingleKanjiRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool SingleKanjiRewriter::Rewrite(const ConversionRequest &request,
                                  Segments *segments) const {
  if (!request.config().use_single_kanji_conversion()) {
    VLOG(2) << "no use_single_kanji_conversion";
    return false;
  }

  bool modified = false;
  const size_t segments_size = segments->conversion_segments_size();
  const bool is_single_segment = (segments_size == 1);
  for (size_t i = 0; i < segments_size; ++i) {
    AddDescriptionForExsistingCandidates(
        variant_token_array_, variant_string_array_, variant_type_array_,
        segments->mutable_conversion_segment(i));

    const std::string &key = segments->conversion_segment(i).key();
    std::vector<std::string> kanji_list;
    if (!LookupKanjiList(single_kanji_token_array_, single_kanji_string_array_,
                         key, &kanji_list)) {
      continue;
    }
    InsertCandidate(variant_token_array_, variant_string_array_,
                    variant_type_array_, is_single_segment,
                    pos_matcher_.GetGeneralSymbolId(), kanji_list,
                    segments->mutable_conversion_segment(i));

    modified = true;
  }

  // Tweak for noun prefix.
  // TODO(team): Ideally, this issue can be fixed via the language model
  // and dictionary generation.
  for (size_t i = 0; i < segments_size; ++i) {
    if (segments->conversion_segment(i).candidates_size() == 0) {
      continue;
    }

    if (i + 1 < segments_size) {
      const Segment::Candidate &right_candidate =
          segments->conversion_segment(i + 1).candidate(0);
      // right segment must be a noun.
      if (!pos_matcher_.IsContentNoun(right_candidate.lid)) {
        continue;
      }
    } else if (segments_size != 1) {  // also apply if segments_size == 1.
      continue;
    }

    const std::string &key = segments->conversion_segment(i).key();
    const auto range = noun_prefix_dictionary_->equal_range(key);
    if (range.first == range.second) {
      continue;
    }
    InsertNounPrefix(pos_matcher_, segments->mutable_conversion_segment(i),
                     range.first, range.second);
    // Ignore the next noun content word.
    ++i;
    modified = true;
  }

  return modified;
}
}  // namespace mozc
