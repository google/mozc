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

#include "config/character_form_manager.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/config_file_stream.h"
#include "base/number_util.h"
#include "base/singleton.h"
#include "base/strings/assign.h"
#include "base/strings/japanese.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "base/vlog.h"
#include "config/config_handler.h"
#include "protocol/config.pb.h"
#include "storage/lru_storage.h"

namespace mozc {
namespace config {
namespace {

using ::mozc::storage::LruStorage;

constexpr uint32_t kLruSize = 128;           // enough?
constexpr uint32_t kSeedValue = 0x7fe1fed1;  // random seed value for storage
constexpr char kFileName[] = "user://cform.db";

class CharacterFormManagerImpl {
 public:
  CharacterFormManagerImpl()
      : storage_(nullptr), require_consistent_conversion_(false) {}
  CharacterFormManagerImpl(const CharacterFormManagerImpl&) = delete;
  CharacterFormManagerImpl& operator=(const CharacterFormManagerImpl&) = delete;
  virtual ~CharacterFormManagerImpl() = default;

  Config::CharacterForm GetCharacterForm(absl::string_view str) const;

  void SetCharacterForm(absl::string_view str, Config::CharacterForm form);
  void GuessAndSetCharacterForm(absl::string_view str);

  void ConvertString(absl::string_view str, std::string* output) const;

  bool ConvertStringWithAlternative(absl::string_view str, std::string* output,
                                    std::string* alternative_output) const;

  // clear setting
  void Clear();

  // set default setting
  virtual void SetDefaultRule() = 0;

  // clear storage (not clear setting)
  void ClearHistory();

  // Note that rule is MERGED.
  // Call Clear() first if you want to set rule from scratch
  void AddRule(absl::string_view key, Config::CharacterForm form);

  void set_storage(LruStorage* storage) { storage_ = storage; }

  void set_require_consistent_conversion(bool val) {
    require_consistent_conversion_ = val;
  }

 private:
  Config::CharacterForm GetCharacterFormFromStorage(char16_t ucs2) const;

  void SaveCharacterFormToStorage(char16_t ucs2, Config::CharacterForm);

  // Returns true if input string will be consistent character form after
  // conversion.
  // For example:
  //  input = "3.14"
  //  preference for numbers = FULL_WIDTH
  //             for period = HALF_WIDTH
  //  this will be "３.１４" and it is not consistent
  //  so this function will return false
  bool TryConvertStringWithPreference(absl::string_view str,
                                      std::string* output) const;

  void ConvertStringAlternative(absl::string_view str,
                                std::string* output) const;

  LruStorage* storage_;

  // store the setting of a character
  absl::flat_hash_map<char16_t, Config::CharacterForm> conversion_table_;

  absl::flat_hash_map<char16_t, std::vector<char16_t>> group_table_;

  // When this flag is true,
  // character form conversion requires that output has consistent forms.
  // i.e. output should consists by half-width only or full-width only.
  bool require_consistent_conversion_;
};

// TODO(hidehiko): Get rid of inheritance.
class PreeditCharacterFormManagerImpl : public CharacterFormManagerImpl {
 public:
  PreeditCharacterFormManagerImpl() { SetDefaultRule(); }

  void SetDefaultRule() override {
    Clear();
    // AddRule("ア", Config::FULL_WIDTH);
    AddRule("ア", Config::FULL_WIDTH);
    AddRule("A", Config::FULL_WIDTH);
    AddRule("0", Config::FULL_WIDTH);
    AddRule("(){}[]", Config::FULL_WIDTH);
    AddRule(".,", Config::FULL_WIDTH);
    // AddRule("。、", Config::FULL_WIDTH);  // don't like half-width
    // AddRule("・「」", Config::FULL_WIDTH);  // don't like half-width
    AddRule("。、", Config::FULL_WIDTH);
    AddRule("・「」", Config::FULL_WIDTH);
    AddRule("\"'", Config::FULL_WIDTH);
    AddRule(":;", Config::FULL_WIDTH);
    AddRule("#%&@$^_|`\\", Config::FULL_WIDTH);
    AddRule("~", Config::FULL_WIDTH);
    AddRule("<>=+-/*", Config::FULL_WIDTH);
    AddRule("?!", Config::FULL_WIDTH);

    set_require_consistent_conversion(false);
  }
};

// TODO(hidehiko): Get rid of inheritance.
class ConversionCharacterFormManagerImpl : public CharacterFormManagerImpl {
 public:
  ConversionCharacterFormManagerImpl() { SetDefaultRule(); }

  void SetDefaultRule() override {
    Clear();
    // AddRule("ア", Config::FULL_WIDTH);
    // don't like half-width
    AddRule("ア", Config::FULL_WIDTH);
    AddRule("A", Config::LAST_FORM);
    AddRule("0", Config::LAST_FORM);
    AddRule("(){}[]", Config::LAST_FORM);
    AddRule(".,", Config::LAST_FORM);
    // AddRule("。、", Config::FULL_WIDTH);  // don't like half-width
    // AddRule("・「」", Config::FULL_WIDTH);  // don't like half-width
    AddRule("。、", Config::FULL_WIDTH);
    AddRule("・「」", Config::FULL_WIDTH);
    AddRule("\"'", Config::LAST_FORM);
    AddRule(":;", Config::LAST_FORM);
    AddRule("#%&@$^_|`\\", Config::LAST_FORM);
    AddRule("~", Config::LAST_FORM);
    AddRule("<>=+-/*", Config::LAST_FORM);
    AddRule("?!", Config::LAST_FORM);

    set_require_consistent_conversion(true);
  }
};

// A class to handle storage entries for number style.
class NumberStyleManager {
 public:
  NumberStyleManager()
      : key_(reinterpret_cast<const char*>(&kKey), sizeof(char16_t)),
        storage_(nullptr) {}

  void SetNumberStyle(const CharacterFormManager::NumberFormStyle& form_style) {
    const NumberStyleEntry entry(form_style);
    storage_->Insert(key_, reinterpret_cast<const char*>(&entry));
  }

  std::optional<const CharacterFormManager::NumberFormStyle> GetNumberStyle()
      const {
    const char* value = storage_->Lookup(key_);
    if (value == nullptr) {
      return std::nullopt;
    }
    const NumberStyleEntry* entry =
        reinterpret_cast<const NumberStyleEntry*>(value);
    CharacterFormManager::NumberFormStyle form_style{entry->form(),
                                                     entry->style()};
    return form_style;
  }

  void set_storage(LruStorage* storage) { storage_ = storage; }

 private:
  // Entry should fit value_size (sizeof uint32_t) of the storage.
  class NumberStyleEntry {
   public:
    explicit NumberStyleEntry(
        const CharacterFormManager::NumberFormStyle& form_style)
        : form_(form_style.form), style_(form_style.style) {}

    NumberUtil::NumberString::Style style() const {
      return static_cast<NumberUtil::NumberString::Style>(style_);
    }

    Config::CharacterForm form() const {
      return static_cast<Config::CharacterForm>(form_);
    }

   private:
    [[maybe_unused]] uint32_t reversed_ : 26;
    uint32_t form_ : 2;
    uint32_t style_ : 4;
  };

  // Note that "N" will not be returned by GetNormalizedCharacter().
  static constexpr char16_t kKey = 0x004E;  // "N"
  const absl::string_view key_;

  // This class does not have the ownership of `storage_`.
  LruStorage* storage_;
};

// Returns canonical/normalized UCS2 character for given string.
// Example:
// "インターネット" -> "ア"  (All katakana becomes "ア")
// "810124" -> "0"           (All numbers becomes "0")
// "Google" -> "A"           (All numbers becomes "A")
// "&" -> "&"                (Symbol is used as it is)
// "ほげほげ" -> 0x0000      (Unknown)
// "𠮟"       -> 0x0000      (Non BMP character is also Unknown)
char16_t GetNormalizedCharacter(const absl::string_view str) {
  const Util::ScriptType type = Util::GetScriptType(str);
  char16_t ucs2 = 0x0000;
  switch (type) {
    case Util::KATAKANA:
      ucs2 = 0x30A2;  // return "ア"
      break;
    case Util::NUMBER:
      ucs2 = 0x0030;  // return "0"
      break;
    case Util::ALPHABET:
      ucs2 = 0x0041;  // return "A"
      break;
    case Util::KANJI:
    case Util::HIRAGANA:
      ucs2 = 0x0000;  // no conversion
      break;
    default:                                        // maybe symbol
      if (strings::AtLeastCharsLen(str, 2) == 1) {  // must be 1 character
        // normalize it to half width
        std::string tmp = japanese::HalfWidthToFullWidth(str);
        char32_t codepoint = Utf8AsChars32(tmp).front();
        if (codepoint <= 0xffff) {
          ucs2 = static_cast<char16_t>(codepoint);
        } else {
          ucs2 = 0x0000;  // no conversion as fall back
        }
      }
      break;
  }

  return ucs2;
}

std::string ConvertToAlternative(std::string input, Util::FormType form,
                                 Util::ScriptType type) {
  switch (form) {
    case Util::FULL_WIDTH:
      if (type == Util::KATAKANA ||
          Util::IsFullWidthSymbolInHalfWidthKatakana(input)) {
        return japanese::HalfWidthToFullWidth(input);
      } else {
        return japanese::FullWidthToHalfWidth(input);
      }
      break;
    case Util::HALF_WIDTH:
      return japanese::HalfWidthToFullWidth(input);
      break;
    default:
      return input;
  }
}

Config::CharacterForm CharacterFormManagerImpl::GetCharacterForm(
    const absl::string_view str) const {
  const char16_t ucs2 = GetNormalizedCharacter(str);
  if (ucs2 == 0x0000) {
    return Config::NO_CONVERSION;
  }

  const auto it = conversion_table_.find(ucs2);
  if (it == conversion_table_.end()) {
    return Config::NO_CONVERSION;
  }

  if (it->second == Config::LAST_FORM) {
    return GetCharacterFormFromStorage(ucs2);
  }

  return it->second;
}

void CharacterFormManagerImpl::ClearHistory() {
  if (storage_ != nullptr) {
    storage_->Clear();
  }
}

// TODO(taku): need to chunk str
void CharacterFormManagerImpl::GuessAndSetCharacterForm(
    const absl::string_view str) {
  const Util::FormType form = Util::GetFormType(str);
  if (form == Util::FULL_WIDTH) {
    SetCharacterForm(str, Config::FULL_WIDTH);
    return;
  }

  if (form == Util::HALF_WIDTH) {
    SetCharacterForm(str, Config::HALF_WIDTH);
    return;
  }
}

void CharacterFormManagerImpl::SetCharacterForm(const absl::string_view str,
                                                Config::CharacterForm form) {
  const char16_t ucs2 = GetNormalizedCharacter(str);
  if (ucs2 == 0x0000) {
    return;
  }

  const auto it = conversion_table_.find(ucs2);
  if (it == conversion_table_.end()) {
    return;
  }

  if (it->second == Config::LAST_FORM) {
    SaveCharacterFormToStorage(ucs2, form);
    return;
  }
}

Config::CharacterForm CharacterFormManagerImpl::GetCharacterFormFromStorage(
    char16_t ucs2) const {
  if (storage_ == nullptr) {
    return Config::FULL_WIDTH;  // Return default setting
  }
  const absl::string_view key(reinterpret_cast<const char*>(&ucs2),
                              sizeof(ucs2));
  const char* value = storage_->Lookup(key);
  if (value == nullptr) {
    return Config::FULL_WIDTH;  // Return default setting
  }
  const uint32_t ivalue = *reinterpret_cast<const uint32_t*>(value);
  return static_cast<Config::CharacterForm>(ivalue);
}

void CharacterFormManagerImpl::SaveCharacterFormToStorage(
    char16_t ucs2, Config::CharacterForm form) {
  if (form != Config::FULL_WIDTH && form != Config::HALF_WIDTH) {
    return;
  }

  if (storage_ == nullptr) {
    return;
  }

  const absl::string_view key(reinterpret_cast<const char*>(&ucs2),
                              sizeof(ucs2));
  const char* value = storage_->Lookup(key);
  if (value != nullptr && static_cast<Config::CharacterForm>(*value) == form) {
    return;
  }

  // Do cast since CharacterForm may not be 32 bit
  const uint32_t iform = static_cast<uint32_t>(form);

  const auto iter = group_table_.find(ucs2);
  if (iter == group_table_.end()) {
    storage_->Insert(key, reinterpret_cast<const char*>(&iform));
  } else {
    // Update values in the same group.
    absl::Span<const char16_t> group = iter->second;
    for (size_t i = 0; i < group.size(); ++i) {
      const char16_t group_ucs2 = group[i];
      const absl::string_view group_key(
          reinterpret_cast<const char*>(&group_ucs2), sizeof(group_ucs2));
      storage_->Insert(group_key, reinterpret_cast<const char*>(&iform));
    }
  }
  MOZC_VLOG(2) << static_cast<uint16_t>(ucs2) << " is stored to " << kFileName
               << " as " << form;
}

void CharacterFormManagerImpl::ConvertString(const absl::string_view str,
                                             std::string* output) const {
  ConvertStringWithAlternative(str, output, nullptr);
}

bool CharacterFormManagerImpl::TryConvertStringWithPreference(
    const absl::string_view str, std::string* output) const {
  DCHECK(output);
  Config::CharacterForm target_form = Config::NO_CONVERSION;
  Config::CharacterForm prev_form = Config::NO_CONVERSION;
  Util::ScriptType prev_type = Util::UNKNOWN_SCRIPT;
  bool ret = true;

  std::string buf;
  const Utf8AsChars32 chars(str);
  for (auto it = chars.begin(); it != chars.end(); ++it) {
    if (!it.ok()) {
      continue;
    }
    const Util::ScriptType type = Util::GetScriptType(*it);
    // Cache previous ScriptType to reduce to call GetCharacterForm()
    Config::CharacterForm form = prev_form;
    if ((type == Util::UNKNOWN_SCRIPT) ||
        (type == Util::KATAKANA && prev_type != Util::KATAKANA) ||
        (type == Util::NUMBER && prev_type != Util::NUMBER) ||
        (type == Util::ALPHABET && prev_type != Util::ALPHABET)) {
      form = GetCharacterForm(it.view());
    } else if (type == Util::KANJI || type == Util::HIRAGANA) {
      form = Config::NO_CONVERSION;
    }

    // Cache previous Form to reduce to call ConvertToFullWidthOrHalf
    if (it != chars.begin() && prev_form != form) {
      *output += CharacterFormManager::ConvertWidth(std::move(buf), prev_form);
      buf.clear();
    }

    if (target_form == Config::NO_CONVERSION) {
      target_form = form;
    } else if (form != Config::NO_CONVERSION && form != target_form) {
      ret = false;
    }
    absl::StrAppend(&buf, it.view());
    prev_type = type;
    prev_form = form;
  }

  if (!buf.empty()) {
    *output += CharacterFormManager::ConvertWidth(std::move(buf), prev_form);
  }

  return ret;
}

void CharacterFormManagerImpl::ConvertStringAlternative(
    const absl::string_view str, std::string* output) const {
  DCHECK(output);
  Util::FormType prev_form = Util::UNKNOWN_FORM;
  Util::ScriptType prev_type = Util::UNKNOWN_SCRIPT;

  std::string buf;
  const Utf8AsChars32 chars(str);
  for (auto it = chars.begin(); it != chars.end(); ++it) {
    const Util::ScriptType type = Util::GetScriptType(*it);
    // Cache previous ScriptType to reduce to call GetFormType()
    Util::FormType form = prev_form;

    if ((type == Util::UNKNOWN_SCRIPT) ||
        (type == Util::KATAKANA && prev_type != Util::KATAKANA) ||
        (type == Util::NUMBER && prev_type != Util::NUMBER) ||
        (type == Util::ALPHABET && prev_type != Util::ALPHABET)) {
      form = Util::GetFormType(*it);
    } else if (type == Util::KANJI || type == Util::HIRAGANA) {
      form = Util::UNKNOWN_FORM;
    }

    // Cache previous Form to reduce to call ConvertToFullWidthOrHalf
    if (it != chars.begin() && prev_form != form) {
      *output += ConvertToAlternative(std::move(buf), prev_form, prev_type);
      buf.clear();
    }

    absl::StrAppend(&buf, it.view());
    prev_type = type;
    prev_form = form;
  }

  if (!buf.empty()) {
    *output += ConvertToAlternative(std::move(buf), prev_form, prev_type);
  }
}

bool CharacterFormManagerImpl::ConvertStringWithAlternative(
    const absl::string_view str, std::string* output,
    std::string* alternative_output) const {
  // If require_consistent_conversion_ is true,
  // do not convert to inconsistent form string.
  DCHECK(output);
  output->clear();
  if (!TryConvertStringWithPreference(str, output) &&
      require_consistent_conversion_) {
    strings::Assign(*output, str);
  }

  if (alternative_output != nullptr) {
    alternative_output->clear();
    ConvertStringAlternative(*output, alternative_output);
  }

  // return true if alternative_output and output are different
  return (alternative_output != nullptr && *alternative_output != *output);
}

void CharacterFormManagerImpl::Clear() {
  conversion_table_.clear();
  group_table_.clear();
}

void CharacterFormManagerImpl::AddRule(const absl::string_view key,
                                       Config::CharacterForm form) {
  std::vector<char16_t> group;
  for (const absl::string_view ch : Utf8AsChars(key)) {
    const char16_t ucs2 = GetNormalizedCharacter(ch);
    if (ucs2 != 0x0000) {
      group.push_back(ucs2);
    }
  }

  if (group.empty()) {
    return;
  }

  constexpr size_t kMaxGroupSize = 128;
  if (group.size() > kMaxGroupSize) {
    LOG(WARNING) << "Too long rule. skipped";
    return;
  }

  constexpr size_t kMaxTableSize = 256;
  if (conversion_table_.size() + group.size() > kMaxTableSize ||
      group_table_.size() + group.size() > kMaxTableSize) {
    LOG(WARNING) << "conversion_table becomes too big. skipped";
    return;
  }

  MOZC_VLOG(2) << "Adding Rule: " << key << " " << form;

  // sort + unique
  // use vector because set is slower.
  // group table is used in SaveCharacterFormToStorage and this will be called
  // every time user submits conversion.
  std::sort(group.begin(), group.end());
  group.erase(std::unique(group.begin(), group.end()), group.end());

  for (const char16_t ucs2 : group) {
    conversion_table_[ucs2] = form;  // overwrite
    if (group.size() > 1) {
      // add to group table
      // the key "UCS2" and other UCS2 in group are treated as the same way.
      group_table_[ucs2] = group;  // overwrite
    }
  }
}

}  // namespace

class CharacterFormManager::Data {
 public:
  Data();
  ~Data() = default;

  CharacterFormManagerImpl* GetPreeditManager() { return preedit_.get(); }
  CharacterFormManagerImpl* GetConversionManager() { return conversion_.get(); }
  NumberStyleManager* GetNumberStyleManager() { return number_style_.get(); }

 private:
  std::unique_ptr<PreeditCharacterFormManagerImpl> preedit_;
  std::unique_ptr<ConversionCharacterFormManagerImpl> conversion_;
  std::unique_ptr<NumberStyleManager> number_style_;
  std::unique_ptr<LruStorage> storage_;
};

CharacterFormManager::Data::Data() {
  const std::string filename = ConfigFileStream::GetFileName(kFileName);
  const uint32_t key_type = 0;
  storage_ = LruStorage::Create(filename.c_str(), sizeof(key_type), kLruSize,
                                kSeedValue);
  if (!storage_) {
    LOG(ERROR) << "cannot open " << filename;
    storage_ = std::make_unique<LruStorage>();
  }
  preedit_ = std::make_unique<PreeditCharacterFormManagerImpl>();
  conversion_ = std::make_unique<ConversionCharacterFormManagerImpl>();
  number_style_ = std::make_unique<NumberStyleManager>();
  preedit_->set_storage(storage_.get());
  conversion_->set_storage(storage_.get());
  number_style_->set_storage(storage_.get());
}

CharacterFormManager* CharacterFormManager::GetCharacterFormManager() {
  return Singleton<CharacterFormManager>::get();
}

CharacterFormManager::CharacterFormManager() : data_(std::make_unique<Data>()) {
  ReloadConfig(*ConfigHandler::GetSharedConfig());
}

void CharacterFormManager::ReloadConfig(const Config& config) {
  Clear();
  if (config.character_form_rules_size() > 0) {
    for (size_t i = 0; i < config.character_form_rules_size(); ++i) {
      const absl::string_view group = config.character_form_rules(i).group();
      const Config::CharacterForm preedit_form =
          config.character_form_rules(i).preedit_character_form();
      const Config::CharacterForm conversion_form =
          config.character_form_rules(i).conversion_character_form();
      AddPreeditRule(group, preedit_form);
      AddConversionRule(group, conversion_form);
    }
  } else {
    SetDefaultRule();
  }
}

std::string CharacterFormManager::ConvertWidth(std::string input,
                                               Config::CharacterForm form) {
  switch (form) {
    case Config::FULL_WIDTH:
      return japanese::HalfWidthToFullWidth(input);
    case Config::HALF_WIDTH:
      return japanese::FullWidthToHalfWidth(input);
    default:
      return input;
  }
}

void CharacterFormManager::ConvertPreeditString(const absl::string_view input,
                                                std::string* output) const {
  data_->GetPreeditManager()->ConvertString(input, output);
}

void CharacterFormManager::ConvertConversionString(
    const absl::string_view input, std::string* output) const {
  data_->GetConversionManager()->ConvertString(input, output);
}

bool CharacterFormManager::ConvertPreeditStringWithAlternative(
    const absl::string_view input, std::string* output,
    std::string* alternative_output) const {
  return data_->GetPreeditManager()->ConvertStringWithAlternative(
      input, output, alternative_output);
}

bool CharacterFormManager::ConvertConversionStringWithAlternative(
    const absl::string_view input, std::string* output,
    std::string* alternative_output) const {
  return data_->GetConversionManager()->ConvertStringWithAlternative(
      input, output, alternative_output);
}

Config::CharacterForm CharacterFormManager::GetPreeditCharacterForm(
    const absl::string_view input) const {
  return data_->GetPreeditManager()->GetCharacterForm(input);
}

Config::CharacterForm CharacterFormManager::GetConversionCharacterForm(
    const absl::string_view input) const {
  return data_->GetConversionManager()->GetCharacterForm(input);
}

void CharacterFormManager::ClearHistory() {
  // no need to call, as storage is shared
  // GetPreeditManager()->ClearHistory();
  MOZC_VLOG(1) << "CharacterFormManager::ClearHistory() is called";
  data_->GetConversionManager()->ClearHistory();
}

void CharacterFormManager::Clear() {
  MOZC_VLOG(1) << "CharacterFormManager::Clear() is called";
  data_->GetConversionManager()->Clear();
  data_->GetPreeditManager()->Clear();
}

void CharacterFormManager::SetCharacterForm(const absl::string_view input,
                                            Config::CharacterForm form) {
  // no need to call Preedit, as storage is shared
  // GetPreeditManager()->SetCharacterForm(input, form);
  data_->GetConversionManager()->SetCharacterForm(input, form);
}

void CharacterFormManager::GuessAndSetCharacterForm(
    const absl::string_view input) {
  // no need to call Preedit, as storage is shared
  // GetPreeditManager()->SetCharacterForm(input, form);
  data_->GetConversionManager()->GuessAndSetCharacterForm(input);
}

void CharacterFormManager::SetLastNumberStyle(
    const NumberFormStyle& form_style) {
  data_->GetNumberStyleManager()->SetNumberStyle(form_style);
}

std::optional<const CharacterFormManager::NumberFormStyle>
CharacterFormManager::GetLastNumberStyle() const {
  return data_->GetNumberStyleManager()->GetNumberStyle();
}

void CharacterFormManager::AddPreeditRule(const absl::string_view input,
                                          Config::CharacterForm form) {
  data_->GetPreeditManager()->AddRule(input, form);
}

void CharacterFormManager::AddConversionRule(const absl::string_view input,
                                             Config::CharacterForm form) {
  data_->GetConversionManager()->AddRule(input, form);
}

void CharacterFormManager::SetDefaultRule() {
  data_->GetPreeditManager()->SetDefaultRule();
  data_->GetConversionManager()->SetDefaultRule();
}

}  // namespace config
}  // namespace mozc
