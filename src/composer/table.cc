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

#include <cstddef>
#include <cstdint>
#include <istream>  // NOLINT
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/config_file_stream.h"
#include "base/hash.h"
#include "base/util.h"
#include "composer/special_key.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace composer {
namespace {

using ::mozc::composer::internal::DeleteSpecialKeys;

constexpr char kDefaultPreeditTableFile[] = "system://romanji-hiragana.tsv";
constexpr char kRomajiPreeditTableFile[] = "system://romanji-hiragana.tsv";
// Table for Kana combinations like "か゛" → "が".
constexpr char kKanaCombinationTableFile[] = "system://kana.tsv";

// Special tables for 12keys
constexpr char k12keysHiraganaTableFile[] = "system://12keys-hiragana.tsv";
constexpr char k12keysHiraganaIntuitiveTableFile[] =
    "system://12keys-hiragana_intuitive.tsv";
constexpr char k12keysHalfwidthasciiTableFile[] =
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
constexpr char k50KeysHiraganaTableFile[] = "system://50keys-hiragana.tsv";

constexpr char kNewChunkPrefix[] = "\t";

}  // namespace

// ========================================
// Entry
// ========================================
Entry::Entry(const absl::string_view input, const absl::string_view result,
             const absl::string_view pending, const TableAttributes attributes)
    : input_(input),
      result_(result),
      pending_(pending),
      attributes_(attributes) {}

// ========================================
// Table
// ========================================
Table::Table() {
  // Add predefined special keys.
  special_key_map_.Register("{?}");  // toggle
  special_key_map_.Register("{*}");  // internal state
  special_key_map_.Register("{<}");  // rewind
  special_key_map_.Register("{!}");  // timeout
}

constexpr absl::string_view kKuten = "、";
constexpr absl::string_view kTouten = "。";
constexpr absl::string_view kComma = "，";
constexpr absl::string_view kPeriod = "．";
constexpr absl::string_view kCornerOpen = "「";
constexpr absl::string_view kCornerClose = "」";
constexpr absl::string_view kSlash = "／";
constexpr absl::string_view kSquareOpen = "[";
constexpr absl::string_view kSquareClose = "]";
constexpr absl::string_view kMiddleDot = "・";

bool Table::InitializeWithRequestAndConfig(const commands::Request& request,
                                           const config::Config& config) {
  case_sensitive_ = false;
  bool result = false;
  if (request.special_romanji_table() !=
      mozc::commands::Request::DEFAULT_TABLE) {
    const char* table_file_name;
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
      case mozc::commands::Request::FIFTY_KEYS_TO_HIRAGANA:
        table_file_name = k50KeysHiraganaTableFile;
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
  const mozc::composer::Entry* entry = nullptr;

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

bool Table::IsLoopingEntry(const absl::string_view input,
                           const absl::string_view pending) const {
  if (input.empty() || pending.empty()) {
    return false;
  }

  std::string key(pending);
  do {
    // If input is a prefix of key, it should be looping.
    // (ex. input="a", pending="abc").
    if (key.starts_with(input)) {
      return true;
    }

    size_t key_length = 0;
    bool fixed = false;
    const Entry* entry = LookUpPrefix(key, &key_length, &fixed);
    if (entry == nullptr) {
      return false;
    }
    DCHECK_LE(key_length, key.size());
    key = absl::StrCat(entry->pending(), key.substr(key_length));
  } while (!key.empty());

  return false;
}

const Entry* absl_nullable Table::AddRule(const absl::string_view input,
                                          const absl::string_view output,
                                          const absl::string_view pending) {
  return AddRuleWithAttributes(input, output, pending, NO_TABLE_ATTRIBUTE);
}

const Entry* absl_nullable Table::AddRuleWithAttributes(
    const absl::string_view escaped_input, const absl::string_view output,
    const absl::string_view escaped_pending, const TableAttributes attributes) {
  if (attributes & NEW_CHUNK) {
    // TODO(komatsu): Make a new trie tree for checking the new chunk
    // attribute rather than reusing the conversion trie.
    const std::string additional_input =
        absl::StrCat(kNewChunkPrefix, escaped_input);
    AddRuleWithAttributes(additional_input, output, escaped_pending,
                          NO_TABLE_ATTRIBUTE);
  }

  constexpr size_t kMaxSize = 300;
  if (escaped_input.size() >= kMaxSize || output.size() >= kMaxSize ||
      escaped_pending.size() >= kMaxSize) {
    LOG(ERROR) << "Invalid input/output/pending";
    return nullptr;
  }

  const std::string input = special_key_map_.Register(escaped_input);
  const std::string pending = special_key_map_.Register(escaped_pending);
  if (IsLoopingEntry(input, pending)) {
    LOG(WARNING) << "Entry " << input << " " << output << " " << pending
                 << " is removed, since the rule is looping";
    return nullptr;
  }

  const Entry* old_entry = nullptr;
  if (entries_.LookUp(input, &old_entry)) {
    DeleteEntry(old_entry);
  }

  auto entry = std::make_unique<Entry>(input, output, pending, attributes);
  Entry* entry_ptr = entry.get();
  entries_.AddEntry(input, entry_ptr);
  entry_set_.insert(std::move(entry));

  // Check if the input has a large capital character.
  // Invisible character is exception.
  if (!case_sensitive_) {
    const std::string trimed_input = DeleteSpecialKeys(input);
    for (ConstChar32Iterator iter(trimed_input); !iter.Done(); iter.Next()) {
      const char32_t codepoint = iter.Get();
      if ('A' <= codepoint && codepoint <= 'Z') {
        case_sensitive_ = true;
        break;
      }
    }
  }
  return entry_ptr;
}

void Table::DeleteRule(const absl::string_view input) {
  // NOTE : If this method is called and an entry is deleted,
  //     case_sensitive_ turns to be invalid
  //     because it is not updated.
  //     Currently updating logic is omitted because;
  //     - Updating needs some complex implementation.
  //     - This method is not used.
  //     - This method has no tests.
  //     - This method is private scope.
  const Entry* old_entry;
  if (entries_.LookUp(input, &old_entry)) {
    DeleteEntry(old_entry);
  }
  entries_.DeleteEntry(input);
}

bool Table::LoadFromString(const std::string& str) {
  std::istringstream is(str);
  return LoadFromStream(&is);
}

bool Table::LoadFromFile(const char* filepath) {
  std::unique_ptr<std::istream> ifs(ConfigFileStream::LegacyOpen(filepath));
  if (ifs == nullptr) {
    return false;
  }
  return LoadFromStream(ifs.get());
}

namespace {
constexpr char kAttributeDelimiter = ' ';

TableAttributes ParseAttributes(const absl::string_view input) {
  TableAttributes attributes = NO_TABLE_ATTRIBUTE;

  std::vector<absl::string_view> attribute_strings =
      absl::StrSplit(input, kAttributeDelimiter, absl::AllowEmpty());

  for (const absl::string_view attribute_string : attribute_strings) {
    if (attribute_string == "NewChunk") {
      attributes |= NEW_CHUNK;
    } else if (attribute_string == "NoTransliteration") {
      attributes |= NO_TRANSLITERATION;
    } else if (attribute_string == "DirectInput") {
      attributes |= DIRECT_INPUT;
    } else if (attribute_string == "EndChunk") {
      attributes |= END_CHUNK;
    }
  }
  return attributes;
}
}  // namespace

bool Table::LoadFromStream(std::istream* is) {
  DCHECK(is);
  std::string line;
  while (!is->eof()) {
    std::getline(*is, line);
    Util::ChopReturns(&line);
    if (line.empty()) {
      continue;
    }

    std::vector<std::string> rules =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    if (rules.size() == 4) {
      const TableAttributes attributes = ParseAttributes(rules[3]);
      AddRuleWithAttributes(rules[0], rules[1], rules[2], attributes);
    } else if (rules.size() == 3) {
      AddRule(rules[0], rules[1], rules[2]);
    } else if (rules.size() == 2) {
      AddRule(rules[0], rules[1], "");
    } else {
      if (line[0] != '#') {
        LOG(ERROR) << "Format error: " << line;
      }
      continue;
    }
  }

  return true;
}

const Entry* Table::LookUp(const absl::string_view input) const {
  const Entry* entry = nullptr;
  if (case_sensitive_) {
    entries_.LookUp(input, &entry);
  } else {
    std::string normalized_input(input);
    Util::LowerString(&normalized_input);
    entries_.LookUp(normalized_input, &entry);
  }
  return entry;
}

const Entry* Table::LookUpPrefix(const absl::string_view input,
                                 size_t* key_length, bool* fixed) const {
  const Entry* entry = nullptr;
  if (case_sensitive_) {
    entries_.LookUpPrefix(input, &entry, key_length, fixed);
  } else {
    std::string normalized_input(input);
    Util::LowerString(&normalized_input);
    entries_.LookUpPrefix(normalized_input, &entry, key_length, fixed);
  }
  return entry;
}

void Table::LookUpPredictiveAll(const absl::string_view input,
                                std::vector<const Entry*>* results) const {
  if (case_sensitive_) {
    entries_.LookUpPredictiveAll(input, results);
  } else {
    std::string normalized_input(input);
    Util::LowerString(&normalized_input);
    entries_.LookUpPredictiveAll(normalized_input, results);
  }
}

bool Table::HasNewChunkEntry(const absl::string_view input) const {
  if (input.empty()) {
    return false;
  }

  const std::string key = absl::StrCat(kNewChunkPrefix, input);
  size_t key_length = 0;
  bool fixed = false;
  LookUpPrefix(key, &key_length, &fixed);
  if (key_length > 1) {
    return true;
  }

  return false;
}

bool Table::HasSubRules(const absl::string_view input) const {
  if (case_sensitive_) {
    return entries_.HasSubTrie(input);
  } else {
    std::string normalized_input(input);
    Util::LowerString(&normalized_input);
    return entries_.HasSubTrie(normalized_input);
  }
}

void Table::DeleteEntry(const Entry* entry) { entry_set_.erase(entry); }

bool Table::case_sensitive() const { return case_sensitive_; }

void Table::set_case_sensitive(const bool case_sensitive) {
  case_sensitive_ = case_sensitive;
}

// static
const Table& Table::GetDefaultTable() {
  return *GetSharedDefaultTable();  // NOLINT: The referenced object has static
                                    // lifetime.
}

std::shared_ptr<const Table> Table::GetSharedDefaultTable() {
  static absl::NoDestructor<std::shared_ptr<const Table>> kDefaultSharedTable(
      new Table());
  return *kDefaultSharedTable;
}

// ========================================
// TableContainer
// ========================================
TableManager::TableManager()
    : custom_roman_table_fingerprint_(CityFingerprint32("")) {}

std::shared_ptr<const Table> TableManager::GetTable(
    const mozc::commands::Request& request,
    const mozc::config::Config& config) {
  // calculate the hash depending on the request and the config
  const uint32_t hash =
      absl::HashOf(request.special_romanji_table(), config.preedit_method(),
                   config.punctuation_method(), config.symbol_method());

  // When custom_roman_table is set, force to create new table.
  bool update_custom_roman_table = false;
  if ((config.preedit_method() == config::Config::ROMAN) &&
      config.has_custom_roman_table() && !config.custom_roman_table().empty()) {
    const uint32_t custom_roman_table_fingerprint =
        CityFingerprint32(config.custom_roman_table());
    if (custom_roman_table_fingerprint != custom_roman_table_fingerprint_) {
      update_custom_roman_table = true;
      custom_roman_table_fingerprint_ = custom_roman_table_fingerprint;
    }
  }

  if (const auto iterator = table_map_.find(hash);
      iterator != table_map_.end()) {
    if (update_custom_roman_table) {
      // Delete the previous table to update the table.
      table_map_.erase(iterator);
    } else {
      return iterator->second;
    }
  }

  auto table = std::make_shared<Table>();
  if (!table->InitializeWithRequestAndConfig(request, config)) {
    return nullptr;
  }

  table_map_.emplace(hash, table);
  return table;
}

void TableManager::ClearCaches() { table_map_.clear(); }

}  // namespace composer
}  // namespace mozc
