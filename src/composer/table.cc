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

// Trie table for Romaji (or Kana) conversion

#include "composer/table.h"

#include <cstdint>
#include <istream>  // NOLINT
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/trie.h"
#include "base/util.h"
#include "composer/internal/typing_model.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace composer {
namespace {
constexpr char kDefaultPreeditTableFile[] = "system://romanji-hiragana.tsv";
constexpr char kRomajiPreeditTableFile[] = "system://romanji-hiragana.tsv";
// Table for Kana combinations like "か゛" → "が".
constexpr char kKanaCombinationTableFile[] = "system://kana.tsv";

// Special tables for 12keys
const char k12keysHiraganaTableFile[] = "system://12keys-hiragana.tsv";
const char k12keysHiraganaIntuitiveTableFile[] =
    "system://12keys-hiragana_intuitive.tsv";
const char k12keysHalfwidthasciiTableFile[] =
    "system://12keys-halfwidthascii.tsv";
constexpr char kFlickHiraganaTableFile[] = "system://flick-hiragana.tsv";
constexpr char kFlickHalfwidthasciiIosTableFile[] =
    "system://flick-halfwidthascii_ios.tsv";
constexpr char kFlickNumberTableFile[] = "system://flick-number.tsv";
constexpr char kFlickHiraganaIntuitiveTableFile[] =
    "system://flick-hiragana_intuitive.tsv";
constexpr char kFlickHalfwidthasciiTableFile[] =
    "system://flick-halfwidthascii.tsv";
constexpr char kToggleFlickHiraganaTableFile[] =
    "system://toggle_flick-hiragana.tsv";
constexpr char kToggleFlickHiraganaIntuitiveTableFile[] =
    "system://toggle_flick-hiragana_intuitive.tsv";
constexpr char kToggleFlickHalfwidthasciiIosTableFile[] =
    "system://toggle_flick-halfwidthascii_ios.tsv";
constexpr char kToggleFlickNumberTableFile[] =
    "system://toggle_flick-number.tsv";
constexpr char kToggleFlickHalfwidthasciiTableFile[] =
    "system://toggle_flick-halfwidthascii.tsv";
// Special tables for QWERTY mobile
constexpr char kQwertyMobileHiraganaTableFile[] =
    "system://qwerty_mobile-hiragana.tsv";
constexpr char kQwertyMobileHalfwidthasciiTableFile[] =
    "system://qwerty_mobile-halfwidthascii.tsv";
// Special tables for Godan
constexpr char kGodanHiraganaTableFile[] = "system://godan-hiragana.tsv";
constexpr char kNotouchHiraganaTableFile[] = "system://notouch-hiragana.tsv";
// Reuse qwerty_mobile-halfwidthascii table
constexpr char kNotouchHalfwidthasciiTableFile[] =
    "system://qwerty_mobile-halfwidthascii.tsv";

constexpr char kNewChunkPrefix[] = "\t";
constexpr char kSpecialKeyOpen[] = "\x0F";   // Shift-In of ASCII
constexpr char kSpecialKeyClose[] = "\x0E";  // Shift-Out of ASCII
}  // namespace

// ========================================
// Entry
// ========================================
Entry::Entry(const std::string &input, const std::string &result,
             const std::string &pending, const TableAttributes attributes)
    : input_(input),
      result_(result),
      pending_(pending),
      attributes_(attributes) {}

// ========================================
// Table
// ========================================
Table::Table() : entries_(new EntryTrie), case_sensitive_(false) {}

Table::~Table() { ResetEntrySet(); }

static constexpr char kKuten[] = "、";
static constexpr char kTouten[] = "。";
static constexpr char kComma[] = "，";
static constexpr char kPeriod[] = "．";

static constexpr char kCornerOpen[] = "「";
static constexpr char kCornerClose[] = "」";
static constexpr char kSlash[] = "／";
static constexpr char kSquareOpen[] = "[";
static constexpr char kSquareClose[] = "]";
static constexpr char kMiddleDot[] = "・";

bool Table::InitializeWithRequestAndConfig(
    const commands::Request &request, const config::Config &config,
    const DataManagerInterface &data_manager) {
  case_sensitive_ = false;
  bool result = false;
  typing_model_ = TypingModel::CreateTypingModel(
      request.special_romanji_table(), data_manager);
  if (request.special_romanji_table() !=
      mozc::commands::Request::DEFAULT_TABLE) {
    const char *table_file_name;
    switch (request.special_romanji_table()) {
      case mozc::commands::Request::TWELVE_KEYS_TO_HIRAGANA:
        table_file_name = k12keysHiraganaTableFile;
        break;
      case mozc::commands::Request::TWELVE_KEYS_TO_HIRAGANA_INTUITIVE:
        table_file_name = k12keysHiraganaIntuitiveTableFile;
        break;
      case mozc::commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII:
        table_file_name = k12keysHalfwidthasciiTableFile;
        break;
      case mozc::commands::Request::FLICK_TO_HIRAGANA:
        table_file_name = kFlickHiraganaTableFile;
        break;
      case mozc::commands::Request::FLICK_TO_HIRAGANA_INTUITIVE:
        table_file_name = kFlickHiraganaIntuitiveTableFile;
        break;
      case mozc::commands::Request::FLICK_TO_HALFWIDTHASCII_IOS:
        table_file_name = kFlickHalfwidthasciiIosTableFile;
        break;
      case mozc::commands::Request::FLICK_TO_HALFWIDTHASCII:
        table_file_name = kFlickHalfwidthasciiTableFile;
        break;
      case mozc::commands::Request::FLICK_TO_NUMBER:
        table_file_name = kFlickNumberTableFile;
        break;
      case mozc::commands::Request::TOGGLE_FLICK_TO_HIRAGANA:
        table_file_name = kToggleFlickHiraganaTableFile;
        break;
      case mozc::commands::Request::TOGGLE_FLICK_TO_HIRAGANA_INTUITIVE:
        table_file_name = kToggleFlickHiraganaIntuitiveTableFile;
        break;
      case mozc::commands::Request::TOGGLE_FLICK_TO_HALFWIDTHASCII_IOS:
        table_file_name = kToggleFlickHalfwidthasciiIosTableFile;
        break;
      case mozc::commands::Request::TOGGLE_FLICK_TO_NUMBER:
        table_file_name = kToggleFlickNumberTableFile;
        break;
      case mozc::commands::Request::TOGGLE_FLICK_TO_HALFWIDTHASCII:
        table_file_name = kToggleFlickHalfwidthasciiTableFile;
        break;
      case mozc::commands::Request::QWERTY_MOBILE_TO_HIRAGANA:
        // This table is almost as same as "romaji-hiragana.tsv",
        // and the diff should be only the behavior of ','.
        // So probably we'd like to share the table, but we keep this way
        // for now, as this is still internal code.
        // TODO(hidehiko): refactor this code to clean up.
        table_file_name = kQwertyMobileHiraganaTableFile;
        break;
      case mozc::commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII:
        table_file_name = kQwertyMobileHalfwidthasciiTableFile;
        break;
      case mozc::commands::Request::GODAN_TO_HIRAGANA:
        table_file_name = kGodanHiraganaTableFile;
        break;
      case mozc::commands::Request::NOTOUCH_TO_HIRAGANA:
        table_file_name = kNotouchHiraganaTableFile;
        break;
      case mozc::commands::Request::NOTOUCH_TO_HALFWIDTHASCII:
        table_file_name = kNotouchHalfwidthasciiTableFile;
        break;
      default:
        table_file_name = nullptr;
    }
    if (table_file_name && LoadFromFile(table_file_name)) {
      return true;
    }
  }
  switch (config.preedit_method()) {
    case config::Config::ROMAN:
      result = (config.has_custom_roman_table() &&
                !config.custom_roman_table().empty())
                   ? LoadFromString(config.custom_roman_table())
                   : LoadFromFile(kRomajiPreeditTableFile);
      break;
    case config::Config::KANA:
      result = LoadFromFile(kRomajiPreeditTableFile);
      break;
    default:
      LOG(ERROR) << "Unkonwn preedit method: " << config.preedit_method();
      break;
  }

  if (!result) {
    result = LoadFromFile(kDefaultPreeditTableFile);
    if (!result) {
      return false;
    }
  }

  // Initialize punctuations.
  const config::Config::PunctuationMethod punctuation_method =
      config.punctuation_method();
  const mozc::composer::Entry *entry = nullptr;

  // Comma / Kuten
  entry = LookUp(",");
  if (entry == nullptr ||
      (entry->result() == kKuten && entry->pending().empty())) {
    if (punctuation_method == config::Config::COMMA_PERIOD ||
        punctuation_method == config::Config::COMMA_TOUTEN) {
      AddRule(",", kComma, "");
    } else {
      AddRule(",", kKuten, "");
    }
  }

  // Period / Touten
  entry = LookUp(".");
  if (entry == nullptr ||
      (entry->result() == kTouten && entry->pending().empty())) {
    if (punctuation_method == config::Config::COMMA_PERIOD ||
        punctuation_method == config::Config::KUTEN_PERIOD) {
      AddRule(".", kPeriod, "");
    } else {
      AddRule(".", kTouten, "");
    }
  }

  // Initialize symbols.
  const config::Config::SymbolMethod symbol_method = config.symbol_method();

  // Slash / Middle dot
  entry = LookUp("/");
  if (entry == nullptr ||
      (entry->result() == kMiddleDot && entry->pending().empty())) {
    if (symbol_method == config::Config::SQUARE_BRACKET_SLASH ||
        symbol_method == config::Config::CORNER_BRACKET_SLASH) {
      AddRule("/", kSlash, "");
    } else {
      AddRule("/", kMiddleDot, "");
    }
  }

  // Square open bracket / Corner open bracket
  entry = LookUp("[");
  if (entry == nullptr ||
      (entry->result() == kCornerOpen && entry->pending().empty())) {
    if (symbol_method == config::Config::CORNER_BRACKET_MIDDLE_DOT ||
        symbol_method == config::Config::CORNER_BRACKET_SLASH) {
      AddRule("[", kCornerOpen, "");
    } else {
      AddRule("[", kSquareOpen, "");
    }
  }

  // Square close bracket / Corner close bracket
  entry = LookUp("]");
  if (entry == nullptr ||
      (entry->result() == kCornerClose && entry->pending().empty())) {
    if (symbol_method == config::Config::CORNER_BRACKET_MIDDLE_DOT ||
        symbol_method == config::Config::CORNER_BRACKET_SLASH) {
      AddRule("]", kCornerClose, "");
    } else {
      AddRule("]", kSquareClose, "");
    }
  }

  // result should be true here.
  CHECK(result);

  // Load Kana combination rules.
  result = LoadFromFile(kKanaCombinationTableFile);
  return result;
}

bool Table::IsLoopingEntry(const std::string &input,
                           const std::string &pending) const {
  if (input.empty() || pending.empty()) {
    return false;
  }

  std::string key = pending;
  do {
    // If input is a prefix of key, it should be looping.
    // (ex. input="a", pending="abc").
    if (Util::StartsWith(key, input)) {
      return true;
    }

    size_t key_length = 0;
    bool fixed = false;
    const Entry *entry = LookUpPrefix(key, &key_length, &fixed);
    if (entry == nullptr) {
      return false;
    }
    DCHECK_LE(key_length, key.size());
    key = entry->pending() + key.substr(key_length);
  } while (!key.empty());

  return false;
}

const Entry *Table::AddRule(const std::string &input, const std::string &output,
                            const std::string &pending) {
  return AddRuleWithAttributes(input, output, pending, NO_TABLE_ATTRIBUTE);
}

const Entry *Table::AddRuleWithAttributes(const std::string &escaped_input,
                                          const std::string &output,
                                          const std::string &escaped_pending,
                                          const TableAttributes attributes) {
  if (attributes & NEW_CHUNK) {
    // TODO(komatsu): Make a new trie tree for checking the new chunk
    // attribute rather than reusing the conversion trie.
    const std::string additional_input = kNewChunkPrefix + escaped_input;
    AddRuleWithAttributes(additional_input, output, escaped_pending,
                          NO_TABLE_ATTRIBUTE);
  }

  const size_t kMaxSize = 300;
  if (escaped_input.size() >= kMaxSize || output.size() >= kMaxSize ||
      escaped_pending.size() >= kMaxSize) {
    LOG(ERROR) << "Invalid input/output/pending";
    return nullptr;
  }

  const std::string input = ParseSpecialKey(escaped_input);
  const std::string pending = ParseSpecialKey(escaped_pending);
  if (IsLoopingEntry(input, pending)) {
    LOG(WARNING) << "Entry " << input << " " << output << " " << pending
                 << " is removed, since the rule is looping";
    return nullptr;
  }

  const Entry *old_entry = nullptr;
  if (entries_->LookUp(input, &old_entry)) {
    DeleteEntry(old_entry);
  }

  Entry *entry = new Entry(input, output, pending, attributes);
  entries_->AddEntry(input, entry);
  entry_set_.insert(entry);

  // Check if the input has a large captal character.
  // Invisible character is exception.
  if (!case_sensitive_) {
    const std::string trimed_input = DeleteSpecialKey(input);
    for (ConstChar32Iterator iter(trimed_input); !iter.Done(); iter.Next()) {
      const char32 ucs4 = iter.Get();
      if ('A' <= ucs4 && ucs4 <= 'Z') {
        case_sensitive_ = true;
        break;
      }
    }
  }
  return entry;
}

void Table::DeleteRule(const std::string &input) {
  // NOTE : If this method is called and an entry is deleted,
  //     case_sensitive_ turns to be invalid
  //     because it is not updated.
  //     Currently updating logic is omitted because;
  //     - Updating needs some complex implementation.
  //     - This method is not used.
  //     - This method has no tests.
  //     - This method is private scope.
  const Entry *old_entry;
  if (entries_->LookUp(input, &old_entry)) {
    DeleteEntry(old_entry);
  }
  entries_->DeleteEntry(input);
}

bool Table::LoadFromString(const std::string &str) {
  std::istringstream is(str);
  return LoadFromStream(&is);
}

bool Table::LoadFromFile(const char *filepath) {
  std::unique_ptr<std::istream> ifs(ConfigFileStream::LegacyOpen(filepath));
  if (ifs == nullptr) {
    return false;
  }
  return LoadFromStream(ifs.get());
}

const TypingModel *Table::typing_model() const { return typing_model_.get(); }

namespace {
constexpr char kAttributeDelimiter[] = " ";

TableAttributes ParseAttributes(const std::string &input) {
  TableAttributes attributes = NO_TABLE_ATTRIBUTE;

  std::vector<std::string> attribute_strings;
  Util::SplitStringAllowEmpty(input, kAttributeDelimiter, &attribute_strings);

  for (size_t i = 0; i < attribute_strings.size(); ++i) {
    if (attribute_strings[i] == "NewChunk") {
      attributes |= NEW_CHUNK;
    } else if (attribute_strings[i] == "NoTransliteration") {
      attributes |= NO_TRANSLITERATION;
    } else if (attribute_strings[i] == "DirectInput") {
      attributes |= DIRECT_INPUT;
    } else if (attribute_strings[i] == "EndChunk") {
      attributes |= END_CHUNK;
    }
  }
  return attributes;
}
}  // namespace

bool Table::LoadFromStream(std::istream *is) {
  DCHECK(is);
  std::string line;
  const std::string empty_pending("");
  while (!is->eof()) {
    std::getline(*is, line);
    Util::ChopReturns(&line);
    if (line.empty()) {
      continue;
    }

    std::vector<std::string> rules;
    Util::SplitStringAllowEmpty(line, "\t", &rules);
    if (rules.size() == 4) {
      const TableAttributes attributes = ParseAttributes(rules[3]);
      AddRuleWithAttributes(rules[0], rules[1], rules[2], attributes);
    } else if (rules.size() == 3) {
      AddRule(rules[0], rules[1], rules[2]);
    } else if (rules.size() == 2) {
      AddRule(rules[0], rules[1], empty_pending);
    } else {
      if (line[0] != '#') {
        LOG(ERROR) << "Format error: " << line;
      }
      continue;
    }
  }

  return true;
}

const Entry *Table::LookUp(const std::string &input) const {
  const Entry *entry = nullptr;
  if (case_sensitive_) {
    entries_->LookUp(input, &entry);
  } else {
    std::string normalized_input = input;
    Util::LowerString(&normalized_input);
    entries_->LookUp(normalized_input, &entry);
  }
  return entry;
}

const Entry *Table::LookUpPrefix(const std::string &input, size_t *key_length,
                                 bool *fixed) const {
  const Entry *entry = nullptr;
  if (case_sensitive_) {
    entries_->LookUpPrefix(input, &entry, key_length, fixed);
  } else {
    std::string normalized_input = input;
    Util::LowerString(&normalized_input);
    entries_->LookUpPrefix(normalized_input, &entry, key_length, fixed);
  }
  return entry;
}

void Table::LookUpPredictiveAll(const std::string &input,
                                std::vector<const Entry *> *results) const {
  if (case_sensitive_) {
    entries_->LookUpPredictiveAll(input, results);
  } else {
    std::string normalized_input = input;
    Util::LowerString(&normalized_input);
    entries_->LookUpPredictiveAll(normalized_input, results);
  }
}

bool Table::HasNewChunkEntry(const std::string &input) const {
  if (input.empty()) {
    return false;
  }

  const std::string key = kNewChunkPrefix + input;
  size_t key_length = 0;
  bool fixed = false;
  LookUpPrefix(key, &key_length, &fixed);
  if (key_length > 1) {
    return true;
  }

  return false;
}

bool Table::HasSubRules(const std::string &input) const {
  if (case_sensitive_) {
    return entries_->HasSubTrie(input);
  } else {
    std::string normalized_input = input;
    Util::LowerString(&normalized_input);
    return entries_->HasSubTrie(normalized_input);
  }
}

void Table::DeleteEntry(const Entry *entry) {
  entry_set_.erase(entry);
  delete entry;
}

void Table::ResetEntrySet() {
  for (EntrySet::iterator it = entry_set_.begin(); it != entry_set_.end();
       ++it) {
    delete *it;
  }
  entry_set_.clear();
}

bool Table::case_sensitive() const { return case_sensitive_; }

void Table::set_case_sensitive(const bool case_sensitive) {
  case_sensitive_ = case_sensitive;
}

namespace {
bool FindBlock(const std::string &input, const std::string &open,
               const std::string &close, const size_t offset, size_t *open_pos,
               size_t *close_pos) {
  DCHECK(open_pos);
  DCHECK(close_pos);

  *open_pos = input.find(open, offset);
  if (*open_pos == std::string::npos) {
    return false;
  }

  *close_pos = input.find(close, *open_pos);
  if (*close_pos == std::string::npos) {
    return false;
  }

  return true;
}
}  // namespace

// static
std::string Table::ParseSpecialKey(const std::string &input) {
  std::string output;
  size_t open_pos = 0;
  size_t close_pos = 0;
  for (size_t cursor = 0; cursor < input.size();) {
    if (!FindBlock(input, "{", "}", cursor, &open_pos, &close_pos)) {
      output.append(input, cursor, input.size() - cursor);
      break;
    }

    output.append(input, cursor, open_pos - cursor);

    // The both sizes of "{" and "}" is 1.
    const std::string key(input, open_pos + 1, close_pos - open_pos - 1);
    if (key == "{") {
      // "{{}" is treated as "{".
      output.append("{");
    } else {
      // "{abc}" is replaced with "<kSpecialKeyOpen>abc<kSpecialKeyClose>".
      output.append(kSpecialKeyOpen);
      output.append(key);
      output.append(kSpecialKeyClose);
    }

    cursor = close_pos + 1;
  }
  return output;
}

// static
std::string Table::DeleteSpecialKey(const std::string &input) {
  std::string output;
  size_t open_pos = 0;
  size_t close_pos = 0;
  for (size_t cursor = 0; cursor < input.size();) {
    if (!FindBlock(input, kSpecialKeyOpen, kSpecialKeyClose, cursor, &open_pos,
                   &close_pos)) {
      output.append(input, cursor, input.size() - cursor);
      break;
    }

    output.append(input, cursor, open_pos - cursor);
    // The size of kSpecialKeyClose is 1.
    cursor = close_pos + 1;
  }
  return output;
}

// static
const Table &Table::GetDefaultTable() {
  static Table *default_table = nullptr;
  if (!default_table) {
    default_table = new Table();
  }
  return *default_table;
}

// ========================================
// TableContainer
// ========================================
TableManager::TableManager()
    : custom_roman_table_fingerprint_(Hash::Fingerprint32("")) {}

TableManager::~TableManager() = default;

const Table *TableManager::GetTable(
    const mozc::commands::Request &request, const mozc::config::Config &config,
    const mozc::DataManagerInterface &data_manager) {
  // calculate the hash depending on the request and the config
  uint32_t hash = request.special_romanji_table();
  hash = hash * (mozc::config::Config_PreeditMethod_PreeditMethod_MAX + 1) +
         config.preedit_method();
  hash = hash * (mozc::config::Config_PunctuationMethod_PunctuationMethod_MAX +
                 1) +
         config.punctuation_method();
  hash = hash * (mozc::config::Config_SymbolMethod_SymbolMethod_MAX + 1) +
         config.symbol_method();

  // When custom_roman_table is set, force to create new table.
  bool update_custom_roman_table = false;
  if ((config.preedit_method() == config::Config::ROMAN) &&
      config.has_custom_roman_table() && !config.custom_roman_table().empty()) {
    const uint32_t custom_roman_table_fingerprint =
        Hash::Fingerprint32(config.custom_roman_table());
    if (custom_roman_table_fingerprint != custom_roman_table_fingerprint_) {
      update_custom_roman_table = true;
      custom_roman_table_fingerprint_ = custom_roman_table_fingerprint;
    }
  }

  const auto iterator = table_map_.find(hash);
  if (iterator != table_map_.end()) {
    if (update_custom_roman_table) {
      // Delete the previous table to update the table.
      table_map_.erase(iterator);
    } else {
      return iterator->second.get();
    }
  }

  std::unique_ptr<Table> table(new Table());
  if (!table->InitializeWithRequestAndConfig(request, config, data_manager)) {
    return nullptr;
  }

  const Table *ret = table.get();
  table_map_[hash] = std::move(table);
  return ret;
}

void TableManager::ClearCaches() { table_map_.clear(); }

}  // namespace composer
}  // namespace mozc
