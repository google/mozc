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

#ifndef MOZC_STORAGE_ENCRYPTED_STRING_STORAGE_H_
#define MOZC_STORAGE_ENCRYPTED_STRING_STORAGE_H_

#include <string>

#include "absl/strings/string_view.h"

namespace mozc {
namespace storage {

class StringStorageInterface {
 public:
  virtual ~StringStorageInterface() = default;

  virtual bool Load(std::string *output) const = 0;
  virtual bool Save(const std::string &input) const = 0;
};

class EncryptedStringStorage : public StringStorageInterface {
 public:
  explicit EncryptedStringStorage(const absl::string_view filename)
      : filename_(filename) {}
  EncryptedStringStorage(const EncryptedStringStorage &) = delete;
  EncryptedStringStorage &operator=(const EncryptedStringStorage &) = delete;

  bool Load(std::string *output) const override;
  bool Save(const std::string &input) const override;

  const std::string &filename() const { return filename_; }

 protected:
  virtual bool Encrypt(const std::string &salt, std::string *data) const;
  virtual bool Decrypt(const std::string &salt, std::string *data) const;

 private:
  std::string filename_;
};

}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_ENCRYPTED_STRING_STORAGE_H_
