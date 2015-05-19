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

#include "storage/louds/louds_trie_builder.h"

#include <algorithm>
#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "storage/louds/bit_stream.h"

namespace mozc {
namespace storage {
namespace louds {

LoudsTrieBuilder::LoudsTrieBuilder() : built_(false) {
}

void LoudsTrieBuilder::Add(const string &word) {
  CHECK(!built_);
  CHECK(!word.empty());
  word_list_.push_back(word);
}

namespace {

// A pair of word and its original index in the (sorted) word_list_.
class Entry {
 public:
  Entry(const string &word, size_t original_index)
      : word_(&word), original_index_(original_index) {
  }

  const string &word() const { return *word_; }
  size_t original_index() const { return original_index_; }

 private:
  const string *word_;
  size_t original_index_;
};

class EntryLengthLessThan {
 public:
  explicit EntryLengthLessThan(size_t length) : length_(length) {
  }

  bool operator()(const Entry &entry) {
    return entry.word().length() < length_;
  }

 private:
  size_t length_;
};

void PushInt(size_t value, string* image) {
  // Make sure the value is fit in the 32-bit value.
  CHECK_EQ(value & ~0xFFFFFFFF, 0);

  // Output LSB to MSB.
  image->push_back(static_cast<char>(value & 0xFF));
  image->push_back(static_cast<char>((value >> 8) & 0xFF));
  image->push_back(static_cast<char>((value >> 16) & 0xFF));
  image->push_back(static_cast<char>((value >> 24) & 0xFF));
}
}  // namespace

void LoudsTrieBuilder::Build() {
  CHECK(!built_);

  // Initialize for the build. Sort and de-dup the words.
  sort(word_list_.begin(), word_list_.end());
  word_list_.erase(
      unique(word_list_.begin(), word_list_.end()), word_list_.end());
  vector<Entry> entry_list;
  entry_list.reserve(word_list_.size());
  for (size_t i = 0; i < word_list_.size(); ++i) {
    entry_list.push_back(Entry(word_list_[i], i));
  }
  id_list_.resize(word_list_.size(), - 1);

  // Output the tree to streams.
  BitStream trie_stream;
  BitStream terminal_stream;
  string edge_character;

  // Push root.
  trie_stream.PushBit(1);
  trie_stream.PushBit(0);
  edge_character.push_back('\0');
  terminal_stream.PushBit(0);

  // Then, traverse the sorted word list.
  // The basic concept to output the trie is simple:
  // - Iterate the depth beginning with 0.
  // - If the entry is the first one in the word list, the corresponding
  //   node should be newly created.
  // - If the prefix[0:depth] (inclusive) is different from the previous entry
  //   (if exists), the corresponding node should be newly created.
  // - Otherwise, the node should be shared with the previous entry.
  // So, if it turned out that we need to create a new node, just output '1'
  // for LOUDS to represent an "edge," output the corresponding character,
  // and output a bit representing whether the node is terminal or not.
  // In addition, output the 'id' of the word.
  //
  // Then we check if we need to output '0' for LOUDS as the stop bit of
  // a node. It should be done when the entry is the last child of its parent.
  // - If the entry is the last one in the word list, it should be the last
  //   child of its parent.
  // - If the prefix[0:depth) (exclusive, i.e. [0:depth - 1] inclusive) is
  //   different from the next entry (if exists), it should be the last child
  //   of its parent.
  // - Otherwise, the node is not the last child of its parent, because it
  //   is shared with the next entry.
  //
  // Finally, to keep the pre-condition of above algorithms, we remove
  // entries which we already output.
  //
  // Here, there is an issue. Considering a very simple case; only 'a' is in
  // the word list.
  // At first, output '1' to LOUDS stream, and 'a' to the edge character.
  // Also as it is the terminal, output '1' to the terminal stream and
  // store the id '0'.
  // Then, as 'a' is the last entry, output '0' to the LOUDS stream.
  // Then 'a' is removed since it has already been output as a terminal node.
  // Now, look at the LOUDS stream, it's '10'. However, '100' is expected,
  // because the child node also needs stop bit '0' as a leaf.
  // To achieve this, we keep entries which are output as terminals one more
  // depth, and skip "edge check" for the entries.
  // This doesn't break the edge check condition, and stop bit check condition,
  // but adds a chance to output stop bits for leaves.
  int id = 0;
  for (size_t depth = 0; !entry_list.empty(); ++depth) {
    for (size_t i = 0; i < entry_list.size(); ++i) {
      const string &word = entry_list[i].word();
      if (word.length() > depth &&
          (i == 0 ||
           // To ensure the entry_list[i - 1].word().length >= depth + 1,
           // we call c_str() (which adds '\0' if necessary) as a hack.
           word.compare(
               0, depth + 1,
               entry_list[i - 1].word().c_str(), 0, depth + 1) != 0)) {
        // This is the first string of this node. Output an edge.
        trie_stream.PushBit(1);
        edge_character.push_back(entry_list[i].word()[depth]);

        if (entry_list[i].word().length() == depth + 1) {
          // This is a terminal node.
          // Note that the terminal string should be at the first of
          // strings sharing the node. So the check above should work well.
          terminal_stream.PushBit(1);
          id_list_[entry_list[i].original_index()] = id;
          ++id;
        } else {
          // This is not a terminal node.
          terminal_stream.PushBit(0);
        }
      }

      if (i == entry_list.size() - 1 ||
          word.compare(0, depth, entry_list[i + 1].word(), 0, depth) != 0) {
        // This is the last child (string) for the parent.
        trie_stream.PushBit(0);
      }
    }

    // Remove all terminal strings.
    entry_list.erase(
        remove_if(entry_list.begin(), entry_list.end(),
                  EntryLengthLessThan(depth + 1)),
        entry_list.end());
  }

  // Set 32-bits alignment.
  trie_stream.FillPadding32();
  terminal_stream.FillPadding32();

  // Output
  PushInt(trie_stream.ByteSize(), &image_);
  PushInt(terminal_stream.ByteSize(), &image_);
  // The num bits of each character annoated to each edge.
  PushInt(8, &image_);
  PushInt(edge_character.size(), &image_);

  image_.append(trie_stream.image());
  image_.append(terminal_stream.image());
  image_.append(edge_character);

  built_ = true;
}

const string &LoudsTrieBuilder::image() const {
  CHECK(built_);
  return image_;
}

int LoudsTrieBuilder::GetId(const string &word) const {
  CHECK(built_);

  // Binary search the word.
  vector<string>::const_iterator iter =
      lower_bound(word_list_.begin(), word_list_.end(), word);
  if (iter == word_list_.end() || *iter != word) {
    // Not found.
    return -1;
  }

  return id_list_[distance(word_list_.begin(), iter)];
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
