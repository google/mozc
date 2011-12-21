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

#include "dictionary/user_dictionary_importer.h"

#ifdef OS_WINDOWS
#include <windows.h>
#ifdef HAS_MSIME_HEADER
#indlude <msime.h>
#endif  // HAS_MSIME_HEADER
#endif  // OS_WINDOWS

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/mmap.h"
#include "base/singleton.h"
#include "base/util.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"

namespace mozc {
namespace {
uint64 EntryFingerprint(
    const UserDictionaryStorage::UserDictionaryEntry &entry) {
  return Util::Fingerprint(entry.key() + "\t" +
                           entry.value() + "\t" +
                           entry.pos());
}

void NormalizePOS(const string &input, string *output) {
  string tmp;
  output->clear();
  Util::FullWidthAsciiToHalfWidthAscii(input, &tmp);
  Util::HalfWidthKatakanaToFullWidthKatakana(tmp, output);
}

// A data type to hold conversion rules of POSes and stemming
// suffixes. If mozc_pos is set to be an empty string (""), it means
// that words of the POS should be ignored in Mozc.
struct POSMap {
  const char *source_pos;   // POS string of a third party IME.
  const char *mozc_pos;     // POS string of Mozc.
  const char *suffix;       // Stemming suffix that should be appended
  // when converting a dictionary entry to
  // Mozc style.
};

// Include actual POS mapping rules defined outside the file.
#include "dictionary/pos_map.h"

// A functor for searching an array of POSMap for the given POS. The
// class is used with std::lower_bound().
class POSMapCompare {
 public:
  bool operator() (const POSMap &l_pos_map, const POSMap &r_pos_map) const {
    return (strcmp(l_pos_map.source_pos, r_pos_map.source_pos) < 0);
  }
};

// Converts POS of a third party IME to that of Mozc using the given
// mapping.
bool ConvertEntryInternal(
    const POSMap *pos_map, size_t map_size,
    const UserDictionaryStorage::UserDictionaryEntry &from,
    UserDictionaryStorage::UserDictionaryEntry *to) {
  if (to == NULL) {
    LOG(ERROR) << "Null pointer is passed.";
    return false;
  }

  to->Clear();

  if (from.pos().empty()) {
    return false;
  }

  // Normalize POS (remove full width ascii and half width katakana)
  string pos;
  NormalizePOS(from.pos(), &pos);

  // ATOK's POS has a special marker for distinguishing
  // auto-registered words/manually-registered words.
  // remove the mark here.
  if (!pos.empty() &&
      (pos[pos.size() - 1] == '$' ||
       pos[pos.size() - 1] == '*')) {
    pos.resize(pos.size() - 1);
  }

  POSMap key;
  key.source_pos = pos.c_str();
  key.mozc_pos = NULL;
  key.suffix = NULL;

  // Search for mapping for the given POS.
  const POSMap *found = lower_bound(pos_map, pos_map + map_size,
                                    key, POSMapCompare());
  if (found == pos_map + map_size ||
      strcmp(found->source_pos, key.source_pos) != 0) {
    LOG(WARNING) << "Invalid POS is passed: " << from.pos();
    return false;
  }

  // Enpty Mozc POS means that words of the POS should be ignored
  // in Mozc. Set all arguments to an empty string.
  // const POSMap &map_rule = pos_map[index];
  if (found->mozc_pos == NULL) {
    to->clear_key();
    to->clear_value();
    to->clear_pos();
    return false;
  }

  to->set_key(from.key());
  to->set_value(from.value());
  to->set_pos(found->mozc_pos);

  // normalize reading
  string normalized_key;
  UserDictionaryUtil::NormalizeReading(to->key(), &normalized_key);
  to->set_key(normalized_key);

  // copy comment
  if (from.has_comment()) {
    to->set_comment(from.comment());
  }

  // validation
  if (!UserDictionaryUtil::IsValidEntry(*to)) {
    return false;
  }

  return true;
}
}  // namespace

#if defined(OS_WINDOWS) && defined(HAS_MSIME_HEADER)
namespace {
typedef BOOL (WINAPI *FPCreateIFEDictionaryInstance)(VOID **);

const size_t kBufferSize = 256;

class IFEDictionaryFactory {
 public:
  IFEDictionaryFactory()
      : lib_(NULL), create_ifedictionary_instance_(NULL) {
    const wchar_t *kIMEJPLibs[] =
        { L"imjp14k.dll",  // Office 14 / 2010
          L"imjp12k.dll",  // Office 12 / 2007
          L"imjp10k.dll",  // Windows NT 6.0, 6.1
          L"imjp9k.dll",   // Office 11 / 2003
          // The bottom-of-the-line of our targets is Windows XP
          // so we should stop looking up IMEs at "imjp81k.dll"
          // http://b/2440318
          L"imjp81k.dll"   // Windows NT 5.1, 5.2
          // L"imjp8k.dll",   // Office 10 / XP / 2002
        };

    // check imjp dll from newer ones.
    for (size_t i = 0; i < arraysize(kIMEJPLibs); ++i) {
      lib_ = Util::LoadSystemLibrary(kIMEJPLibs[i]);
      if (NULL != lib_) {
        break;
      }
    }

    if (NULL == lib_) {
      LOG(ERROR) << "LoadSystemLibrary failed";
      return;
    }

    create_ifedictionary_instance_ =
        reinterpret_cast<FPCreateIFEDictionaryInstance>
        (::GetProcAddress(lib_, "CreateIFEDictionaryInstance"));

    if (NULL == create_ifedictionary_instance_) {
      LOG(ERROR) << "GetProcAddress failed";
      return;
    }
  }

  IFEDictionary *Create() {
    if (create_ifedictionary_instance_ == NULL) {
      LOG(ERROR) << "CreateIFEDictionaryInstance is NULL";
      return NULL;
    }

    IFEDictionary *dic = NULL;
    const HRESULT result = (*create_ifedictionary_instance_)(
        reinterpret_cast<LPVOID *>(&dic));

    if (S_OK != result) {
      LOG(ERROR) << "CreateIFEDictionaryInstance() failed: " << result;
      return NULL;
    }

    VLOG(1) << "Can create IFEDictionary successfully";

    return dic;
  }

 private:
  HMODULE lib_;
  FPCreateIFEDictionaryInstance create_ifedictionary_instance_;
};

class ScopedIFEDictionary {
 public:
  explicit ScopedIFEDictionary(IFEDictionary *dic)
      : dic_(dic) {}

  ~ScopedIFEDictionary() {
    if (dic_ != NULL) {
      dic_->Close();
      dic_->Release();
    }
  }

  IFEDictionary & operator*() const { return *dic_; }
  IFEDictionary* operator->() const { return dic_; }
  IFEDictionary* get() const { return dic_; }

 private:
  IFEDictionary *dic_;
};

// Iterator for MS-IME user dictionary
class MSIMEImportIterator
    : public UserDictionaryImporter::InputIteratorInterface {
 public:
  MSIMEImportIterator()
      : dic_(Singleton<IFEDictionaryFactory>::get()->Create()),
        buf_(kBufferSize), result_(E_FAIL), size_(0), index_(0) {
    if (dic_.get() == NULL) {
      LOG(ERROR) << "IFEDictionaryFactory returned NULL";
      return;
    }

    // open user dictionary
    HRESULT result = dic_->Open(NULL, NULL);
    if (S_OK != result) {
      LOG(ERROR) << "Cannot open user dictionary: " << result_;
      return;
    }

    POSTBL *pos_table = NULL;
    int pos_size = 0;
    result_ = dic_->GetPosTable(&pos_table, &pos_size);
    if (S_OK != result_ || pos_table == NULL || pos_size == 0) {
      LOG(ERROR) << "Cannot get POS table: " << result;
      result_ = E_FAIL;
      return;
    }

    string name;
    for (int i = 0; i < pos_size; ++i) {
      Util::SJISToUTF8(reinterpret_cast<char *>(pos_table->szName), &name);
      pos_map_.insert(make_pair(pos_table->nPos, name));
      ++pos_table;
    }

    // extract all words registered by user.
    // Don't use auto-registered words, since Mozc may not be able to
    // handle auto_registered words correctly, and user is basically
    // unaware of auto-registered words.
    result_ = dic_->GetWords(NULL, NULL, NULL,
                             IFED_POS_ALL,
                             IFED_SELECT_ALL,
                             IFED_REG_USER,  // | FED_REG_AUTO
                             reinterpret_cast<UCHAR *>(&buf_[0]),
                             kBufferSize * sizeof(IMEWRD),
                             &size_);
  }

  bool IsAvailable() const {
    return result_ == IFED_S_MORE_ENTRIES || result_ == S_OK;
  }

  bool Next(UserDictionaryStorage::UserDictionaryEntry *entry) {
    if (!IsAvailable()) {
      LOG(ERROR) << "Iterator is not available";
      return false;
    }

    if (entry == NULL) {
      LOG(ERROR) << "Entry is NULL";
      return false;
    }
    entry->Clear();

    if (index_ < size_) {
      if (buf_[index_].pwchReading == NULL ||
          buf_[index_].pwchDisplay == NULL) {
        ++index_;
        LOG(ERROR) << "pwchDisplay or pwchReading is NULL";
        return true;
      }

      // set key/value
      Util::WideToUTF8(buf_[index_].pwchReading, entry->mutable_key());
      Util::WideToUTF8(buf_[index_].pwchDisplay, entry->mutable_value());

      // set POS
      map<int, string>::const_iterator it = pos_map_.find(buf_[index_].nPos1);
      if (it == pos_map_.end()) {
        ++index_;
        LOG(ERROR) << "Unknown POS id: " << buf_[index_].nPos1;
        entry->Clear();
        return true;
      }
      entry->set_pos(it->second);

      // set comment
      if (buf_[index_].pvComment != NULL) {
        if (buf_[index_].uct == IFED_UCT_STRING_SJIS) {
          Util::SJISToUTF8(
              reinterpret_cast<const char *>(buf_[index_].pvComment),
              entry->mutable_comment());
        } else if (buf_[index_].uct == IFED_UCT_STRING_UNICODE) {
          Util::WideToUTF8(
              reinterpret_cast<const wchar_t *>(buf_[index_].pvComment),
              entry->mutable_comment());
        }
      }
    }

    if (index_ < size_) {
      ++index_;
      return true;
    } else if (result_ == S_OK) {
      return false;
    } else if (result_ == IFED_S_MORE_ENTRIES) {
      result_ = dic_->NextWords(reinterpret_cast<UCHAR *>(&buf_[0]),
                                kBufferSize * sizeof(IMEWRD),
                               &size_);
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
  vector<IMEWRD> buf_;
  ScopedIFEDictionary dic_;
  map<int, string> pos_map_;
  HRESULT result_;
  ULONG size_;
  ULONG index_;
};
}  // namespace
#endif  // OS_WINDOWS && HAS_MSIME_HEADER

UserDictionaryImporter::ErrorType UserDictionaryImporter::ImportFromMSIME(
    UserDictionaryStorage::UserDictionary *user_dic) {
  DCHECK(user_dic);
#if defined(OS_WINDOWS) && defined(HAS_MSIME_HEADER)
  MSIMEImportIterator iter;
  return ImportFromIterator(&iter, user_dic);
#endif  // OS_WINDOWS && HAS_MSIME_HEADER
  return UserDictionaryImporter::IMPORT_NOT_SUPPORTED;
}

UserDictionaryImporter::ErrorType
UserDictionaryImporter::ImportFromIterator(
    UserDictionaryImporter::InputIteratorInterface *iter,
    UserDictionaryStorage::UserDictionary *user_dic) {
  if (iter == NULL || user_dic == NULL) {
    LOG(ERROR) << "iter or user_dic is NULL";
    return UserDictionaryImporter::IMPORT_FATAL;
  }

  const int max_size =
      user_dic->syncable() ?
          static_cast<int>(UserDictionaryStorage::max_sync_entry_size()) :
          static_cast<int>(UserDictionaryStorage::max_entry_size());

  UserDictionaryImporter::ErrorType ret =
      UserDictionaryImporter::IMPORT_NO_ERROR;

  set<uint64> dup_set;
  for (size_t i = 0; i < user_dic->entries_size(); ++i) {
    dup_set.insert(EntryFingerprint(user_dic->entries(i)));
  }

  UserDictionaryStorage::UserDictionaryEntry entry, tmp_entry;
  while (iter->Next(&entry)) {
    if (user_dic->entries_size() >= max_size) {
      LOG(WARNING) << "Too many words in one dictionary";
      return UserDictionaryImporter::IMPORT_TOO_MANY_WORDS;
    }

    if (entry.key().empty() &&
        entry.value().empty() &&
        entry.comment().empty()) {
      // Empty entry is just skipped. It could be annoying
      // if we show an warning dialog when these empty candidates exist.
      continue;
    }

    if (!UserDictionaryImporter::ConvertEntry(entry, &tmp_entry)) {
      LOG(WARNING) << "Entry is not valid";
      ret = UserDictionaryImporter::IMPORT_INVALID_ENTRIES;
      continue;
    }

    //  don't register words if it is aleady in the current dictionary
    if (!dup_set.insert(EntryFingerprint(tmp_entry)).second) {
      continue;
    }

    UserDictionaryStorage::UserDictionaryEntry *new_entry
        = user_dic->add_entries();
    DCHECK(new_entry);
    new_entry->CopyFrom(tmp_entry);
  }

  return ret;
}

UserDictionaryImporter::ErrorType
UserDictionaryImporter::ImportFromTextLineIterator(
    UserDictionaryImporter::IMEType ime_type,
    UserDictionaryImporter::TextLineIteratorInterface *iter,
    UserDictionaryStorage::UserDictionary *user_dic) {
  TextInputIterator text_iter(ime_type, iter);
  if (text_iter.ime_type() == UserDictionaryImporter::NUM_IMES) {
    return UserDictionaryImporter::IMPORT_NOT_SUPPORTED;
  }

  return UserDictionaryImporter::ImportFromIterator(&text_iter, user_dic);
}

UserDictionaryImporter::IStreamTextLineIterator::IStreamTextLineIterator(
    istream *is) : is_(is) {}

UserDictionaryImporter::IStreamTextLineIterator::~IStreamTextLineIterator() {}

bool UserDictionaryImporter::IStreamTextLineIterator::IsAvailable() const {
  return *is_;
}

bool UserDictionaryImporter::IStreamTextLineIterator::Next(string *line) {
  return getline(*is_, *line);
}

void UserDictionaryImporter::IStreamTextLineIterator::Reset() {
  is_->seekg(0, ios_base::beg);
}

UserDictionaryImporter::TextInputIterator::TextInputIterator(
    UserDictionaryImporter::IMEType ime_type,
    UserDictionaryImporter::TextLineIteratorInterface *iter)
    : ime_type_(NUM_IMES), iter_(iter) {
  CHECK(iter_);
  if (!iter_->IsAvailable()) {
    return;
  }

  UserDictionaryImporter::IMEType guessed_type =
      UserDictionaryImporter::NUM_IMES;
  string line;
  if (iter_->Next(&line)) {
    guessed_type = UserDictionaryImporter::GuessIMEType(line);
    iter_->Reset();
  }

  ime_type_ = DetermineFinalIMEType(ime_type, guessed_type);

  VLOG(1) << "Setting type to: " << static_cast<int>(ime_type_);
}
UserDictionaryImporter::TextInputIterator::~TextInputIterator() {}

bool UserDictionaryImporter::TextInputIterator::IsAvailable() const {
  DCHECK(iter_);
  return (iter_->IsAvailable() &&
          ime_type_ != UserDictionaryImporter::IME_AUTO_DETECT &&
          ime_type_ != UserDictionaryImporter::NUM_IMES);
}

bool UserDictionaryImporter::TextInputIterator::Next(
    UserDictionaryStorage::UserDictionaryEntry *entry) {
  DCHECK(iter_);
  if (!IsAvailable()) {
    LOG(ERROR) << "iterator is not available";
    return false;
  }

  if (entry == NULL) {
    LOG(ERROR) << "Entry is NULL";
    return false;
  }

  entry->Clear();

  string line;
  while (iter_->Next(&line)) {
    Util::ChopReturns(&line);
    if (line.empty()) {
      continue;
    }

    if (line[0] == '!' &&
        (ime_type_ == UserDictionaryImporter::MSIME ||
         ime_type_ == UserDictionaryImporter::ATOK)) {
      continue;
    }

    if (line[0] == '#' &&
        ime_type_ == UserDictionaryImporter::MOZC) {
      continue;
    }

    if (ime_type_ == UserDictionaryImporter::KOTOERI &&
        line.find("//") == 0) {
      continue;
    }

    VLOG(2) << line;

    vector<string> values;
    switch (ime_type_) {
      case UserDictionaryImporter::MSIME:
      case UserDictionaryImporter::ATOK:
      case UserDictionaryImporter::MOZC:
        Util::SplitStringAllowEmpty(line, "\t", &values);
        if (values.size() < 3) {
          continue;  // ignore this line
        }
        entry->set_key(values[0]);
        entry->set_value(values[1]);
        entry->set_pos(values[2]);
        if (values.size() >= 4) {
          entry->set_comment(values[3]);
        }
        return true;
        break;
      case UserDictionaryImporter::KOTOERI:
        Util::SplitCSV(line, &values);
        if (values.size() < 3) {
          continue;  // ignore this line
        }
        entry->set_key(values[0]);
        entry->set_value(values[1]);
        entry->set_pos(values[2]);
        return true;
        break;
      default:
        LOG(ERROR) << "Unknown format: " <<
            static_cast<int>(ime_type_);
        return false;
    }
  }

  return false;
}

bool UserDictionaryImporter::ConvertEntry(
    const UserDictionaryStorage::UserDictionaryEntry &from,
    UserDictionaryStorage::UserDictionaryEntry *to) {
  return ConvertEntryInternal(kPOSMap, arraysize(kPOSMap), from, to);
}

UserDictionaryImporter::IMEType
UserDictionaryImporter::GuessIMEType(const string &line) {
  if (line.empty()) {
    return UserDictionaryImporter::NUM_IMES;
  }

  string lower = line;
  Util::LowerString(&lower);

  if (lower.find("!microsoft ime") == 0) {
    return UserDictionaryImporter::MSIME;
  }

  // Old ATOK format (!!DICUT10) is not supported for now
  // http://b/2455897
  if (lower.find("!!dicut") == 0 && lower.size() > 7) {
    const string version = lower.substr(7, lower.size() - 7);
    if (Util::SimpleAtoi(version) >= 11) {
      return UserDictionaryImporter::ATOK;
    } else {
      return UserDictionaryImporter::NUM_IMES;
    }
  }

  if (lower.find("!!atok_tango_text_header") == 0) {
    return UserDictionaryImporter::ATOK;
  }

  if (line[0] == '"' && line[line.size() - 1] == '"' &&
      line.find("\t") == string::npos) {
    return UserDictionaryImporter::KOTOERI;
  }

  if (line[0] == '#' ||
      line.find("\t") != string::npos) {
    return UserDictionaryImporter::MOZC;
  }

  return UserDictionaryImporter::NUM_IMES;
}

  // return the final IME type from user_ime_type and guessed_ime_type
UserDictionaryImporter::IMEType UserDictionaryImporter::DetermineFinalIMEType(
    UserDictionaryImporter::IMEType user_ime_type,
    UserDictionaryImporter::IMEType guessed_ime_type) {
  UserDictionaryImporter::IMEType result_ime_type
      = UserDictionaryImporter::NUM_IMES;

  if (user_ime_type == UserDictionaryImporter::IME_AUTO_DETECT) {
    // trust guessed type
    result_ime_type = guessed_ime_type;
  } else if (user_ime_type == UserDictionaryImporter::MOZC) {
    // MOZC is compatible with MS-IME and ATOK.
    // Even if the auto detection failed, try to use Mozc format.
    if (guessed_ime_type != UserDictionaryImporter::KOTOERI) {
      result_ime_type = user_ime_type;
    }
  } else {
    // ATOK,MS-IME and Kotoeri can be detected with 100% accuracy.
    if (guessed_ime_type == user_ime_type) {
      result_ime_type = user_ime_type;
    }
  }

  return result_ime_type;
}


UserDictionaryImporter::EncodingType
UserDictionaryImporter::GuessEncodingType(const char *str, size_t size) {
  // Unicode BOM
  if (size >= 2 &&
      ((static_cast<uint8>(str[0]) == 0xFF &&
        static_cast<uint8>(str[1]) == 0xFE) ||
       (static_cast<uint8>(str[0]) == 0xFE &&
        static_cast<uint8>(str[1]) == 0xFF))) {
    return UserDictionaryImporter::UTF16;
  }

  // UTF-8 BOM
  if (size >= 3 &&
      static_cast<uint8>(str[0]) == 0xEF &&
      static_cast<uint8>(str[1]) == 0xBB &&
      static_cast<uint8>(str[2]) == 0xBF) {
    return UserDictionaryImporter::UTF8;
  }

  // Count valid UTF8
  // TODO(taku): improve the accuracy by making a DFA.
  const char *begin = str;
  const char *end = str + size;
  size_t valid_utf8 = 0;
  size_t valid_script = 0;
  while (begin < end) {
    size_t mblen = 0;
    const char32 ucs4 = Util::UTF8ToUCS4(begin, end, &mblen);
    ++valid_utf8;
    for (size_t i = 1; i < mblen; ++i) {
      if (begin[i] >= 0x80 && begin[i] <= 0xBF) {
        ++valid_utf8;
      }
    }

    // "\n\r\t " or Japanese code point
    if (ucs4 == 0x000A || ucs4 == 0x000D ||
        ucs4 == 0x0020 || ucs4 == 0x0009 ||
        Util::GetScriptType(ucs4) != Util::UNKNOWN_SCRIPT) {
      valid_script += mblen;
    }

    begin += mblen;
  }

  // TODO(taku): no theoritical justification for these
  // parameters
  if (1.0 * valid_utf8 / size >= 0.9 &&
      1.0 * valid_script / size >= 0.5) {
    return UserDictionaryImporter::UTF8;
  }

  return UserDictionaryImporter::SHIFT_JIS;
}

UserDictionaryImporter::EncodingType
UserDictionaryImporter::GuessFileEncodingType(const string &filename) {
  Mmap<char> mmap;
  if (!mmap.Open(filename.c_str(), "r")) {
    LOG(ERROR) << "cannot open: " << filename;
    return UserDictionaryImporter::NUM_ENCODINGS;
  }
  const size_t kMaxCheckSize = 1024;
  const size_t size = min(kMaxCheckSize,
                          static_cast<size_t>(mmap.GetFileSize()));
  return GuessEncodingType(mmap.begin(), size);
}
}  // namespace mozc
