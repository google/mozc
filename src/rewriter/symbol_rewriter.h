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

#ifndef MOZC_REWRITER_SYMBOL_REWRITER_H_
#define MOZC_REWRITER_SYMBOL_REWRITER_H_

#include <string>
#include "rewriter/embedded_dictionary.h"
#include "rewriter/rewriter_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class Segment;
class Segments;

class SymbolRewriter: public RewriterInterface  {
 public:
  SymbolRewriter();
  virtual ~SymbolRewriter();
  virtual bool Rewrite(Segments *segments) const;

private:
  FRIEND_TEST(SymbolRewriterTest, TriggerRewriteEntireTest);
  FRIEND_TEST(SymbolRewriterTest, TriggerRewriteEachTest);
  FRIEND_TEST(SymbolRewriterTest, SplitDescriptionTest);

  // Some characters may have different description for full/half width forms.
  // Here we just change the description in this function.
  static const string GetDescription(const string &value,
                                     const char *description,
                                     const char *additional_description);

  // return true key has no-hiragana
  static bool IsSymbol(const string &key);

  // Insert alternative form of space.
  static void ExpandSpace(Segment *segment);

  // Returns true if the symbol is platform dependent
  static bool IsPlatformDependent(const EmbeddedDictionary::Value &value);

  // Return true if two symbols are in same group.
  static bool InSameSymbolGroup(const EmbeddedDictionary::Value &lhs,
                                const EmbeddedDictionary::Value &rhs);

  // Insert Symbol into segment.
  static void InsertCandidates(const EmbeddedDictionary::Value *value,
                               size_t size,
                               bool context_sensitive,
                               Segment *segment);

  // Insert symbols using connected all segments.
  static bool RewriteEntireCandidate(Segments *segments);

  // Insert symbols using single segment.
  static bool RewriteEachCandidate(Segments *segments);
};
}  // namespace mozc

#endif  // MOZC_REWRITER_SYMBOL_REWRITER_H_
