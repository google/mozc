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

#ifndef MOZC_DICTIONARY_DICTIONARY_IMPL_H_
#define MOZC_DICTIONARY_DICTIONARY_IMPL_H_

#include <vector>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "dictionary/dictionary_interface.h"

namespace mozc {

class POSMatcher;
class SuppressionDictionary;

namespace dictionary {

class DictionaryImpl : public DictionaryInterface {
 public:
  // Initializes a dictionary with given dictionaries and POS data.  The system
  // and value dictionaries are owned by this class but the user dictionary is
  // just a reference and to be deleted by the caller. Note that the user
  // dictionary is not a const reference because this class may reload the user
  // dictionary.
  // TODO(noriyukit): Currently DictionaryInterface::Reload() is not used and
  // thus user_dictionary can be const as well. We can make it const after
  // clarifying the ownership of the user dictionary and changing code so that
  // the owner reloads it.
  DictionaryImpl(const DictionaryInterface *system_dictionary,
                 const DictionaryInterface *value_dictionary,
                 DictionaryInterface *user_dictionary,
                 const SuppressionDictionary *suppression_dictionary,
                 const POSMatcher *pos_matcher);

  virtual ~DictionaryImpl();

  virtual bool HasValue(const StringPiece value) const;

  virtual Node *LookupPredictiveWithLimit(
      const char *str, int size, const Limit &limit,
      NodeAllocatorInterface *allocator) const;

  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const;

  virtual Node *LookupPrefixWithLimit(
      const char *str, int size,
      const Limit &limit,
      NodeAllocatorInterface *allocator) const;

  virtual Node *LookupPrefix(const char *str, int size,
                             NodeAllocatorInterface *allocator) const;

  virtual Node *LookupExact(const char *str, int size,
                            NodeAllocatorInterface *allocator) const;

  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const;

  virtual bool Reload();

  virtual void PopulateReverseLookupCache(
      const char *str, int size, NodeAllocatorInterface *allocator) const;

  virtual void ClearReverseLookupCache(NodeAllocatorInterface *allocator) const;

 private:
  enum LookupType {
    PREDICTIVE,
    PREFIX,
    REVERSE,
    EXACT,
  };

  Node *LookupInternal(const char *str, int size,
                       LookupType type,
                       const Limit &limit,
                       NodeAllocatorInterface *allocator) const;

  Node *MaybeRemoveSpecialNodes(Node *node) const;

  // Used to check POS IDs.
  const POSMatcher *pos_matcher_;

  // Main three dictionaries.
  scoped_ptr<const DictionaryInterface> system_dictionary_;
  scoped_ptr<const DictionaryInterface> value_dictionary_;
  DictionaryInterface *user_dictionary_;

  // Convenient container to handle the above three dictionaries as one
  // composite dictionary.
  vector<const DictionaryInterface *> dics_;

  // Suppression dictionary is used to suppress nodes.
  const SuppressionDictionary *suppression_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryImpl);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_IMPL_H_
