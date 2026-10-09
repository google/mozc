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

#include "absl/base/no_destructor.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "base/number_util.h"
#include "base/strings/assign.h"
#include "base/strings/japanese.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "base/vlog.h"
#include "config/config_handler.h"
#include "prediction/user_history_predictor.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace config {
namespace {

using CharacterFormHistory =
    ::mozc::user_history_predictor::UserHistory::CharacterFormHistory;

// Thread-safe in-memory storage for character forms and number style,
// persisted via UserHistoryStorage.
class CharacterFormStorage {
 public:
  CharacterFormStorage() = default;
  CharacterFormStorage(const CharacterFormStorage&) = delete;
  CharacterFormStorage& operator=(const CharacterFormStorage&) = delete;

  Config::CharacterForm GetCharacterForm(char16_t ucs2) const {
    absl::MutexLock lock(mutex_);
    const auto& map = proto_.character_form_map();
    const auto it = map.find(ucs2);
    if (it == map.end() || !Config::CharacterForm_IsValid(it->second)) {
      return Config::FULL_WIDTH;  // Return default setting
    }
    return static_cast<Config::CharacterForm>(it->second);
  }

  void SetCharacterForm(absl::Span<const char16_t> ucs2_list,
                        Config::CharacterForm form) {
    absl::MutexLock lock(mutex_);
    auto* map = proto_.mutable_character_form_map();
    for (const char16_t ucs2 : ucs2_list) {
      auto [it, inserted] = map->try_emplace(ucs2, static_cast<uint32_t>(form));
      if (!inserted) {
        if (it->second == static_cast<uint32_t>(form)) {
          continue;
        }
        it->second = static_cast<uint32_t>(form);
      }
      dirty_ = true;
    }
  }

  std::optional<const CharacterFormManager::NumberFormStyle> GetNumberStyle()
      const {
    absl::MutexLock lock(mutex_);
    if (!proto_.has_last_number_form() || !proto_.has_last_number_style() ||
        !Config::CharacterForm_IsValid(proto_.last_number_form())) {
      return std::nullopt;
    }
    return CharacterFormManager::NumberFormStyle{
        static_cast<Config::CharacterForm>(proto_.last_number_form()),
        static_cast<NumberUtil::NumberString::Style>(
            proto_.last_number_style())};
  }

  void SetNumberStyle(const CharacterFormManager::NumberFormStyle& form_style) {
    absl::MutexLock lock(mutex_);
    if (proto_.has_last_number_form() && proto_.has_last_number_style() &&
        proto_.last_number_form() == static_cast<uint32_t>(form_style.form) &&
        proto_.last_number_style() == static_cast<uint32_t>(form_style.style)) {
      return;
    }
    proto_.set_last_number_form(static_cast<uint32_t>(form_style.form));
    proto_.set_last_number_style(static_cast<uint32_t>(form_style.style));
    dirty_ = true;
  }

  void Clear() {
    absl::MutexLock lock(mutex_);
    proto_.Clear();
    dirty_ = false;
  }

  bool IsDirty() const {
    absl::MutexLock lock(mutex_);
    return dirty_;
  }

  void Load(const user_history_predictor::UserHistory& history) {
    absl::MutexLock lock(mutex_);
    proto_ = history.character_form_history();
    dirty_ = false;
  }

  void Save(user_history_predictor::UserHistory* history) {
    DCHECK(history);
    absl::MutexLock lock(mutex_);
    if (proto_.ByteSizeLong() > 0) {
      *history->mutable_character_form_history() = proto_;
    } else {
      history->clear_character_form_history();
    }
    dirty_ = false;
  }

 private:
  mutable absl::Mutex mutex_;
  CharacterFormHistory proto_ ABSL_GUARDED_BY(mutex_);
  bool dirty_ ABSL_GUARDED_BY(mutex_) = false;
};

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

  void set_storage(CharacterFormStorage* storage) { storage_ = storage; }

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

  CharacterFormStorage* storage_;

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

struct CharToken {
  char32_t cp;
  absl::string_view view;
  Util::ScriptType type;
  Util::FormType form;
};

// Tokenizes |str| into unicode character tokens with their script type and form
// type.
//
// Contextual adjustment:
// Periods ('.', '．') and commas (',', '，') surrounded by numeric digits (e.g.
// '.' in "3.14" or ',' in "1,000") are reclassified as Util::NUMBER.
// Only '.' and ',' are reclassified because they represent internal decimal
// and thousands separators forming a single numeric entity. Other separators
// like '/' (dates: "2026/10/07") and ':' (times: "12:30") are not reclassified
// because they separate distinct numeric components, and are guarded against
// mixed-width artifacts by the run-based consistency check below.
// Reclassified separators adopt GetCharacterForm("0") so they follow the
// number rule instead of their own symbol group rule.
std::vector<CharToken> TokenizeStringForConversion(absl::string_view str) {
  std::vector<CharToken> tokens;
  const Utf8AsChars32 chars(str);
  for (auto it = chars.begin(); it != chars.end(); ++it) {
    if (!it.ok()) {
      continue;
    }
    tokens.push_back(
        {*it, it.view(), Util::GetScriptType(*it), Util::GetFormType(*it)});
  }
  for (size_t i = 1; i + 1 < tokens.size(); ++i) {
    if ((tokens[i].cp == '.' || tokens[i].cp == U'．' || tokens[i].cp == ',' ||
         tokens[i].cp == U'，') &&
        tokens[i - 1].type == Util::NUMBER &&
        tokens[i + 1].type == Util::NUMBER) {
      tokens[i].type = Util::NUMBER;
    }
  }
  return tokens;
}

// Returns true if the token acts as a delimiter between separate
// variable-width runs (e.g. kanji, hiragana, katakana, or fixed full-width
// Japanese punctuation/brackets in "Tシャツ", "3時", "「Tシャツ」", or
// "100。").
bool IsVariableWidthRunDelimiter(const CharToken& token) {
  return token.type == Util::KANJI || token.type == Util::HIRAGANA ||
         token.type == Util::KATAKANA ||
         Util::IsFullWidthSymbolInHalfWidthKatakana(token.view);
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

void CharacterFormManagerImpl::GuessAndSetCharacterForm(
    const absl::string_view str) {
  Util::ScriptType prev_type = Util::UNKNOWN_SCRIPT;
  absl::string_view chunk;
  auto flush_chunk = [&](absl::string_view target, Util::ScriptType type) {
    if (target.empty()) {
      return;
    }
    // Ignore symbol chunks embedded in a larger string (e.g. ',' in "1,000")
    // so they do not overwrite symbol group preferences, while still learning
    // when the entire committed string is a symbol (e.g. "[").
    if (type == Util::UNKNOWN_SCRIPT && target.size() != str.size()) {
      return;
    }
    const Util::FormType form = Util::GetFormType(target);
    if (form == Util::FULL_WIDTH) {
      SetCharacterForm(target, Config::FULL_WIDTH);
    } else if (form == Util::HALF_WIDTH) {
      SetCharacterForm(target, Config::HALF_WIDTH);
    }
  };

  const Utf8AsChars32 chars(str);
  for (auto it = chars.begin(); it != chars.end(); ++it) {
    if (!it.ok()) {
      flush_chunk(chunk, prev_type);
      chunk = absl::string_view();
      prev_type = Util::UNKNOWN_SCRIPT;
      continue;
    }
    Util::ScriptType type = Util::GetScriptType(*it);
    if (*it == U'・') {
      // Treat full-width middle dot as UNKNOWN_SCRIPT (matching
      // GetNormalizedCharacter("・")) so embedded middle dots (e.g. in
      // "東京・大阪" or "ｱ・ｲ") are ignored like other symbols.
      type = Util::UNKNOWN_SCRIPT;
    }
    if (!chunk.empty() && type != prev_type) {
      flush_chunk(chunk, prev_type);
      chunk = it.view();
    } else if (chunk.empty()) {
      chunk = it.view();
    } else {
      chunk = absl::string_view(
          chunk.data(), static_cast<size_t>(it.view().data() +
                                            it.view().size() - chunk.data()));
    }
    prev_type = type;
  }
  flush_chunk(chunk, prev_type);
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
  return storage_->GetCharacterForm(ucs2);
}

void CharacterFormManagerImpl::SaveCharacterFormToStorage(
    char16_t ucs2, Config::CharacterForm form) {
  if (form != Config::FULL_WIDTH && form != Config::HALF_WIDTH) {
    return;
  }

  if (storage_ == nullptr) {
    return;
  }

  const auto iter = group_table_.find(ucs2);
  if (iter == group_table_.end()) {
    storage_->SetCharacterForm(absl::MakeConstSpan(&ucs2, 1), form);
  } else {
    // Update values in the same group.
    storage_->SetCharacterForm(iter->second, form);
  }
  MOZC_VLOG(2) << static_cast<uint16_t>(ucs2) << " is stored as " << form;
}

void CharacterFormManagerImpl::ConvertString(const absl::string_view str,
                                             std::string* output) const {
  ConvertStringWithAlternative(str, output, nullptr);
}

bool CharacterFormManagerImpl::TryConvertStringWithPreference(
    const absl::string_view str, std::string* output) const {
  DCHECK(output);
  const std::vector<CharToken> tokens = TokenizeStringForConversion(str);
  if (tokens.empty()) {
    return true;
  }

  Config::CharacterForm run_target_form = Config::NO_CONVERSION;
  Config::CharacterForm prev_form = Config::NO_CONVERSION;
  Util::ScriptType prev_type = Util::UNKNOWN_SCRIPT;
  bool ret = true;

  std::string buf;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const auto& token = tokens[i];
    const Util::ScriptType type = token.type;

    // Cache previous ScriptType to reduce to call GetCharacterForm()
    Config::CharacterForm form = prev_form;
    if (type == Util::NUMBER) {
      if (prev_type != Util::NUMBER) {
        form = GetCharacterForm("0");
      }
    } else if (type == Util::UNKNOWN_SCRIPT) {
      form = GetCharacterForm(token.view);
    } else if (type == Util::KATAKANA && prev_type != Util::KATAKANA) {
      form = GetCharacterForm(token.view);
    } else if (type == Util::ALPHABET && prev_type != Util::ALPHABET) {
      form = GetCharacterForm(token.view);
    } else if (type == Util::KANJI || type == Util::HIRAGANA) {
      form = Config::NO_CONVERSION;
    }

    // Cache previous Form to reduce to call ConvertWidth
    if (i > 0 && prev_form != form) {
      *output += CharacterFormManager::ConvertWidth(std::move(buf), prev_form);
      buf.clear();
    }

    if (IsVariableWidthRunDelimiter(token)) {
      // Delimiters (kanji, hiragana, katakana, and fixed Japanese punctuation/
      // brackets) delimit separate variable-width runs, allowing e.g. "Tシャツ"
      // or "3時" to convert consistently without being blocked by mixed
      // scripts.
      run_target_form = Config::NO_CONVERSION;
    } else {
      if (run_target_form == Config::NO_CONVERSION) {
        run_target_form = form;
      } else if (form != Config::NO_CONVERSION && form != run_target_form) {
        ret = false;
      }
    }
    absl::StrAppend(&buf, token.view);
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
  const std::vector<CharToken> tokens = TokenizeStringForConversion(str);
  if (tokens.empty()) {
    return;
  }

  Util::FormType prev_form = Util::UNKNOWN_FORM;
  Util::ScriptType prev_type = Util::UNKNOWN_SCRIPT;

  std::string buf;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const auto& token = tokens[i];
    const Util::ScriptType type = token.type;

    Util::FormType form = prev_form;
    if ((type == Util::UNKNOWN_SCRIPT) ||
        (type == Util::KATAKANA && prev_type != Util::KATAKANA) ||
        (type == Util::NUMBER && prev_type != Util::NUMBER) ||
        (type == Util::ALPHABET && prev_type != Util::ALPHABET)) {
      form = token.form;
    } else if (type == Util::KANJI || type == Util::HIRAGANA) {
      form = Util::UNKNOWN_FORM;
    }

    // Flush when form or script type changes.
    if (i > 0 && (prev_form != form || prev_type != type)) {
      *output += ConvertToAlternative(std::move(buf), prev_form, prev_type);
      buf.clear();
    }

    absl::StrAppend(&buf, token.view);
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
  DCHECK(output);
  output->clear();
  const bool is_consistent = TryConvertStringWithPreference(str, output);
  if (!is_consistent && require_consistent_conversion_) {
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
  CharacterFormStorage* GetStorage() { return storage_.get(); }

 private:
  std::unique_ptr<PreeditCharacterFormManagerImpl> preedit_;
  std::unique_ptr<ConversionCharacterFormManagerImpl> conversion_;
  std::unique_ptr<CharacterFormStorage> storage_;
};

CharacterFormManager::Data::Data() {
  storage_ = std::make_unique<CharacterFormStorage>();
  preedit_ = std::make_unique<PreeditCharacterFormManagerImpl>();
  conversion_ = std::make_unique<ConversionCharacterFormManagerImpl>();
  preedit_->set_storage(storage_.get());
  conversion_->set_storage(storage_.get());
}

CharacterFormManager* CharacterFormManager::GetCharacterFormManager() {
  static absl::NoDestructor<CharacterFormManager> manager;
  return manager.get();
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

void CharacterFormManager::LoadStorage(
    const user_history_predictor::UserHistory& history) {
  data_->GetStorage()->Load(history);
}

void CharacterFormManager::SaveStorage(
    user_history_predictor::UserHistory* history) {
  data_->GetStorage()->Save(history);
}

bool CharacterFormManager::IsStorageDirty() const {
  return data_->GetStorage()->IsDirty();
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
  data_->GetStorage()->SetNumberStyle(form_style);
}

std::optional<const CharacterFormManager::NumberFormStyle>
CharacterFormManager::GetLastNumberStyle() const {
  return data_->GetStorage()->GetNumberStyle();
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
