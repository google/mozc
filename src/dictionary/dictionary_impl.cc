// Copyright 2010-2014, Google Inc.
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

#include "dictionary/dictionary_impl.h"

#include <string>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/node.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/node_list_builder.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"

namespace mozc {
namespace dictionary {

DictionaryImpl::DictionaryImpl(
    const DictionaryInterface *system_dictionary,
    const DictionaryInterface *value_dictionary,
    DictionaryInterface *user_dictionary,
    const SuppressionDictionary *suppression_dictionary,
    const POSMatcher *pos_matcher)
    : pos_matcher_(pos_matcher),
      system_dictionary_(system_dictionary),
      value_dictionary_(value_dictionary),
      user_dictionary_(user_dictionary),
      suppression_dictionary_(suppression_dictionary) {
  CHECK(pos_matcher_);
  CHECK(system_dictionary_.get());
  CHECK(value_dictionary_.get());
  CHECK(user_dictionary_);
  CHECK(suppression_dictionary_);
  dics_.push_back(system_dictionary_.get());
  dics_.push_back(value_dictionary_.get());
  dics_.push_back(user_dictionary_);
}

DictionaryImpl::~DictionaryImpl() {
  dics_.clear();
}

bool DictionaryImpl::HasValue(const StringPiece value) const {
  for (size_t i = 0; i < dics_.size(); ++i) {
    if (dics_[i]->HasValue(value)) {
      return true;
    }
  }
  return false;
}

Node *DictionaryImpl::LookupPredictiveWithLimit(
    const char *str, int size,
    const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  return LookupInternal(str, size, PREDICTIVE, limit, allocator);
}

Node *DictionaryImpl::LookupPredictive(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  return LookupInternal(str, size, PREDICTIVE, Limit(), allocator);
}

namespace {

// This class wraps implementation of DictionaryInterface::Callback to provide
// it with the same token filtering rule as combination of
// MaybeRemoveSpecialNodes() and SuppressNodes().
class CallbackWithFilter : public DictionaryInterface::Callback {
 public:
  CallbackWithFilter(const bool use_spelling_correction,
                     const bool use_zip_code_conversion,
                     const bool use_t13n_conversion,
                     const POSMatcher *pos_matcher,
                     const SuppressionDictionary *suppression_dictionary,
                     DictionaryInterface::Callback *callback)
      : use_spelling_correction_(use_spelling_correction),
        use_zip_code_conversion_(use_zip_code_conversion),
        use_t13n_conversion_(use_t13n_conversion),
        pos_matcher_(pos_matcher),
        suppression_dictionary_(suppression_dictionary),
        callback_(callback) {}

  virtual ResultType OnKey(StringPiece key) {
    return callback_->OnKey(key);
  }

  virtual ResultType OnActualKey(StringPiece key, StringPiece actual_key,
                                 bool is_expanded) {
    return callback_->OnActualKey(key, actual_key, is_expanded);
  }

  virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                             const Token &token) {
    if (!(token.attributes & Token::USER_DICTIONARY)) {
      if (!use_spelling_correction_ &&
          (token.attributes & Token::SPELLING_CORRECTION)) {
        return TRAVERSE_CONTINUE;
      }
      if (!use_zip_code_conversion_ && pos_matcher_->IsZipcode(token.lid)) {
        return TRAVERSE_CONTINUE;
      }
      if (!use_t13n_conversion_ &&
          Util::IsEnglishTransliteration(token.value)) {
        return TRAVERSE_CONTINUE;
      }
    }
    if (suppression_dictionary_->SuppressEntry(token.key, token.value)) {
      return TRAVERSE_CONTINUE;
    }
    return callback_->OnToken(key, actual_key, token);
  }

 private:
  const bool use_spelling_correction_;
  const bool use_zip_code_conversion_;
  const bool use_t13n_conversion_;
  const POSMatcher *pos_matcher_;
  const SuppressionDictionary *suppression_dictionary_;
  DictionaryInterface::Callback *callback_;
};

}  // namespace

void DictionaryImpl::LookupPrefix(
    StringPiece key,
    bool use_kana_modifier_insensitive_lookup,
    Callback *callback) const {
  CallbackWithFilter callback_with_filter(
      GET_CONFIG(use_spelling_correction),
      GET_CONFIG(use_zip_code_conversion),
      GET_CONFIG(use_t13n_conversion),
      pos_matcher_,
      suppression_dictionary_,
      callback);
  for (size_t i = 0; i < dics_.size(); ++i) {
    dics_[i]->LookupPrefix(
        key, use_kana_modifier_insensitive_lookup, &callback_with_filter);
  }
}

void DictionaryImpl::LookupExact(StringPiece key, Callback *callback) const {
  CallbackWithFilter callback_with_filter(
      GET_CONFIG(use_spelling_correction),
      GET_CONFIG(use_zip_code_conversion),
      GET_CONFIG(use_t13n_conversion),
      pos_matcher_,
      suppression_dictionary_,
      callback);
  for (size_t i = 0; i < dics_.size(); ++i) {
    dics_[i]->LookupExact(key, &callback_with_filter);
  }
}

Node *DictionaryImpl::LookupReverse(const char *str, int size,
                                    NodeAllocatorInterface *allocator) const {
  return LookupInternal(str, size, REVERSE, Limit(), allocator);
}

bool DictionaryImpl::LookupComment(StringPiece key, StringPiece value,
                                   string *comment) const {
  // TODO(komatsu): UserDictionary should be treated as the highest priority.
  // In the current implementation, UserDictionary is the last node of dics_,
  // but the only dictionary which may return true.
  for (size_t i = 0; i < dics_.size(); ++i) {
    if (dics_[i]->LookupComment(key, value, comment)) {
      return true;
    }
  }
  return false;
}

bool DictionaryImpl::Reload() {
  return user_dictionary_->Reload();
}

void DictionaryImpl::PopulateReverseLookupCache(
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  for (size_t i = 0; i < dics_.size(); ++i) {
    dics_[i]->PopulateReverseLookupCache(str, size, allocator);
  }
}

void DictionaryImpl::ClearReverseLookupCache(
    NodeAllocatorInterface *allocator) const {
  for (size_t i = 0; i < dics_.size(); ++i) {
    dics_[i]->ClearReverseLookupCache(allocator);
  }
}

Node *DictionaryImpl::MaybeRemoveSpecialNodes(Node *node) const {
  const bool use_spelling_correction = GET_CONFIG(use_spelling_correction);
  const bool use_zip_code_conversion = GET_CONFIG(use_zip_code_conversion);
  const bool use_t13n_conversion = GET_CONFIG(use_t13n_conversion);

  if (use_spelling_correction && use_zip_code_conversion &&
      use_t13n_conversion) {
    return node;
  }

  Node *head = NULL;
  Node *prev = NULL;

  for (Node *n = node; n != NULL; n = n->bnext) {
    // Skip the filtering logic if the node is generated by user dictionary.
    if (!(n->attributes & Node::USER_DICTIONARY)) {
      if (!use_spelling_correction &&
          n->attributes & Node::SPELLING_CORRECTION) {
        continue;
      }

      if (!use_zip_code_conversion && pos_matcher_->IsZipcode(n->lid)) {
        continue;
      }

      if (!use_t13n_conversion && Util::IsEnglishTransliteration(n->value)) {
        continue;
      }
    }

    if (head == NULL) {
      head = n;
    }
    if (prev != NULL) {
      prev->bnext = n;
    }
    prev = n;
  }

  if (prev != NULL) {
    prev->bnext = NULL;
  }

  return head;
}

Node *DictionaryImpl::LookupInternal(const char *str, int size,
                                     LookupType type,
                                     const Limit &limit,
                                     NodeAllocatorInterface *allocator) const {
  Node *head = NULL;
  for (size_t i = 0; i < dics_.size(); ++i) {
    Node *nodes = NULL;
    switch (type) {
      case PREDICTIVE: {
        nodes = dics_[i]->LookupPredictiveWithLimit(
            str, size, limit, allocator);
        break;
      }
      case PREFIX: {
        LOG(FATAL) << "LookupInternal for PREFIX is deprecated.";
        break;
      }
      case REVERSE: {
        nodes = dics_[i]->LookupReverse(str, size, allocator);
        break;
      }
      case EXACT: {
        LOG(FATAL) << "LookupInternal for EXACT is deprecated.";
        break;
      }
    }
    if (head == NULL) {
      head = nodes;
    } else if (nodes != NULL) {
      Node *last = NULL;
      // TODO(taku)  this is O(n^2) algorithm. can be fixed.
      for (last = nodes; last->bnext != NULL; last = last->bnext) {}
      last->bnext = head;
      head = nodes;
    }
  }

  head = MaybeRemoveSpecialNodes(head);
  head = suppression_dictionary_->SuppressNodes(head);

  return head;
}

}  // namespace dictionary
}  // namespace mozc
