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

#ifndef MOZC_REWRITER_REWRITER_INTERFACE_H_
#define MOZC_REWRITER_REWRITER_INTERFACE_H_

#include <array>
#include <cstddef>  // for size_t
#include <cstdint>
#include <optional>

#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {

class RewriterInterface {
 public:
  virtual ~RewriterInterface() = default;

  enum CapabilityType {
    NOT_AVAILABLE = 0,
    CONVERSION = 1,
    PREDICTION = 2,
    SUGGESTION = 4,
    ALL = (1 | 2 | 4),
  };

  // Returns capability of this rewriter.
  // If (capability() & CONVERSION), this rewriter
  // is called after StartConversion().
  virtual int capability(const ConversionRequest &request) const {
    return CONVERSION;
  }

  struct ResizeSegmentsRequest {
    // Position of the segment to be resized.
    size_t segment_index = 0;

    // The new size of each segment in Unicode character (e.g. 3 for "あいう").
    // The type and size (i.e. uint8_t and 8) comes from the implementation of
    // UserBoundaryHistoryRewriter that stores the segment sizes in this format.
    using SegmentSizes = std::array<uint8_t, 8>;
    SegmentSizes segment_sizes = {0, 0, 0, 0, 0, 0, 0, 0};
  };

  virtual std::optional<ResizeSegmentsRequest> CheckResizeSegmentsRequest(
      const ConversionRequest &request, const Segments &segments) const {
    return std::nullopt;
  }

  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const = 0;

  // This method is mainly called when user puts SPACE key
  // and changes the focused candidate.
  // In this method, Converter will find bracketing matching.
  // e.g., when user selects "「",  corresponding closing bracket "」"
  // is chosen in the preedit.
  virtual bool Focus(Segments *segments, size_t segment_index,
                     int candidate_index) const {
    return true;
  }

  // Hook(s) for all mutable operations
  virtual void Finish(const ConversionRequest &request,
                      const Segments &segments) {}

  // Reverts the last Finish operation.
  virtual void Revert(const Segments &segments) {}

  // Delete the user history based entry corresponding to the specified
  // candidate.
  // Returns true when at least one deletion operation succeeded
  // |segment_index| is the index for all segments, not the index of
  // conversion_segments.
  virtual bool ClearHistoryEntry(const Segments &segments, size_t segment_index,
                                 int candidate_index) {
    return false;
  }

  // Synchronizes internal data to local file system.  This method is called
  // when the server received SYNC_DATA command from the client.  Currently,
  // this event happens, e.g., when user moves to another text area.
  virtual bool Sync() { return true; }

  // Reloads internal data from local file system.  This method is called when
  // the server received RELOAD command from the client.  Currently, this event
  // happens when user edited the user dictionary by dictionary tool.
  virtual bool Reload() { return true; }

  // Clears internal data on local storage.  This method is called when the
  // server received CLEAR_USER_HISTORY command from the client.  Currently,
  // this event happens when user explicitly requested "clear user history"
  // on settings UI.
  virtual void Clear() {}

 protected:
  RewriterInterface() = default;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_REWRITER_INTERFACE_H_
