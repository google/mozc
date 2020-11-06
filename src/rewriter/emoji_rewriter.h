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

#ifndef MOZC_REWRITER_EMOJI_REWRITER_H_
#define MOZC_REWRITER_EMOJI_REWRITER_H_

#include <cstddef>
#include <iterator>
#include <utility>

#include "base/serialized_string_array.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "rewriter/rewriter_interface.h"
#include "absl/strings/string_view.h"

namespace mozc {

class ConversionRequest;

// EmojiRewriter class adds UTF-8 emoji characters in converted candidates of
// given segments, if each segment has a special key to convert.
// Added emoji characters are chosen by Yomi (reading of it) registered in
// a dictionary. If a segment has a key "えもじ", all emoji characters are
// pushed to its candidate list.
//
// Usage:
//
//   mozc::Segments segments;
//   mozc::Segment *segment = segments.add_segment();
//   mozc::Segment::Candidate *candidate = segment->add_candidate();
//   candidate->set_key("えもじ");
//
//   // Use one of data manager from data_manager/.
//   mozc::EmojiRewriter rewriter(data_manager);
//   rewriter.Rewrite(mozc::ConvresionRequest(), &segments);
//
// Here, the first segment of segments is expected to have all emoji
// characters in its candidates' values.  You can see them as such:
//
//   for (size_t i = 0; i < segment->candidate_size(); ++i) {
//     LOG(INFO) << segment->candidate(i).value;
//   }
class EmojiRewriter : public RewriterInterface {
 public:
  static constexpr size_t kEmojiDataByteLength = 28;

  // Emoji data token is 28 bytes data of the following format:
  //
  // +-------------------------------------+
  // | Key index (4 byte)                  |
  // +-------------------------------------+
  // | UTF8 emoji index (4 byte)           |
  // +-------------------------------------+
  // | Android PUA code (4 byte)           |
  // +-------------------------------------+
  // | UTF8 description index (4 byte)     |
  // +-------------------------------------+
  // | Docomo description index (4 byte)   |
  // +-------------------------------------+
  // | Softbank description index (4 byte) |
  // +-------------------------------------+
  // | KDDI description index (4 byte)     |
  // +-------------------------------------+
  //
  // Here, index is the position in the string array at which the corresponding
  // string value is stored.  Tokens are sorted in order of key so that it can
  // be search by binary search.
  //
  // The following iterator class can be used to iterate over token array.
  class EmojiDataIterator
      : public std::iterator<std::random_access_iterator_tag, uint32> {
   public:
    EmojiDataIterator() : ptr_(nullptr) {}
    explicit EmojiDataIterator(const char *ptr) : ptr_(ptr) {}

    uint32 key_index() const { return *reinterpret_cast<const uint32 *>(ptr_); }
    uint32 emoji_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 4);
    }
    uint32 android_pua() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 8);
    }
    uint32 description_utf8_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 12);
    }
    uint32 description_docomo_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 16);
    }
    uint32 description_softbank_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 20);
    }
    uint32 description_kddi_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 24);
    }

    // Returns key index as token array is searched by key.
    uint32 operator*() const { return key_index(); }

    void swap(EmojiDataIterator &x) {
      using std::swap;
      swap(ptr_, x.ptr_);
    }
    friend void swap(EmojiDataIterator &x, EmojiDataIterator &y) {
      return x.swap(y);
    }

    EmojiDataIterator &operator++() {
      ptr_ += kEmojiDataByteLength;
      return *this;
    }

    EmojiDataIterator operator++(int) {
      const char *tmp = ptr_;
      ptr_ += kEmojiDataByteLength;
      return EmojiDataIterator(tmp);
    }

    EmojiDataIterator &operator--() {
      ptr_ -= kEmojiDataByteLength;
      return *this;
    }

    EmojiDataIterator operator--(int) {
      const char *tmp = ptr_;
      ptr_ -= kEmojiDataByteLength;
      return EmojiDataIterator(tmp);
    }

    EmojiDataIterator &operator+=(ptrdiff_t n) {
      ptr_ += n * kEmojiDataByteLength;
      return *this;
    }

    EmojiDataIterator &operator-=(ptrdiff_t n) {
      ptr_ -= n * kEmojiDataByteLength;
      return *this;
    }

    friend EmojiDataIterator operator+(EmojiDataIterator x, ptrdiff_t n) {
      return x += n;
    }

    friend EmojiDataIterator operator+(ptrdiff_t n, EmojiDataIterator x) {
      return x += n;
    }

    friend EmojiDataIterator operator-(EmojiDataIterator x, ptrdiff_t n) {
      return x -= n;
    }

    friend ptrdiff_t operator-(EmojiDataIterator x, EmojiDataIterator y) {
      return (x.ptr_ - y.ptr_) / kEmojiDataByteLength;
    }

    friend bool operator==(EmojiDataIterator x, EmojiDataIterator y) {
      return x.ptr_ == y.ptr_;
    }

    friend bool operator!=(EmojiDataIterator x, EmojiDataIterator y) {
      return x.ptr_ != y.ptr_;
    }

    friend bool operator<(EmojiDataIterator x, EmojiDataIterator y) {
      return x.ptr_ < y.ptr_;
    }

    friend bool operator<=(EmojiDataIterator x, EmojiDataIterator y) {
      return x.ptr_ <= y.ptr_;
    }

    friend bool operator>(EmojiDataIterator x, EmojiDataIterator y) {
      return x.ptr_ > y.ptr_;
    }

    friend bool operator>=(EmojiDataIterator x, EmojiDataIterator y) {
      return x.ptr_ >= y.ptr_;
    }

   private:
    const char *ptr_ = nullptr;
  };

  using IteratorRange = std::pair<EmojiDataIterator, EmojiDataIterator>;

  // This class does not take an ownership of |emoji_data_list|, |token_list|
  // and |value_list|.  If NULL pointer is passed to it, Mozc process
  // terminates with an error.
  explicit EmojiRewriter(const DataManagerInterface &data_manager);
  ~EmojiRewriter() override;

  int capability(const ConversionRequest &request) const override;

  // Returns true if emoji candidates are added.  When user settings are set
  // not to use EmojiRewriter, does nothing other than returning false.
  // Otherwise, main process are done in ReriteCandidates().
  // A reference to a ConversionRequest instance is not used, but it is required
  // because of the interface.
  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

  // Counts the number of segments in which emoji candidates are selected,
  // and stores the result as usage stats.
  // NOTE: This method is expected to be called after the segments are processed
  // with COMMIT command in a SessionConverter instance.  May record wrong
  // stats if you call this method in other situation.
  void Finish(const ConversionRequest &request, Segments *segments) override;

  // Returns true if the given candidate includes emoji characters.
  // TODO(peria, hidehiko): Unify this checker and IsEmojiEntry defined in
  //     predictor/user_history_predictor.cc.  If you make similar functions
  //     before the merging in case, put a same note to avoid twisted
  //     dependency.
  static bool IsEmojiCandidate(const Segment::Candidate &candidate);

 private:
  EmojiDataIterator begin() const {
    return EmojiDataIterator(token_array_data_.data());
  }
  EmojiDataIterator end() const {
    return EmojiDataIterator(token_array_data_.data() +
                             token_array_data_.size());
  }

  // Adds emoji candidates on each segment of given segments, if it has a
  // specific string as a key based on a dictionary.  If a segment's value is
  // "えもじ", adds all emoji candidates.
  // Returns true if emoji candidates are added in any segment.
  bool RewriteCandidates(Segments *segments) const;

  IteratorRange LookUpToken(absl::string_view key) const;

  absl::string_view token_array_data_;
  SerializedStringArray string_array_;

  DISALLOW_COPY_AND_ASSIGN(EmojiRewriter);
};

}  // namespace mozc

#endif  // MOZC_REWRITER_EMOJI_REWRITER_H_
