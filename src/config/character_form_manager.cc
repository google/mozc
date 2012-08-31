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

#include "config/character_form_manager.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "storage/lru_storage.h"

namespace mozc {
namespace config {

using mozc::storage::LRUStorage;

namespace {

const uint32 kLRUSize    = 128;  // enough?
const uint32 kSeedValue  = 0x7fe1fed1;  // random seed value for storage
const char   kFileName[] = "user://cform.db";

REGISTER_MODULE_RELOADER(
    character_form,
    { CharacterFormManager::GetCharacterFormManager()->Reload(); } )

class CharacterFormManagerImpl {
 public:
  CharacterFormManagerImpl();
  virtual ~CharacterFormManagerImpl();

  Config::CharacterForm GetCharacterForm(const string &key) const;

  void SetCharacterForm(const string &key, Config::CharacterForm form);
  void GuessAndSetCharacterForm(const string &key);

  void ConvertString(const string &input, string *output) const;

  bool ConvertStringWithAlternative(
      const string &input, string *output, string *alternative_output) const;

  // clear setting
  void Clear();

  // set default setting
  virtual void SetDefaultRule() = 0;

  // clear storage (not clear setting)
  void ClearHistory();

  // Note that rule is MERGED.
  // Call Clear() first if you want to set rule from scratch
  void AddRule(const string &key, Config::CharacterForm form);

  void set_storage(LRUStorage *storage) {
    storage_ = storage;
  }

  void set_require_consistent_conversion(bool val) {
    require_consistent_conversion_ = val;
  }

 private:
  Config::CharacterForm GetCharacterFormFromStorage(uint16 ucs2) const;

  void SaveCharacterFormToStorage(uint16 ucs2, Config::CharacterForm);

  // Returns true if input string will be consistent character form after
  // conversion.
  // For example:
  //  input = "3.14"
  //  preference for numbers = FULL_WIDTH
  //             for period = HALF_WIDTH
  //  this will be "３.１４" and it is not consistent
  //  so this function will return false
  bool TryConvertStringWithPreference(const string &str, string *output) const;

  void ConvertStringAlternative(const string &str, string *output) const;

  LRUStorage *storage_;

  // store the setting of a character
  map<uint16, Config::CharacterForm> conversion_table_;

  map<uint16, vector<uint16> > group_table_;

  // When this flag is true,
  // character form conversion requires that output has consistent forms.
  // i.e. output should consists by half-width only or full-width only.
  bool require_consistent_conversion_;

  DISALLOW_COPY_AND_ASSIGN(CharacterFormManagerImpl);
};

// TODO(hidehiko): Get rid of inheritance.
class PreeditCharacterFormManagerImpl : public CharacterFormManagerImpl {
 public:
  PreeditCharacterFormManagerImpl() {
    SetDefaultRule();
  }

  virtual void SetDefaultRule() {
    Clear();
    // AddRule("ア", Config::FULL_WIDTH);
    AddRule("\xE3\x82\xA2", Config::FULL_WIDTH);
    AddRule("A", Config::FULL_WIDTH);
    AddRule("0", Config::FULL_WIDTH);
    AddRule("(){}[]", Config::FULL_WIDTH);
    AddRule(".,", Config::FULL_WIDTH);
    // AddRule("。、", Config::FULL_WIDTH);  // don't like half-width
    // AddRule("・「」", Config::FULL_WIDTH);  // don't like half-width
    AddRule("\xE3\x80\x82\xE3\x80\x81", Config::FULL_WIDTH);
    AddRule("\xE3\x83\xBB\xE3\x80\x8C\xE3\x80\x8D", Config::FULL_WIDTH);
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
  ConversionCharacterFormManagerImpl() {
    SetDefaultRule();
  }

  virtual void SetDefaultRule() {
    Clear();
    // AddRule("ア", Config::FULL_WIDTH);
    // don't like half-width
    AddRule("\xE3\x82\xA2", Config::FULL_WIDTH);
    AddRule("A", Config::LAST_FORM);
    AddRule("0", Config::LAST_FORM);
    AddRule("(){}[]", Config::LAST_FORM);
    AddRule(".,", Config::LAST_FORM);
    // AddRule("。、", Config::FULL_WIDTH);  // don't like half-width
    // AddRule("・「」", Config::FULL_WIDTH);  // don't like half-width
    AddRule("\xE3\x80\x82\xE3\x80\x81", Config::FULL_WIDTH);
    AddRule("\xE3\x83\xBB\xE3\x80\x8C\xE3\x80\x8D", Config::FULL_WIDTH);
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
// "810124" -> "0"         (All numbers becomes "0")
// "Google" -> "A"         (All numbers becomes "A")
// "&" -> "&"              (Symbol is used as it is)
// "ほげほげ" -> 0x0000     (Unknown)
uint16 GetNormalizedCharacter(const string &str) {
  const Util::ScriptType type = Util::GetScriptType(str);
  uint16 ucs2 = 0x0000;
  switch (type) {
    case Util::KATAKANA:
      ucs2 = 0x30A2;   // return "ア"
      break;
    case Util::NUMBER:
      ucs2 = 0x0030;   // return "0"
      break;
    case Util::ALPHABET:
      ucs2 = 0x0041;   // return "A"
      break;
    case Util::KANJI:
    case Util::HIRAGANA:
      ucs2 = 0x0000;  // no conversion
      break;
    default:   // maybe symbol
      if (Util::CharsLen(str) == 1) {   // must be 1 character
        // normalize it to half width
        string tmp;
        Util::HalfWidthToFullWidth(str, &tmp);
        size_t mblen = 0;
        ucs2 = Util::UTF8ToUCS2(tmp.c_str(),
                                tmp.c_str() + tmp.size(),
                                &mblen);
      }
      break;
  }

  return ucs2;
}

void ConvertToAlternative(const string &input, string *output,
                          Util::FormType form, Util::ScriptType type) {
  switch (form) {
    case Util::FULL_WIDTH:
      if (type == Util::KATAKANA ||
          Util::IsFullWidthSymbolInHalfWidthKatakana(input)) {
        return Util::HalfWidthToFullWidth(input, output);
      }
      return Util::FullWidthToHalfWidth(input, output);
    case Util::HALF_WIDTH:
      return Util::HalfWidthToFullWidth(input, output);
    default:
      *output = input;
  }
}

CharacterFormManagerImpl::CharacterFormManagerImpl()
    : storage_(NULL), require_consistent_conversion_(false) {
}

CharacterFormManagerImpl::~CharacterFormManagerImpl() {}

Config::CharacterForm CharacterFormManagerImpl::GetCharacterForm(
    const string &str) const {
  const uint16 ucs2 = GetNormalizedCharacter(str);
  if (ucs2 == 0x0000) {
    return Config::NO_CONVERSION;
  }

  map<uint16, Config::CharacterForm>::const_iterator it =
      conversion_table_.find(ucs2);
  if (it == conversion_table_.end()) {
    return Config::NO_CONVERSION;
  }

  if (it->second == Config::LAST_FORM) {
    return GetCharacterFormFromStorage(ucs2);
  }

  return it->second;
}

void CharacterFormManagerImpl::ClearHistory() {
  if (storage_ != NULL) {
    storage_->Clear();
  }
}

// TODO(taku): need to chunk str
void CharacterFormManagerImpl::GuessAndSetCharacterForm(const string &str) {
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

void CharacterFormManagerImpl::SetCharacterForm(
    const string &str, Config::CharacterForm form) {
  const uint16 ucs2 = GetNormalizedCharacter(str);
  if (ucs2 == 0x0000) {
    return;
  }

  map<uint16, Config::CharacterForm>::const_iterator it =
      conversion_table_.find(ucs2);
  if (it == conversion_table_.end()) {
    return;
  }

  if (it->second == Config::LAST_FORM) {
    SaveCharacterFormToStorage(ucs2, form);
    return;
  }
}

Config::CharacterForm CharacterFormManagerImpl::GetCharacterFormFromStorage(
    uint16 ucs2) const {
  if (storage_ == NULL) {
    return Config::FULL_WIDTH;  // Return default setting
  }
  const string key(reinterpret_cast<const char *>(&ucs2), sizeof(ucs2));
  const char *value = storage_->Lookup(key);
  if (value == NULL) {
    return Config::FULL_WIDTH;  // Return default setting
  }
  const uint32 ivalue = *reinterpret_cast<const uint32 *>(value);
  return static_cast<Config::CharacterForm>(ivalue);
}

void CharacterFormManagerImpl::SaveCharacterFormToStorage(
    uint16 ucs2, Config::CharacterForm form) {
  if (form != Config::FULL_WIDTH && form != Config::HALF_WIDTH) {
    return;
  }

  if (storage_ == NULL) {
    return;
  }

  const string key(reinterpret_cast<const char *>(&ucs2), sizeof(ucs2));
  const char *value = storage_->Lookup(key);
  if (value != NULL && static_cast<Config::CharacterForm>(*value) == form) {
    return;
  }

  // Do cast since CharacterForm may not be 32 bit
  const uint32 iform = static_cast<uint32>(form);

  map<uint16, vector<uint16> >::iterator iter = group_table_.find(ucs2);
  if (iter == group_table_.end()) {
    storage_->Insert(key, reinterpret_cast<const char *>(&iform));
  } else {
    // Update values in the same group.
    const vector<uint16> &group = iter->second;
    for (size_t i = 0; i < group.size(); ++i) {
      const uint16 group_ucs2 = group[i];
      const string group_key(reinterpret_cast<const char *>(&group_ucs2),
                             sizeof(group_ucs2));
      storage_->Insert(group_key, reinterpret_cast<const char *>(&iform));
    }
  }
  VLOG(2) << ucs2 << " is stored to " << kFileName << " as " << form;
}

void CharacterFormManagerImpl::ConvertString(const string &str,
                                             string *output) const {
  ConvertStringWithAlternative(str, output, NULL);
}

bool CharacterFormManagerImpl::TryConvertStringWithPreference(
    const string &str, string *output) const {
  DCHECK(output);
  const char *begin = str.data();
  const char *end = begin + str.size();
  Config::CharacterForm target_form = Config::NO_CONVERSION;
  Config::CharacterForm prev_form = Config::NO_CONVERSION;
  Util::ScriptType prev_type = Util::UNKNOWN_SCRIPT;
  bool ret = true;

  string buf;
  while (begin < end) {
    // TODO(team): Replace by iterator.
    size_t mblen = 0;
    const uint16 ucs2 = Util::UTF8ToUCS2(begin, end,  &mblen);
    const Util::ScriptType type = Util::GetScriptType(ucs2);
    // Cache previous ScriptType to reduce to call GetCharacterForm()
    Config::CharacterForm form = prev_form;
    const string current(begin, mblen);
    if ((type == Util::UNKNOWN_SCRIPT) ||
        (type == Util::KATAKANA && prev_type != Util::KATAKANA) ||
        (type == Util::NUMBER   && prev_type != Util::NUMBER) ||
        (type == Util::ALPHABET && prev_type != Util::ALPHABET)) {
      form = GetCharacterForm(current);
    } else if (type == Util::KANJI || type == Util::HIRAGANA) {
      form = Config::NO_CONVERSION;
    }

    // Cache previous Form to reduce to call ConvertToFullWidthOrHalf
    if (begin != str.data() && prev_form != form) {
      string tmp;
      CharacterFormManager::ConvertWidth(buf, &tmp, prev_form);
      *output += tmp;
      buf.clear();
    }

    if (target_form == Config::NO_CONVERSION) {
      target_form = form;
    } else if (form != Config::NO_CONVERSION && form != target_form) {
      ret = false;
    }
    buf += current;
    prev_type = type;
    prev_form = form;
    begin += mblen;
  }

  if (!buf.empty()) {
    string tmp;
    CharacterFormManager::ConvertWidth(buf, &tmp, prev_form);
    *output += tmp;
  }

  return ret;
}

void CharacterFormManagerImpl::ConvertStringAlternative(
    const string &str, string *output) const {
  DCHECK(output);
  const char *begin = str.c_str();
  const char *end = str.c_str() + str.size();
  Util::FormType prev_form = Util::UNKNOWN_FORM;
  Util::ScriptType prev_type = Util::UNKNOWN_SCRIPT;

  string buf;
  while (begin < end) {
    size_t mblen = 0;
    const uint16 ucs2 = Util::UTF8ToUCS2(begin, end,  &mblen);
    const Util::ScriptType type = Util::GetScriptType(ucs2);
    // Cache previous ScriptType to reduce to call GetFormType()
    Util::FormType form = prev_form;
    const string current(begin, mblen);

    if ((type == Util::UNKNOWN_SCRIPT) ||
        (type == Util::KATAKANA && prev_type != Util::KATAKANA) ||
        (type == Util::NUMBER   && prev_type != Util::NUMBER) ||
        (type == Util::ALPHABET && prev_type != Util::ALPHABET)) {
      form = Util::GetFormType(current);
    } else if (type == Util::KANJI || type == Util::HIRAGANA) {
      form = Util::UNKNOWN_FORM;
    }

    // Cache previous Form to reduce to call ConvertToFullWidthOrHalf
    if (begin != str.c_str() && prev_form != form) {
      string tmp;
      ConvertToAlternative(buf, &tmp, prev_form, prev_type);
      *output += tmp;
      buf.clear();
    }

    buf += current;
    prev_type = type;
    prev_form = form;
    begin += mblen;
  }

  if (!buf.empty()) {
    string tmp;
    ConvertToAlternative(buf, &tmp, prev_form, prev_type);
    *output += tmp;
  }
}

bool CharacterFormManagerImpl::ConvertStringWithAlternative(
    const string &str,
    string *output, string *alternative_output) const {
  // If require_consistent_conversion_ is true,
  // do not convert to inconsistent form string.
  DCHECK(output);
  output->clear();
  if (!TryConvertStringWithPreference(str, output) &&
      require_consistent_conversion_) {
    *output = str;
  }

  if (alternative_output != NULL) {
    alternative_output->clear();
    ConvertStringAlternative(*output, alternative_output);
  }

  // return true if alternative_output and output are different
  return (alternative_output != NULL && *alternative_output != *output);
}

void CharacterFormManagerImpl::Clear() {
  conversion_table_.clear();
  group_table_.clear();
}

void CharacterFormManagerImpl::AddRule(
    const string &key, Config::CharacterForm form) {
  const char *begin = key.c_str();
  const char *end = key.c_str() + key.size();

  vector<uint16> group;
  while (begin < end) {
    const size_t mblen = Util::OneCharLen(begin);
    const string tmp(begin, mblen);
    const uint16 ucs2 = GetNormalizedCharacter(tmp);
    if (ucs2 != 0x0000) {
      group.push_back(ucs2);
    }
    begin += mblen;
  }

  if (group.empty()) {
    return;
  }

  const size_t kMaxGroupSize = 128;
  if (group.size() > kMaxGroupSize) {
    LOG(WARNING) << "Too long rule. skipped";
    return;
  }

  const size_t kMaxTableSize = 256;
  if (conversion_table_.size() + group.size() > kMaxTableSize ||
      group_table_.size() + group.size() > kMaxTableSize) {
    LOG(WARNING) << "conversion_table becomes too big. skipped";
    return;
  }

  VLOG(2) << "Adding Rule: " << key << " " << form;

  // sort + unique
  // use vector because set is slower.
  // group table is used in SaveCharacterFormToStorage and this will be called
  // everytime user submits conversion.
  sort(group.begin(), group.end());
  vector<uint16>::iterator last = unique(group.begin(), group.end());
  group.erase(last, group.end());

  for (size_t i = 0; i < group.size(); ++i) {
    const uint16 ucs2 = group[i];
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
  ~Data() {
  }

  CharacterFormManagerImpl *GetPreeditManager() {
    return preedit_.get();
  }
  CharacterFormManagerImpl *GetConversionManager() {
    return conversion_.get();
  }

 private:
  scoped_ptr<PreeditCharacterFormManagerImpl> preedit_;
  scoped_ptr<ConversionCharacterFormManagerImpl> conversion_;
  scoped_ptr<LRUStorage> storage_;
};

CharacterFormManager::Data::Data() {
  const string filename = ConfigFileStream::GetFileName(kFileName);
  const uint32 key_type = 0;
  storage_.reset(LRUStorage::Create(filename.c_str(),
                                    sizeof(key_type),
                                    kLRUSize,
                                    kSeedValue));
  LOG_IF(ERROR, storage_.get() == NULL)
      << "cannot open " << filename;
  preedit_.reset(new PreeditCharacterFormManagerImpl);
  conversion_.reset(new ConversionCharacterFormManagerImpl);
  preedit_->set_storage(storage_.get());
  conversion_->set_storage(storage_.get());
}

CharacterFormManager *CharacterFormManager::GetCharacterFormManager() {
  return Singleton<CharacterFormManager>::get();
}

CharacterFormManager::CharacterFormManager() : data_(new Data) {
  Reload();
}

CharacterFormManager::~CharacterFormManager() {
}

void CharacterFormManager::Reload() {
  Clear();
  const Config &config = ConfigHandler::GetConfig();

  if (config.character_form_rules_size() > 0) {
    for (size_t i = 0; i < config.character_form_rules_size(); ++i) {
      const string &group = config.character_form_rules(i).group();
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

void CharacterFormManager::ConvertWidth(
    const string &input, string *output, Config::CharacterForm form) {
  if (form == Config::FULL_WIDTH) {
    Util::HalfWidthToFullWidth(input, output);
    return;
  } else if (form == Config::HALF_WIDTH) {
    Util::FullWidthToHalfWidth(input, output);
    return;
  };

  *output = input;
}

void CharacterFormManager::ConvertPreeditString(const string &input,
                                                string *output) const {
  data_->GetPreeditManager()->ConvertString(input, output);
}

void CharacterFormManager::ConvertConversionString(const string &input,
                                                   string *output) const {
  data_->GetConversionManager()->ConvertString(input, output);
}

bool CharacterFormManager::ConvertPreeditStringWithAlternative(
    const string &input, string *output, string *alternative_output) const {
  return data_->GetPreeditManager()->ConvertStringWithAlternative(
      input,
      output, alternative_output);
}

bool CharacterFormManager::ConvertConversionStringWithAlternative(
    const string &input, string *output, string *alternative_output) const {
  return data_->GetConversionManager()->ConvertStringWithAlternative(
      input,
      output, alternative_output);
}

Config::CharacterForm CharacterFormManager::GetPreeditCharacterForm(
    const string &input) const {
  return data_->GetPreeditManager()->GetCharacterForm(input);
}

Config::CharacterForm CharacterFormManager::GetConversionCharacterForm(
    const string &input) const {
  return data_->GetConversionManager()->GetCharacterForm(input);
}

void CharacterFormManager::ClearHistory() {
  // no need to call, as storage is shared
  // GetPreeditManager()->ClearHistory();
  VLOG(1) << "CharacterFormManager::ClearHistory() is called";
  data_->GetConversionManager()->ClearHistory();
}

void CharacterFormManager::Clear() {
  VLOG(1) << "CharacterFormManager::Clear() is called";
  data_->GetConversionManager()->Clear();
  data_->GetPreeditManager()->Clear();
}

void CharacterFormManager::SetCharacterForm(
    const string &input, Config::CharacterForm form) {
  // no need to call Preedit, as storage is shared
  // GetPreeditManager()->SetCharacterForm(input, form);
  data_->GetConversionManager()->SetCharacterForm(input, form);
}

void CharacterFormManager::GuessAndSetCharacterForm(const string &input) {
  // no need to call Preedit, as storage is shared
  // GetPreeditManager()->SetCharacterForm(input, form);
  data_->GetConversionManager()->GuessAndSetCharacterForm(input);
}

void CharacterFormManager::AddPreeditRule(
    const string &input, Config::CharacterForm form) {
  data_->GetPreeditManager()->AddRule(input, form);
}

void CharacterFormManager::AddConversionRule(
    const string &input, Config::CharacterForm form) {
  data_->GetConversionManager()->AddRule(input, form);
}

void CharacterFormManager::SetDefaultRule() {
  data_->GetPreeditManager()->SetDefaultRule();
  data_->GetConversionManager()->SetDefaultRule();
}

namespace {
// Almost the same as UTF8ToUCS2, but skip halfwidth
// voice/semi-voice sound mark as they are treated as one character.
uint16 SkipHalfWidthVoiceSoundMark(const char *begin,
                                   const char *end,
                                   size_t *mblen) {
  uint16 w = 0;
  *mblen = 0;
  while (begin < end) {
    size_t tmp_mblen = 0;
    w = Util::UTF8ToUCS2(begin, end, &tmp_mblen);
    CHECK_GT(tmp_mblen, 0);
    *mblen += tmp_mblen;
    begin += tmp_mblen;
    // 0xFF9E: Halfwidth voice sound mark
    // 0xFF9F: Halfwidth semi-voice sound mark
    if (w != 0xFF9E && w != 0xFF9F) {
      break;
    }
  }

  return w;
}
}   // namespace

bool CharacterFormManager::GetFormTypesFromStringPair(
    const string &input1, FormType *output_form1,
    const string &input2, FormType *output_form2) {
  CHECK(output_form1);
  CHECK(output_form2);

  *output_form1 = CharacterFormManager::UNKNOWN_FORM;
  *output_form2 = CharacterFormManager::UNKNOWN_FORM;

  if (input1.empty() || input2.empty()) {
    return false;
  }

  const char *begin1 = input1.data();
  const char *end1   = input1.data() + input1.size();
  const char *begin2 = input2.data();
  const char *end2   = input2.data() + input2.size();

  while (begin1 < end1 && begin2 < end2) {
    size_t mblen1 = 0;
    size_t mblen2 = 0;
    const uint16 w1 = SkipHalfWidthVoiceSoundMark(begin1, end1, &mblen1);
    const uint16 w2 = SkipHalfWidthVoiceSoundMark(begin2, end2, &mblen2);
    CHECK_GT(mblen1, 0);
    CHECK_GT(mblen2, 0);
    begin1 += mblen1;
    begin2 += mblen2;

    const Util::ScriptType script1 = Util::GetScriptType(w1);
    const Util::ScriptType script2 = Util::GetScriptType(w2);
    const Util::FormType form1 = Util::GetFormType(w1);
    const Util::FormType form2 = Util::GetFormType(w2);

    // TODO(taku): have to check that normalized w1 and w2 are identical
    if (script1 != script2) {
      return false;
    }

    DCHECK_EQ(script1, script2);

    // when having different forms, record the diff.
    if (form1 == Util::FULL_WIDTH && form2 == Util::HALF_WIDTH) {
      if (*output_form1 == CharacterFormManager::HALF_WIDTH ||
          *output_form2 == CharacterFormManager::FULL_WIDTH) {
        // inconsitent with the previous forms.
        return false;
      }
      *output_form1 = CharacterFormManager::FULL_WIDTH;
      *output_form2 = CharacterFormManager::HALF_WIDTH;
    } else if (form1 == Util::HALF_WIDTH && form2 == Util::FULL_WIDTH) {
      if (*output_form1 == CharacterFormManager::FULL_WIDTH ||
          *output_form2 == CharacterFormManager::HALF_WIDTH) {
        // inconsitent with the previous forms.
        return false;
      }
      *output_form1 = CharacterFormManager::HALF_WIDTH;
      *output_form2 = CharacterFormManager::FULL_WIDTH;
    }
  }

  // length should be the same
  if (begin1 != end1 || begin2 != end2) {
    return false;
  }

  if (*output_form1 == CharacterFormManager::UNKNOWN_FORM ||
      *output_form2 == CharacterFormManager::UNKNOWN_FORM) {
    return false;
  }

  return true;
}

}  // namespace config
}  // namespace mozc
