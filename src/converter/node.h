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

#ifndef MOZC_CONVERTER_NODE_H_
#define MOZC_CONVERTER_NODE_H_

#include <cstdint>
#include <string>

#include "dictionary/dictionary_token.h"

namespace mozc {

struct Node {
  enum NodeType {
    NOR_NODE,  // normal node
    BOS_NODE,  // BOS (beginning of sentence)
    EOS_NODE,  // EOS (end of sentence)
    CON_NODE,  // constrained node
    HIS_NODE,  // history node
  };

  enum Attribute {
    DEFAULT_ATTRIBUTE = 0,
    SYSTEM_DICTIONARY = 1 << 0,      // System dictionary (not used now)
    USER_DICTIONARY = 1 << 1,        // User dictionary
    NO_VARIANTS_EXPANSION = 1 << 2,  // No need to expand full/half
    // Internally used in the converter
    // TODO(toshiyuki): Delete this attribute.
    WEAK_CONNECTED_OBSOLETE = 1 << 3,
    STARTS_WITH_PARTICLE = 1 << 4,  // User input starts with particle
    SPELLING_CORRECTION = 1 << 5,   // "Did you mean"
    // Equal to that of Candidate.
    // Life of suggestion candidates from realtime conversion is;
    // 1. Created by ImmutableConverter as Candidate instance.
    // 2. The Candidate instances are aggregated as Node instances
    //    in DictionaryPredictor::AggregateRealtimeConversion.
    // 3. The Node instances are converted into Candidate instances
    //    in DictionaryPredictor::AddPredictionToCandidates.
    // To propagate this information from Node to Candidate,
    // Node should have the same information as Candidate.
    PARTIALLY_KEY_CONSUMED = 1 << 7,
    SUFFIX_DICTIONARY = 1 << 8,  // Suffix dictionary
    KEY_EXPANDED = 1 << 9,       // Keys are expanded.
  };

  // prev and next are linking pointers to connect minimum cost path
  // in the lattice. In other words, we can think the doubly-linked list
  // with prev/next represents the minimum cost path.
  Node* prev;
  Node* next;

  // bnext points to another Node instance which shares the same beginning
  // position of the key.
  // enext points to another Node instance which shares the same ending
  // position of the key.
  //
  // Here illustrates the image:
  //
  // key:         | 0 | 1 | 2 | 3 | 4 | 5 | 6 | ... | N |
  // begin_nodes: | 0 | 1 | 2 | 3 | 4 | 5 | 6 | ... | N | (in lattice)
  //                |   |   :   :   :   :   :         :
  //                |   :
  //                |   :          (nullptr)
  //                |   :           ^
  //                |   :           :
  //                v   :           |
  //               +-----------------+
  //               | Node1(len4)     |
  //               +-----------------+
  //           bnext|   :           ^
  //                v   :           |enext
  //               +-----------------+
  //               | Node2(len4)     | (nullptr)
  //               +-----------------+  ^
  //           bnext|   :           ^   :
  //                |   :           |   :
  //                v   :           :   |enext
  //               +---------------------+
  //               | Node3(len5)         |
  //               +---------------------+ (nullptr)
  //           bnext|   :           :   ^   ^
  //                |   :           :   |   :
  //                v   :           :   :   |enext
  //               +-------------------------+
  //               | Node4(len6)             |
  //               +-------------------------+
  //           bnext|   :           :   :   ^
  //                :   :           :   :   |
  //                v   :           :   :   :
  //          (nullptr) |           :   :   :
  //                    v           :   |enext
  //                   +-----------------+  :
  //                   | Node5(len4)     |  :
  //                   +-----------------+  :
  //               bnext|           :   ^   :
  //                    v           :   |enext
  //                   +-----------------+  :
  //                   | Node6(len4)     |  :
  //                   +-----------------+  :
  //               bnext|           :   ^   :
  //                    |           :   |   :
  //                    v           :   :   |enext
  //                   +---------------------+
  //                   | Node7(len5)         |
  //                   +---------------------+
  //               bnext|           :   :   ^
  //                    v           :   :   |enext
  //                   +---------------------+
  //                   | Node8(len5)         |
  //                   +---------------------+
  //               bnext|           :   :   ^
  //                    :           :   :   |
  //                    v           :   :   |
  //               (nullptr)        :   :   |
  //                                :   :   |
  //                :   :   :   :   :   |   |         :
  // end_nodes:   | 0 | 1 | 2 | 3 | 4 | 5 | 6 | ... | N |  (in lattice)
  //
  // Note:
  // 1) Nodes 1, 2, 3 and 4 start with pos "0", so they are connected by
  //    bnext. Same for Nodes 5, 6, 7 and 8.
  // 2) Nodes 3, 5 and 6 end with pos "5", so they are connected by enext.
  //    Same for Nodes 4, 7 and 8.
  Node* bnext;
  Node* enext;

  // if it is not nullptr, transition cost
  // from constrained_prev to current node is defined,
  // other transition is set to be infinite
  Node* constrained_prev;

  uint16_t rid;
  uint16_t lid;
  uint16_t begin_pos;
  uint16_t end_pos;

  // wcost: word cost for the node; it may be changed after lookup
  // cost: the total cost between BOS and the node
  // raw_wcost: raw word cost for the node; it is not changed after lookup.
  //            It is used for the cache of lattice.
  int32_t wcost;
  int32_t cost;
  int32_t raw_wcost;

  NodeType node_type;
  uint32_t attributes;

  // key: The user input.
  // actual_key: The actual search key that corresponds to the value.
  //           Can differ from key when no modifier conversion is enabled.
  // value: The surface form of the word.
  std::string key;
  std::string actual_key;
  std::string value;

  Node() { Init(); }

  inline void Init() {
    prev = nullptr;
    next = nullptr;
    bnext = nullptr;
    enext = nullptr;
    constrained_prev = nullptr;
    rid = 0;
    lid = 0;
    begin_pos = 0;
    end_pos = 0;
    node_type = NOR_NODE;
    wcost = 0;
    cost = 0;
    raw_wcost = 0;
    attributes = 0;
    key.clear();
    actual_key.clear();
    value.clear();
  }

  inline void InitFromToken(const dictionary::Token& token) {
    prev = nullptr;
    next = nullptr;
    bnext = nullptr;
    enext = nullptr;
    constrained_prev = nullptr;
    rid = token.rid;
    lid = token.lid;
    begin_pos = 0;
    end_pos = 0;
    node_type = NOR_NODE;
    wcost = token.cost;
    cost = 0;
    raw_wcost = 0;
    attributes = 0;
    if (token.attributes & dictionary::Token::SPELLING_CORRECTION) {
      attributes |= SPELLING_CORRECTION;
    }
    if (token.attributes & dictionary::Token::SUFFIX_DICTIONARY) {
      attributes |= SUFFIX_DICTIONARY;
    }
    if (token.attributes & dictionary::Token::USER_DICTIONARY) {
      attributes |= USER_DICTIONARY;
      attributes |= NO_VARIANTS_EXPANSION;
    }
    key = token.key;
    actual_key.clear();
    value = token.value;
  }
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_NODE_H_
