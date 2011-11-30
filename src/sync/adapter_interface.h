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

#ifndef MOZC_SYNC_ADAPTER_INTERFACE_H_
#define MOZC_SYNC_ADAPTER_INTERFACE_H_


namespace google {
namespace protobuf {
template <class T> class RepeatedPtrField;
}  // namespace protobuf
}  // namespace google

namespace ime_sync {
typedef google::protobuf::RepeatedPtrField<ime_sync::SyncItem> SyncItems;
}  // namespace ime_sync


namespace mozc {
namespace sync {

class AdapterInterface {
 public:
  virtual ~AdapterInterface() {}

  // Note that Start is called in the main converter thread.
  // We can implement a logic inside these methods, like preparing
  // sync items.
  virtual bool Start() {
    return true;
  }

  // SetDownloadedItems, GetItemsToUpload and MarkUploaded are called
  // outside of main converter thread.
  virtual bool SetDownloadedItems(const ime_sync::SyncItems &items) = 0;
  virtual bool GetItemsToUpload(ime_sync::SyncItems *items) = 0;
  virtual bool MarkUploaded(
      const ime_sync::SyncItem &item, bool uploaded) = 0;

  // Clear is called after Syncer::Clear finishes.
  // internal status, like timestamp, last_synced_file can be
  // deleted in this method.
  virtual bool Clear() = 0;

  virtual ime_sync::Component component_id() const = 0;
};
}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_ADAPTER_INTERFACE_H_
