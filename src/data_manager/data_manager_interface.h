// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_DATA_MANAGER_DATA_MANAGER_INTERFACE_H_
#define MOZC_DATA_MANAGER_DATA_MANAGER_INTERFACE_H_

#include "base/port.h"

namespace mozc {

class ConnectorInterface;
class DictionaryInterface;
class POSMatcher;
class PosGroup;
class SegmenterInterface;
class UserDictionary;
class UserPOSInterface;

// Builds those objects that depend on a set of embedded data generated from
// files in data/dictionary, such as dictionary.txt, id.def, etc.
class DataManagerInterface {
 public:
  virtual ~DataManagerInterface() {}

  // Returns a reference to the UserPOS class handling user pos data.  Don't
  // delete the returned pointer, which is owned by the manager.
  virtual const UserPOSInterface *GetUserPOS() const = 0;
  virtual UserDictionary *GetUserDictionary() = 0;

  // Since some modules don't require possibly huge dictionary data, we allow
  // derived classes not to implement the following methods.

  // Returns a reference to POSMatcher class handling POS rules. Don't
  // delete the returned pointer, which is owned by the manager.
  virtual const POSMatcher *GetPOSMatcher() { return NULL; }

  // Returns a reference to PosGroup class handling POS grouping rule. Don't
  // delete the returned pointer, which is owned by the manager.
  virtual const PosGroup *GetPosGroup() { return NULL; }

  // Returns a reference to Connector class handling connection data.  Don't
  // delete the returned pointer, which is owned by the manager.
  virtual const ConnectorInterface *GetConnector() { return NULL; }

  // Returns a reference to Segmenter class handling segmentation data.  Don't
  // delete the returned pointer, which is owned by the manager.
  virtual const SegmenterInterface *GetSegmenter() { return NULL; }

  // Creates a system dictionary. The caller is responsible for deleting the
  // returned object.
  virtual DictionaryInterface *CreateSystemDictionary() { return NULL; }

  // Creates a value dictionary. The caller is responsible for deleting the
  // returned object.
  virtual DictionaryInterface *CreateValueDictionary() { return NULL; }

  // Returns a reference to the suffix dictionary.
  virtual DictionaryInterface *GetSuffixDictionary() { return NULL; }

 protected:
  DataManagerInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DataManagerInterface);
};

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_DATA_MANAGER_INTERFACE_H_
