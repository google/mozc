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

#include "gui/base/msime_user_dictionary_importer.h"

#include "dictionary/user_dictionary_importer.h"

#ifdef _WIN32
// clang-format off
#include <windows.h>  // needs to be included first
// clang-format on
#include <msime.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "base/vlog.h"
#include "base/win32/wide_char.h"
#include "gui/base/encoding_util.h"

namespace mozc {
namespace {

constexpr size_t kBufferSize = 256;

// ProgID of MS-IME Japanese.
constexpr wchar_t kVersionIndependentProgIdForMSIME[] = L"MSIME.Japan";

// Interface identifier of user dictionary in MS-IME.
// {019F7153-E6DB-11d0-83C3-00C04FDDB82E}
constexpr GUID kIidIFEDictionary = {
    0x19f7153, 0xe6db, 0x11d0, {0x83, 0xc3, 0x0, 0xc0, 0x4f, 0xdd, 0xb8, 0x2e}};

IFEDictionary* CreateIFEDictionary() {
  CLSID class_id = GUID_NULL;
  // On Windows 7 and prior, multiple versions of MS-IME can be installed
  // side-by-side. As far as we've observed, the latest version will be chosen
  // with version-independent ProgId.
  HRESULT result =
      ::CLSIDFromProgID(kVersionIndependentProgIdForMSIME, &class_id);
  if (FAILED(result)) {
    LOG(ERROR) << "CLSIDFromProgID() failed: " << result;
    return nullptr;
  }
  IFEDictionary* obj = nullptr;
  result =
      ::CoCreateInstance(class_id, nullptr, CLSCTX_INPROC_SERVER,
                         kIidIFEDictionary, reinterpret_cast<void**>(&obj));
  if (FAILED(result)) {
    LOG(ERROR) << "CoCreateInstance() failed: " << result;
    return nullptr;
  }
  MOZC_VLOG(1) << "Can create IFEDictionary successfully";
  return obj;
}

class ScopedIFEDictionary {
 public:
  explicit ScopedIFEDictionary(IFEDictionary* dic) : dic_(dic) {}

  ~ScopedIFEDictionary() {
    if (dic_ != nullptr) {
      dic_->Close();
      dic_->Release();
    }
  }

  IFEDictionary& operator*() const { return *dic_; }
  IFEDictionary* operator->() const { return dic_; }
  IFEDictionary* get() const { return dic_; }

 private:
  IFEDictionary* dic_;
};

// Iterator for MS-IME user dictionary
class MSIMEImportIterator : public user_dictionary::InputIteratorInterface {
 public:
  MSIMEImportIterator()
      : buf_(kBufferSize),
        dic_(CreateIFEDictionary()),
        result_(E_FAIL),
        size_(0),
        index_(0) {
    if (dic_.get() == nullptr) {
      LOG(ERROR) << "IFEDictionaryFactory returned nullptr";
      return;
    }

    // open user dictionary
    HRESULT result = dic_->Open(nullptr, nullptr);
    if (S_OK != result) {
      LOG(ERROR) << "Cannot open user dictionary: " << result_;
      return;
    }

    POSTBL* pos_table = nullptr;
    int pos_size = 0;
    result_ = dic_->GetPosTable(&pos_table, &pos_size);
    if (S_OK != result_ || pos_table == nullptr || pos_size == 0) {
      LOG(ERROR) << "Cannot get POS table: " << result;
      result_ = E_FAIL;
      return;
    }

    for (int i = 0; i < pos_size; ++i) {
      pos_map_.try_emplace(
          pos_table->nPos,
          EncodingUtil::SjisToUtf8(reinterpret_cast<char*>(pos_table->szName)));
      ++pos_table;
    }

    // extract all words registered by user.
    // Don't use auto-registered words, since Mozc may not be able to
    // handle auto_registered words correctly, and user is basically
    // unaware of auto-registered words.
    result_ =
        dic_->GetWords(nullptr, nullptr, nullptr, IFED_POS_ALL, IFED_SELECT_ALL,
                       IFED_REG_USER,  // | FED_REG_AUTO
                       reinterpret_cast<UCHAR*>(&buf_[0]),
                       kBufferSize * sizeof(IMEWRD), &size_);
  }

  bool IsAvailable() const {
    return result_ == IFED_S_MORE_ENTRIES || result_ == S_OK;
  }

  bool Next(user_dictionary::RawEntry* entry) {
    if (!IsAvailable()) {
      LOG(ERROR) << "Iterator is not available";
      return false;
    }

    if (entry == nullptr) {
      LOG(ERROR) << "Entry is nullptr";
      return false;
    }
    entry->Clear();

    if (index_ < size_) {
      if (buf_[index_].pwchReading == nullptr ||
          buf_[index_].pwchDisplay == nullptr) {
        ++index_;
        LOG(ERROR) << "pwchDisplay or pwchReading is nullptr";
        return true;
      }

      // set key/value
      entry->key = mozc::win32::WideToUtf8(buf_[index_].pwchReading);
      entry->value = mozc::win32::WideToUtf8(buf_[index_].pwchDisplay);

      // set POS
      std::map<int, std::string>::const_iterator it =
          pos_map_.find(buf_[index_].nPos1);
      if (it == pos_map_.end()) {
        ++index_;
        LOG(ERROR) << "Unknown POS id: " << buf_[index_].nPos1;
        entry->Clear();
        return true;
      }
      entry->pos = it->second;

      // set comment
      if (buf_[index_].pvComment != nullptr) {
        if (buf_[index_].uct == IFED_UCT_STRING_SJIS) {
          entry->comment = EncodingUtil::SjisToUtf8(
              reinterpret_cast<const char*>(buf_[index_].pvComment));
        } else if (buf_[index_].uct == IFED_UCT_STRING_UNICODE) {
          entry->comment = mozc::win32::WideToUtf8(
              reinterpret_cast<const wchar_t*>(buf_[index_].pvComment));
        }
      }
    }

    if (index_ < size_) {
      ++index_;
      return true;
    } else if (result_ == S_OK) {
      return false;
    } else if (result_ == IFED_S_MORE_ENTRIES) {
      result_ = dic_->NextWords(reinterpret_cast<UCHAR*>(&buf_[0]),
                                kBufferSize * sizeof(IMEWRD), &size_);
      if (result_ == E_FAIL) {
        LOG(ERROR) << "NextWords() failed";
        return false;
      }
      index_ = 0;
      return true;
    }

    return false;
  }

 private:
  std::vector<IMEWRD> buf_;
  ScopedIFEDictionary dic_;
  std::map<int, std::string> pos_map_;
  HRESULT result_;
  ULONG size_;
  ULONG index_;
};

}  // namespace

namespace gui {

user_dictionary::InputIteratorInterface* MSIMEUserDictionarImporter::Create() {
  return new MSIMEImportIterator;
}

}  // namespace gui
}  // namespace mozc

#else  // _WIN32

namespace mozc {
namespace gui {

user_dictionary::InputIteratorInterface* MSIMEUserDictionarImporter::Create() {
  return nullptr;
}

}  // namespace gui
}  // namespace mozc

#endif  // _WIN32
