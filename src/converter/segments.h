// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_CONVERTER_SEGMENTS_H_
#define MOZC_CONVERTER_SEGMENTS_H_

#include <deque>
#include <vector>
#include <string>
#include "base/base.h"

namespace mozc {

struct Node;
class NodeAllocatorInterface;
class Lattice;
template <class T> class ObjectPool;

namespace composer {
  class Composer;
}  // namespace composer

class Segment {
 public:
  enum SegmentType {
    FREE,            // FULL automatic conversion.
    FIXED_BOUNDARY,  // cannot consist of multiple segments.
    FIXED_VALUE,     // cannot consist of multiple segments.
                     // and result is also fixed
    SUBMITTED,       // submitted node
    HISTORY          // history node. It is hidden from user.
  };

  struct Candidate {
    string key;         // reading
    string value;       // surface form
    string content_key;
    string content_value;

    // Meta information
    string prefix;
    string suffix;
    // Description including description type and message
    string description;
    // Title of the usage containing basic form of this candidate.
    string usage_title;
    // Content of the usage.
    string usage_description;

    // Context "sensitive" candidate cost.
    // Taking adjacent words/nodes into consideration.
    // Basically, canidate is sorted by this cost.
    int32  cost;
    // Context "free" candidate cost
    // NOT taking adjacent words/nodes into consideration.
    int32  wcost;
    // (cost without transition cost between left/right boundaries)
    // Cost of only transitions (cost without word cost adjacent context)
    int32  structure_cost;

    // lid of left-most node
    uint16 lid;
    // rid of right-most node
    uint16 rid;

    // Attributes of this candidate. Can set multiple attributes
    // defined in enum |Attribute|.
    uint32 attributes;

    // Candidate style. This is not a bit-field.
    // The style is defined in enum |Style|.
    uint32 style;

    void Init() {
      key.clear();
      value.clear();
      content_value.clear();
      content_key.clear();
      prefix.clear();
      suffix.clear();
      description.clear();
      usage_title.clear();
      usage_description.clear();
      cost = 0;
      structure_cost = 0;
      wcost = 0;
      lid = 0;
      rid = 0;
      attributes = 0;
      style = 0;
    }

    Candidate() : cost(0), wcost(0), structure_cost(0),
                  lid(0), rid(0),
                  attributes(0), style(0) {}

    enum Attribute {
      DEFAULT_ATTRIBUTE = 0,
      BEST_CANDIDATE = 1,        // this was the best candidate before learning
      RERANKED = 2,                // this candidate was reranked by user
      NO_HISTORY_LEARNING = 4,     // don't save it in history
      NO_SUGGEST_LEARNING = 8,     // don't save it in suggestion
      NO_LEARNING = (4 | 8),       // NO_HISTORY_LEARNING | NO_SUGGEST_LEARNING
      CONTEXT_SENSITIVE = 16,      // learn it with left/right context
      SPELLING_CORRECTION = 32,    // has "did you mean"
      NO_VARIANTS_EXPANSION = 64,  // No need to have full/half width expansion
      NO_EXTRA_DESCRIPTION = 128   // No need to have extra descriptions.
    };

    enum Style {
      DEFAULT_STYLE= 0,
      NUMBER_SEPARATED_ARABIC_HALFWIDTH,
      NUMBER_SEPARATED_ARABIC_FULLWIDTH,
      NUMBER_KANJI,
      NUMBER_OLD_KANJI,
      NUMBER_ROMAN_CAPITAL,
      NUMBER_ROMAN_SMALL,
      NUMBER_CIRCLED,
      NUMBER_HEX,
      NUMBER_OCT,
      NUMBER_BIN,
      NUMBER_KANJI_ARABIC,  // "ニ〇〇"
    };

    // return functional key
    // functional_key =
    // key.substr(content_key.size(), key.size() - content_key.size());
    string functional_key() const;

    // return functional value
    // functional_value =
    // value.substr(content_value.size(), value.size() - content_value.size());
    string functional_value() const;
  };

  const SegmentType segment_type() const;
  SegmentType *mutable_segment_type();
  void set_segment_type(const SegmentType &segment_type);

  const string& key() const;
  void set_key(const string &key);

  // Candidate manupluations
  // getter
  const Candidate &candidate(int i) const;

  // setter
  Candidate *mutable_candidate(int i);

  // return the index of candidate
  // if candidate is not found, return candidates_size()
  int indexOf(const Candidate *candidate);

  // push and insert candidates
  Candidate *push_front_candidate();
  Candidate *push_back_candidate();
  Candidate *add_candidate();   // alias of push_back_candidate()
  Candidate *insert_candidate(int i);

  // get size of candidates
  size_t candidates_size() const;

  // erase candidate
  void pop_front_candidate();
  void pop_back_candidate();
  void erase_candidate(int i);
  void erase_candidates(int i, size_t size);

  // erase all candidates
  // do not erase meta candidates
  void clear_candidates();

  // meta candidates
  // TODO(toshiyuki): Integrate meta candidates to candidate and delete these
  size_t meta_candidates_size() const;
  void clear_meta_candidates();
  const vector<Candidate> &meta_candidates() const;
  vector<Candidate> *mutable_meta_candidates();
  const Candidate &meta_candidate(size_t i) const;
  Candidate *mutable_meta_candidate(size_t i);
  Candidate *add_meta_candidate();

  // move old_idx-th-candidate to new_index
  void move_candidate(int old_idx, int new_idx);

  void Clear();

  // Keep clear() method as other modules are still using the old method
  void clear() { Clear(); }

  Segment();
  virtual ~Segment();

 private:
  SegmentType segment_type_;
  string key_;
  deque<Candidate *> candidates_;
  vector<Candidate>  meta_candidates_;
  bool initialized_transliterations_;
  scoped_ptr<ObjectPool<Candidate> > pool_;
  DISALLOW_COPY_AND_ASSIGN(Segment);
};

// Segments is basically an array of Segment.
// Note that there are two types of Segment
// a) History Segment (SegmentType == HISTORY OR SUBMITTED)
//    Segments user entered just before the transacton
// b) Conversion Segment
//    Current segments user inputs
//
// Array of segment is represented as an array as follows
// segments_array[] = {HS_0,HS_1,...HS_N, CS0, CS1, CS2...}
//
// * segment(i) and mutable_segment(int i)
//  access segment regardless of History/Conversion distinctions
//
// * history_segment(i) and mutable_history_segment(i)
//  access only History Segment
//
// conversion_segment(i) and mutable_conversion_segment(i)
//  access only Conversion Segment
//  segment(i + history_segments_size()) == conversion_segment(i)
class Segments {
 public:
  enum RequestType {
    CONVERSION,  // normal conversion
    REVERSE_CONVERSION,  // reverse conversion
    PREDICTION,  // show prediction with user tab key
    SUGGESTION,  // show prediction automatically
  };

  // Client of segments can remember any string which can be used
  // to revert the last Finish operation.
  // "id" can be used for identifying the purpose of the key;
  struct RevertEntry {
    enum RevertEntryType {
      CREATE_ENTRY,
      UPDATE_ENTRY,
    };
    uint16 revert_entry_type;
    // UserHitoryPredictor uses '1' for now.
    // Do not use duplicate keys.
    uint16 id;
    uint32 timestamp;
    string key;
    RevertEntry() : revert_entry_type(0), id(0), timestamp(0) {}
  };

  RequestType request_type() const;
  void set_request_type(RequestType request_type);

  // enable/disable user history
  void set_user_history_enabled(bool user_history_enabled);
  bool user_history_enabled() const;

  // getter
  const Segment &segment(size_t i) const;
  const Segment &conversion_segment(size_t i) const;
  const Segment &history_segment(size_t i) const;

  // setter
  Segment *mutable_segment(size_t i);
  Segment *mutable_conversion_segment(size_t i);
  Segment *mutable_history_segment(size_t i);

  // push and insert segments
  Segment *push_front_segment();
  Segment *push_back_segment();
  Segment *add_segment();   // alias of push_back_segment()
  Segment *insert_segment(size_t i);

  // get size of segments
  size_t segments_size() const;
  size_t history_segments_size() const;
  size_t conversion_segments_size() const;

  // erase segment
  void pop_front_segment();
  void pop_back_segment();
  void erase_segment(size_t i);
  void erase_segments(size_t i, size_t size);

  // erase all segments
  void clear_history_segments();
  void clear_conversion_segments();
  void clear_segments();

  void set_max_history_segments_size(size_t max_history_segments_size);
  size_t max_history_segments_size() const;

  // Let predictor know the maximum size of
  // candidates prediction/suggestion can generate.
  void set_max_prediction_candidates_size(size_t size);
  size_t max_prediction_candidates_size() const;

  // Let converter know the maximum size of
  // candidates converter can generate.
  // NOTE: This field is used as an "optional" field.
  // Rewriter might insert more than |size| candiates.
  // Default setting is 200.
  void set_max_conversion_candidates_size(size_t size);
  size_t max_conversion_candidates_size() const;

  bool resized() const;
  void set_resized(bool resized);

  const composer::Composer *composer() const;
  void set_composer(const composer::Composer *composer);

  // Removes specified number of characters at the end of history segments.
  void RemoveTailOfHistorySegments(size_t num_of_characters);

  // lattice
  void clear_lattice();

  // clear segments and lattice
  void Clear();

  // Dump Segments structure
  string DebugString() const;

  // return lattice instance
  Lattice *lattice() const;

  NodeAllocatorInterface *node_allocator() const;

  // Revert entries
  void clear_revert_entries();
  size_t revert_entries_size() const;
  RevertEntry *push_back_revert_entry();
  const RevertEntry &revert_entry(size_t i) const;
  RevertEntry *mutable_revert_entry(size_t i);

  Segments();
  virtual ~Segments();

 private:
  size_t max_history_segments_size_;
  size_t max_prediction_candidates_size_;
  size_t max_conversion_candidates_size_;
  bool resized_;
  bool user_history_enabled_;

  RequestType request_type_;
  scoped_ptr<Lattice> lattice_;
  scoped_ptr<ObjectPool<Segment> > pool_;
  deque<Segment *> segments_;
  vector<RevertEntry> revert_entries_;
  const composer::Composer *composer_;

  DISALLOW_COPY_AND_ASSIGN(Segments);
};
}

#endif  // MOZC_CONVERTER_SEGMENTS_H_
