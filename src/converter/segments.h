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

#ifndef MOZC_CONVERTER_SEGMENTS_H_
#define MOZC_CONVERTER_SEGMENTS_H_

#include <deque>
#include <vector>
#include <string>
#include "base/base.h"

namespace mozc {

struct Node;
class NBestGenerator;
class ConverterData;
template <class T> class ObjectPool;

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
    string value;
    string content_value;
    string content_key;
    string prefix;
    string suffix;
    // Description including description type and message
    string description;
    // Description message
    string description_message;
    // Title of the usage containing basic form of this candidate.
    string usage_title;
    // Content of the usage.
    string usage_description;
    int32  cost;
    // Cost of transitions (cost without word cost adjacent context)
    int32  structure_cost;
    uint16 lid;
    uint16 rid;
    uint8  learning_type;
    uint8  style;  // candidate style added by rewriters
    bool can_expand_alternative;  // Can expand full/half width form
    // for experimant of removing noisy candidates
    // TODO(toshiyuki): delete this member after the experiment
    vector<const Node *> nodes;
    void Init() {
      value.clear();
      content_value.clear();
      content_key.clear();
      prefix.clear();
      suffix.clear();
      description.clear();
      usage_title.clear();
      usage_description.clear();
      nodes.clear();
      cost = 0;
      structure_cost = 0;
      lid = 0;
      rid = 0;
      learning_type = 0;
      style = 0;
      can_expand_alternative = true;
    }

    Candidate()
        : cost(0), lid(0), rid(0),
          learning_type(0), style(0),
          can_expand_alternative(true) {}

    enum LearningType {
      DEFAULT_LEARNING = 0,
      BEST_CANDIDATE = 1,     // this was the best candidate before learning
      RERANKED = 2,           // this candidate was reranked by user
      NO_HISTORY_LEARNING = 4,   // don't save it in history
      NO_SUGGEST_LEARNING = 8,   // don't save it in suggestion
      NO_LEARNING = (4 | 8),     // NO_HISTORY_LEARNING | NO_SUGGEST_LEARNING
      CONTEXT_SENSITIVE = 16,    // learn it with left/right context
    };

    // 1) Full width / half width description
    // 2) CharForm (hiragana/katakana) description
    // 3) Platform dependent char (JISX0213..etc) description
    // 4) Zipcode description (XXX-XXXX)
    //     * note that this overrides other descriptions
    enum DescriptionType {
      FULL_HALF_WIDTH = 1,
      CHARACTER_FORM = 2,
      PLATFORM_DEPENDENT_CHARACTER = 4,
      ZIPCODE = 8,
    };

    // Candidate types
    enum Style {
      DEFAULT = 0,
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

    // Set description:
    // Can specify which types of descriptions is added with type argument.
    // e.g, type = FULL_HALF_WIDTH | PLATFORM_DEPENDENT_CHARACTER;
    // Note that internal description is cleared.
    void SetDefaultDescription(int type);

    // Set description for transliterations:
    void SetTransliterationDescription();

    // Instead of setting CHARACTER_FORM description,
    // you can set any message with this method. CHARACTER_FORM is
    // ignored. For instance, symbol_rewriter can use this
    // method to display description "矢印" to "→" by passing
    // message = "矢印"
    void SetDescription(int type,
                        const string &message);

    // Reset description keeping description_message
    void ResetDescription(int type);
  };

  const SegmentType segment_type() const;
  SegmentType *mutable_segment_type();
  void set_segment_type(const SegmentType &segment_type);

  const string& key() const;
  void set_key(const string &key);

  // Set transliterations which is ordered by
  // transliteration::TransliterationTypes (HIRAGANA, FULL_KATAKAKA,
  // HALF_ASCII, FULL_ASCII and HALF_KATAKANA).
  void SetTransliterations(const vector<string> &transliterations);

  // Return true if transliterations has been initialized by the above
  // SetTransliterations.
  bool initialized_transliterations() { return initialized_transliterations_; }

  // Candidate manupluations
  // getter
  const Candidate &candidate(int i) const;
  const Candidate &meta_candidate(size_t i) const;

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
  size_t meta_candidates_size() const;
  size_t requested_candidates_size() const;

  // erase candidate
  void pop_front_candidate();
  void pop_back_candidate();
  void erase_candidate(int i);
  void erase_candidates(int i, size_t size);

  // erase all candidates
  void clear_candidates();

  // move old_idx-th-candidate to new_index
  void move_candidate(int old_idx, int new_idx);

  // return true if value is alrady in the candidate
  // do not call this method frequently, it just does linear search
  bool has_candidate_value(const string &value) const;

  NBestGenerator *nbest_generator() const;

  bool Expand(size_t size);
  bool GetCandidates(size_t size);

  // When candidate(i).value() has both halfwidth and fullwidth
  // form, insert an alternate candidate at i + 1.
  // The preference of halfwidth/fullwidth is determined
  // by CharacterFormManager. For example, candidate(i).value is
  // HALF_WIDTH, and the character form preference is FULL_WIDTH,
  // this method does
  //   1. make a copy of candidate at i and insert it at i + 1
  //   2. change the candidate at i to be FULL WIDTH
  //   3. change the candidate at i + 1 to be HALF WIDTH
  // return true if the candidate is expanded.
  bool ExpandAlternative(int i);

  bool Reset();
  void clear();
  void Clear();

  Segment();
  virtual ~Segment();

 private:
  SegmentType segment_type_;
  string key_;
  deque<Candidate *> candidates_;
  vector<Candidate>  meta_candidates_;
  size_t requested_candidates_size_;
  scoped_ptr<NBestGenerator> nbest_generator_;
  scoped_ptr<ObjectPool<Candidate> > pool_;
  bool is_numbers_expanded_;
  bool initialized_transliterations_;
  // Maximum prefix length of Katakana t13n candidate.
  // We only allow that one katakana key is converted into English.
  // This length saves the length of Katakana which appeared at first.
  // TODO(taku): remove it when we improve the candidate filter's accuracy
  size_t katakana_t13n_length_;
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
  void enable_user_history();
  void disable_user_history();
  bool use_user_history() const;

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

  bool has_resized() const;
  void set_resized(bool resized);

  // lattice
  void clear_lattice();
  bool has_lattice() const;

  // clear segments and lattice
  void clear();
  void Clear();

  Node *bos_node() const;
  Node *eos_node() const;

  void DebugString(string *output) const;

  ConverterData* converter_data();

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
  bool resized_;
  bool use_user_history_;
  RequestType request_type_;
  scoped_ptr<ConverterData> converter_data_;
  scoped_ptr<ObjectPool<Segment> > pool_;
  deque<Segment *> segments_;
  vector<RevertEntry> revert_entries_;

  DISALLOW_COPY_AND_ASSIGN(Segments);
};
}

#endif  // MOZC_CONVERTER_SEGMENTS_H_
