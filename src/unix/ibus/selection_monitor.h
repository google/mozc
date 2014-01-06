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

#ifndef MOZC_UNIX_IBUS_SELECTION_MONITOR_H_
#define MOZC_UNIX_IBUS_SELECTION_MONITOR_H_

#include <string>

#include "base/port.h"

namespace mozc {
namespace ibus {

struct SelectionInfo {
  uint64 timestamp;
  uint32 process_id;
  string machine_name;
  string window_title;
  string selected_text;
  SelectionInfo();
};

class SelectionMonitorInterface {
 public:
  virtual ~SelectionMonitorInterface();
  virtual SelectionInfo GetSelectionInfo() ABSTRACT;
  virtual void StartMonitoring() ABSTRACT;
  virtual void QueryQuit() ABSTRACT;
};

class SelectionMonitorFactory {
 public:
  // Returns an instance of SelectionMonitorInterface implementation.
  // Caller must take the ownership of returned object.
  // |max_text_bytes| represents the maximum string size in bytes which
  // limits each string field in SelectionInfo structure.
  static SelectionMonitorInterface *Create(size_t max_text_bytes);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SelectionMonitorFactory);
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_SELECTION_MONITOR_H_
