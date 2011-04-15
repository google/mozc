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

// Here is a dummy implementation of the default converter
// (implementing mozc::ConverterInterface).  We don't reuse the
// default converter for Mozc because it also brings Japanese language
// model code and Japanese dictionary, which we don't want with
// chewing Chinese input.

#include "converter/converter_interface.h"

#include <string>
#include "base/base.h"
#include "base/singleton.h"

namespace mozc {
namespace chewing {
// This converter implementation does nothing.  Everything for
// converter is actually done inside of libchewing.
class DummyConverter : public mozc::ConverterInterface {
 public:
  DummyConverter() {}

  virtual bool StartConversion(Segments *segments,
                               const string &key) const {
    return true;
  }

  virtual bool StartConversionWithComposer(
      Segments *segments, const composer::Composer *composer) const {
    return true;
  }

  virtual bool StartReverseConversion(Segments *segments,
                                      const string &key) const {
    return true;
  }

  virtual bool StartPrediction(Segments *segments,
                               const string &key) const {
    return true;
  }

  virtual bool StartSuggestion(Segments *segments,
                               const string &key) const {
    return true;
  }

  virtual bool FinishConversion(Segments *segments) const {
    return true;
  }

  virtual bool CancelConversion(Segments *segments) const {
    return true;
  }

  virtual bool ResetConversion(Segments *segments) const {
    return true;
  }

  virtual bool RevertConversion(Segments *segments) const {
    return true;
  }

  virtual bool GetCandidates(Segments *segments,
                             size_t segment_index,
                             size_t candidate_size) const {
    return true;
  }

  virtual bool CommitSegmentValue(Segments *segments,
                                  size_t segment_index,
                                  int    candidate_index) const {
    return true;
  }

  virtual bool FocusSegmentValue(Segments *segments,
                                 size_t segment_index,
                                 int candidate_index) const {
    return true;
  }

  virtual bool FreeSegmentValue(Segments *segments,
                                size_t segment_index) const {
    return true;
  }

  virtual bool SubmitFirstSegment(Segments *segments,
                                  size_t candidate_index) const {
    return true;
  }

  virtual bool ResizeSegment(Segments *segments,
                             size_t segment_index,
                             int offset_length) const {
    return true;
  }

  virtual bool ResizeSegment(Segments *segments,
                             size_t start_segment_index,
                             size_t segments_size,
                             const uint8 *new_size_array,
                             size_t array_size) const {
    return true;
  }

  virtual bool Sync() const {
    return true;
  }

  virtual bool Reload() const {
    return true;
  }

  virtual bool ClearUserHistory() const {
    return true;
  }

  virtual bool ClearUserPrediction() const {
    return true;
  }

  virtual bool ClearUnusedUserPrediction() const {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyConverter);
};
}  // namespace chewing

namespace {
ConverterInterface *g_converter = NULL;
}  // anonymous namespace

// Here intercepts the default implementation of converter factory to
// prevent linking to the Japanese code.
ConverterInterface *ConverterFactory::GetConverter() {
  if (g_converter == NULL) {
    g_converter = Singleton<mozc::chewing::DummyConverter>::get();
  }
  return g_converter;
}

void ConverterFactory::SetConverter(ConverterInterface *converter) {
  g_converter = converter;
}
}  // namespace mozc
