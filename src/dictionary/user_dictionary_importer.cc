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

#include "dictionary/user_dictionary_importer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/types/span.h"
#include "base/japanese_util.h"
#include "base/mmap.h"
#include "base/number_util.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "base/vlog.h"
#include "dictionary/user_dictionary_util.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace user_dictionary {
namespace {

using ::mozc::user_dictionary::UserDictionary;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;

size_t HashOf(const UserDictionary::Entry& entry) {
  DCHECK(UserDictionary::PosType_IsValid(entry.pos()));
  return absl::HashOf(entry.key(), entry.value(), entry.pos());
}

// Normalizes POS (removes full width ascii and half width katakana).
std::string NormalizePos(const absl::string_view input) {
  std::string tmp = japanese_util::FullWidthAsciiToHalfWidthAscii(input);
  return japanese_util::HalfWidthKatakanaToFullWidthKatakana(tmp);
}

std::pair<absl::string_view, absl::string_view> SplitPosAndLocale(
    const absl::string_view pos ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return absl::StrSplit(pos, absl::MaxSplits(':', 1));
}

// A data type to hold conversion rules of POSes. If mozc_pos is set to be an
// empty string (""), it means that words of the POS should be ignored in Mozc.
struct PosMap {
  absl::string_view source_pos;      // POS string of a third party IME.
  UserDictionary::PosType mozc_pos;  // POS of Mozc.
};

// Include actual POS mapping rules defined outside the file.
#include "dictionary/pos_map.inc"

// Convert POS of a third party IME to that of Mozc using the given mapping.
bool ConvertEntryInternal(const absl::Span<const PosMap> pos_map,
                          const RawEntry& from, UserDictionary::Entry* to) {
  if (to == nullptr) {
    LOG(ERROR) << "Null pointer is passed.";
    return false;
  }

  to->Clear();

  if (from.pos.empty()) {
    return false;
  }

  const std::string normalized_pos = NormalizePos(from.pos);
  auto [pos, locale] = SplitPosAndLocale(normalized_pos);

  // ATOK's POS has a special marker for distinguishing auto-registered
  // words/manually-registered words. Remove the mark here.
  absl::ConsumePrefix(&pos, "$");
  absl::ConsumePrefix(&pos, "*");

  // Search for mapping for the given POS.
  const auto found = absl::c_lower_bound(
      pos_map, pos, [](const PosMap map, absl::string_view pos) -> bool {
        return map.source_pos < pos;
      });
  if (found == pos_map.end() || found->source_pos != pos) {
    LOG(WARNING) << "Invalid POS is passed: " << from.pos;
    return false;
  }
  if (!UserDictionary::PosType_IsValid(found->mozc_pos)) {
    to->clear_key();
    to->clear_value();
    to->clear_pos();
    return false;
  }

  to->set_key(from.key);
  to->set_value(from.value);
  to->set_pos(found->mozc_pos);

  // Normalize reading.
  to->set_key(user_dictionary::NormalizeReading(to->key()));

  // Copy comment.
  if (!from.comment.empty()) {
    to->set_comment(from.comment);
  }

  // Sets locale field.
  if (!locale.empty()) {
    to->set_locale(locale);
  }

  // Validation.
  if (user_dictionary::ValidateEntry(*to) !=
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    return false;
  }

  return true;
}

}  // namespace

ErrorType ImportFromIterator(InputIteratorInterface* iter,
                             UserDictionary* user_dic) {
  if (iter == nullptr || user_dic == nullptr) {
    LOG(ERROR) << "iter or user_dic is nullptr";
    return IMPORT_FATAL;
  }

  const size_t max_size = user_dictionary::max_entry_size();

  ErrorType ret = IMPORT_NO_ERROR;

  absl::flat_hash_set<size_t> existent_entries;
  for (const auto& entry : user_dic->entries()) {
    existent_entries.insert(HashOf(entry));
  }

  RawEntry raw_entry;
  while (iter->Next(&raw_entry)) {
    if (user_dic->entries_size() >= max_size) {
      LOG(WARNING) << "Too many words in one dictionary";
      return IMPORT_TOO_MANY_WORDS;
    }

    if (raw_entry.key.empty() && raw_entry.value.empty() &&
        raw_entry.comment.empty()) {
      // Empty entry is just skipped. It could be annoying if we show a
      // warning dialog when these empty candidates exist.
      continue;
    }

    UserDictionary::Entry entry;
    if (!ConvertEntry(raw_entry, &entry)) {
      LOG(WARNING) << "Entry is not valid";
      ret = IMPORT_INVALID_ENTRIES;
      continue;
    }

    // Don't register words if it is already in the current dictionary.
    if (!existent_entries.insert(HashOf(entry)).second) {
      continue;
    }

    user_dic->mutable_entries()->Add(std::move(entry));
  }

  return ret;
}

ErrorType ImportFromTextLineIterator(IMEType ime_type,
                                     TextLineIteratorInterface* iter,
                                     UserDictionary* user_dic) {
  TextInputIterator text_iter(ime_type, iter);
  if (text_iter.ime_type() == NUM_IMES) {
    return IMPORT_NOT_SUPPORTED;
  }

  return ImportFromIterator(&text_iter, user_dic);
}

StringTextLineIterator::StringTextLineIterator(absl::string_view data)
    : data_(data), first_(data_.begin()) {}

bool StringTextLineIterator::IsAvailable() const {
  return data_.end() != first_;
}

bool StringTextLineIterator::Next(std::string* line) {
  if (!IsAvailable()) {
    return false;
  }

  constexpr absl::string_view kCRLF("\r\n");
  for (auto it = first_; it != data_.end(); ++it) {
    if (*it == '\n' || *it == '\r') {
      line->assign(first_, it);
      // Handles CR/LF issue.
      if (absl::string_view(std::to_address(it), data_.end() - it)
              .starts_with(kCRLF)) {
        ++it;
      }
      first_ = ++it;
      return true;
    }
  }

  line->assign(first_, data_.end());
  first_ = data_.end();
  return true;
}

void StringTextLineIterator::Reset() { first_ = data_.begin(); }

TextInputIterator::TextInputIterator(IMEType ime_type,
                                     TextLineIteratorInterface* iter)
    : ime_type_(NUM_IMES), iter_(iter) {
  CHECK(iter_);
  if (!iter_->IsAvailable()) {
    return;
  }

  IMEType guessed_type = NUM_IMES;
  std::string line;
  if (iter_->Next(&line)) {
    guessed_type = GuessIMEType(line);
    iter_->Reset();
  }

  ime_type_ = DetermineFinalIMEType(ime_type, guessed_type);

  MOZC_VLOG(1) << "Setting type to: " << static_cast<int>(ime_type_);
}

bool TextInputIterator::IsAvailable() const {
  DCHECK(iter_);
  return (iter_->IsAvailable() && ime_type_ != IME_AUTO_DETECT &&
          ime_type_ != NUM_IMES);
}

bool TextInputIterator::Next(RawEntry* entry) {
  DCHECK(iter_);
  if (!IsAvailable()) {
    LOG(ERROR) << "iterator is not available";
    return false;
  }

  if (entry == nullptr) {
    LOG(ERROR) << "Entry is nullptr";
    return false;
  }

  entry->Clear();

  std::string line;
  while (iter_->Next(&line)) {
    Util::ChopReturns(&line);
    // Skip empty lines.
    if (line.empty()) {
      continue;
    }
    // Skip comment lines.
    // TODO(yukawa): Use string::front once C++11 is enabled on Mac.
    if (((ime_type_ == MSIME || ime_type_ == ATOK) && line[0] == '!') ||
        ((ime_type_ == MOZC || ime_type_ == GBOARD_V1) && line[0] == '#') ||
        (ime_type_ == KOTOERI && line.starts_with("//"))) {
      continue;
    }

    MOZC_VLOG(2) << line;

    std::vector<std::string> values;
    switch (ime_type_) {
      case MSIME:
      case ATOK:
      case MOZC:
      case GBOARD_V1:
        values = absl::StrSplit(line, '\t', absl::AllowEmpty());
        if (values.size() < 3) {
          continue;  // Ignore this line.
        }
        entry->key = std::move(values[0]);
        entry->value = std::move(values[1]);
        if (ime_type_ == GBOARD_V1) {
          // values[2] specifies locale, not POS.
          // Use NO_POS as a default value.
          // pos = "品詞なし" + ":" + locale
          entry->pos = absl::StrCat("品詞なし:", std::move(values[2]));
        } else {
          entry->pos = std::move(values[2]);
        }
        if (values.size() >= 4) {
          entry->comment = std::move(values[3]);
        }
        return true;
        break;
      case KOTOERI:
        Util::SplitCSV(line, &values);
        if (values.size() < 3) {
          continue;  // Ignore this line.
        }
        entry->key = std::move(values[0]);
        entry->value = std::move(values[1]);
        entry->pos = std::move(values[2]);
        return true;
        break;
      default:
        LOG(ERROR) << "Unknown format: " << static_cast<int>(ime_type_);
        return false;
    }
  }

  return false;
}

bool ConvertEntry(const RawEntry& from, UserDictionary::Entry* to) {
  return ConvertEntryInternal(kPosMap, from, to);
}

IMEType GuessIMEType(absl::string_view line) {
  if (line.empty()) {
    return NUM_IMES;
  }

  std::string lower(line);
  Util::LowerString(&lower);

  if (lower.starts_with("!microsoft ime")) {
    return MSIME;
  }

  // Old ATOK format (!!DICUT10) is not supported for now
  // http://b/2455897
  if (lower.starts_with("!!dicut") && lower.size() > 7) {
    const absl::string_view version(lower.data() + 7, lower.size() - 7);
    if (NumberUtil::SimpleAtoi(version) >= 11) {
      return ATOK;
    } else {
      return NUM_IMES;
    }
  }

  if (lower.starts_with("!!atok_tango_text_header")) {
    return ATOK;
  }

  if (line.front() == '"' && line.back() == '"' &&
      !absl::StrContains(line, '\t')) {
    return KOTOERI;
  }

  if (lower.starts_with("# gboard dictionary version:1")) {
    return GBOARD_V1;
  }

  if (line.front() == '#' || absl::StrContains(line, '\t')) {
    return MOZC;
  }

  return NUM_IMES;
}

IMEType DetermineFinalIMEType(IMEType user_ime_type, IMEType guessed_ime_type) {
  IMEType result_ime_type = NUM_IMES;

  if (user_ime_type == IME_AUTO_DETECT) {
    // Trust guessed type.
    result_ime_type = guessed_ime_type;
  } else if (user_ime_type == MOZC) {
    // MOZC is compatible with MS-IME and ATOK.
    // Even if the auto detection failed, try to use Mozc format.
    if (guessed_ime_type != KOTOERI) {
      result_ime_type = user_ime_type;
    }
  } else {
    // ATOK, MS-IME and Kotoeri can be detected with 100% accuracy.
    if (guessed_ime_type == user_ime_type) {
      result_ime_type = user_ime_type;
    }
  }

  return result_ime_type;
}

EncodingType GuessEncodingType(absl::string_view str) {
  // Unicode BOM.
  if (str.size() >= 2 && ((static_cast<uint8_t>(str[0]) == 0xFF &&
                           static_cast<uint8_t>(str[1]) == 0xFE) ||
                          (static_cast<uint8_t>(str[0]) == 0xFE &&
                           static_cast<uint8_t>(str[1]) == 0xFF))) {
    return UTF16;
  }

  // UTF-8 BOM.
  if (str.size() >= 3 && static_cast<uint8_t>(str[0]) == 0xEF &&
      static_cast<uint8_t>(str[1]) == 0xBB &&
      static_cast<uint8_t>(str[2]) == 0xBF) {
    return UTF8;
  }

  // Count valid UTF8 characters.
  // TODO(taku): Improve the accuracy by making a DFA.
  size_t valid_utf8 = 0;
  size_t valid_script = 0;
  for (const UnicodeChar ch : Utf8AsUnicodeChar(str)) {
    if (!ch.ok()) {
      break;
    }
    valid_utf8 += ch.utf8().size();

    // "\n\r\t " or Japanese code point
    const char32_t codepoint = ch.char32();
    if (codepoint == 0x000A || codepoint == 0x000D || codepoint == 0x0020 ||
        codepoint == 0x0009 ||
        Util::GetScriptType(codepoint) != Util::UNKNOWN_SCRIPT) {
      valid_script += ch.utf8().size();
    }
  }

  // TODO(taku): No theoretical justification for these parameters.
  if (1.0 * valid_utf8 / str.size() >= 0.9 &&
      1.0 * valid_script / str.size() >= 0.5) {
    return UTF8;
  }

  return SHIFT_JIS;
}

EncodingType GuessFileEncodingType(const std::string& filename) {
  absl::StatusOr<Mmap> mmap = Mmap::Map(filename, Mmap::READ_ONLY);
  if (!mmap.ok()) {
    LOG(ERROR) << "cannot open: " << filename << ": " << mmap.status();
    return NUM_ENCODINGS;
  }
  constexpr size_t kMaxCheckSize = 1024;
  const size_t size = std::min<size_t>(kMaxCheckSize, mmap->size());
  const absl::string_view mapped_data(mmap->begin(), size);
  return GuessEncodingType(mapped_data);
}

}  // namespace user_dictionary
}  // namespace mozc
