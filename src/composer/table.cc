// Copyright 2010-2013, Google Inc.
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

#include <sstream>
#include <string>

#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/trie.h"
#include "base/util.h"
#include "composer/internal/typing_model.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"

namespace mozc {
namespace composer {
namespace {
const char kDefaultPreeditTableFile[] = "system://romanji-hiragana.tsv";
const char kRomajiPreeditTableFile[] = "system://romanji-hiragana.tsv";
// Table for Kana combinations like "か゛" → "が".
const char kKanaCombinationTableFile[] = "system://kana.tsv";

// Special tables for 12keys
const char k12keysHiraganaTableFile[] = "system://12keys-hiragana.tsv";
const char k12keysHalfwidthasciiTableFile[]
    = "system://12keys-halfwidthascii.tsv";
const char k12keysNumberTableFile[]
    = "system://12keys-number.tsv";
const char kFlickHiraganaTableFile[] = "system://flick-hiragana.tsv";
const char kFlickHalfwidthasciiTableFile[]
    = "system://flick-halfwidthascii.tsv";
const char kFlickNumberTableFile[]
    = "system://flick-number.tsv";
const char kToggleFlickHiraganaTableFile[]
    = "system://toggle_flick-hiragana.tsv";
const char kToggleFlickHalfwidthasciiTableFile[]
    = "system://toggle_flick-halfwidthascii.tsv";
const char kToggleFlickNumberTableFile[]
    = "system://toggle_flick-number.tsv";
// Special tables for QWERTY mobile
const char kQwertyMobileHiraganaTableFile[]
    = "system://qwerty_mobile-hiragana.tsv";
const char kQwertyMobileHiraganaNumberTableFile[]
    = "system://qwerty_mobile-hiragana-number.tsv";
const char kQwertyMobileHalfwidthasciiTableFile[]
    = "system://qwerty_mobile-halfwidthascii.tsv";
// Special tables for Godan
const char kGodanHiraganaTableFile[] = "system://godan-hiragana.tsv";

const char kNewChunkPrefix[] = "\t";
const char kSpecialKeyOpen[] = "\x0F";  // Shift-In of ASCII
const char kSpecialKeyClose[] = "\x0E";  // Shift-Out of ASCII
}  // anonymous namespace

// ========================================
// Entry
// ========================================
Entry::Entry(const string &input,
             const string &result,
             const string &pending,
             const TableAttributes attributes)
    : input_(input),
      result_(result),
      pending_(pending),
      attributes_(attributes) {}

// ========================================
// Table
// ========================================
Table::Table()
    : entries_(new EntryTrie),
      case_sensitive_(false),
      typing_model_(NULL) {}

Table::~Table() {
  ResetEntrySet();
}

static const char kKuten[]  = "\xE3\x80\x81";  // "、"
static const char kTouten[] = "\xE3\x80\x82";  // "。"
static const char kComma[]  = "\xEF\xBC\x8C";  // "，"
static const char kPeriod[] = "\xEF\xBC\x8E";  // "．"

static const char kCornerOpen[]  = "\xE3\x80\x8C";  // "「"
static const char kCornerClose[] = "\xE3\x80\x8D";  // "」"
static const char kSlash[]       = "\xEF\xBC\x8F";  // "／"
static const char kSquareOpen[]  = "[";
static const char kSquareClose[] = "]";
static const char kMiddleDot[]   = "\xE3\x83\xBB";  // "・"

bool Table::InitializeWithRequestAndConfig(const commands::Request &request,
                                           const config::Config &config) {
  case_sensitive_ = false;
  bool result = false;
  typing_model_ = TypingModel::GetTypingModel(request.special_romanji_table());
  if (request.special_romanji_table()
      != mozc::commands::Request::DEFAULT_TABLE) {
    const char *table_file_name;
    switch (request.special_romanji_table()) {
      case mozc::commands::Request::TWELVE_KEYS_TO_HIRAGANA:
        table_file_name = k12keysHiraganaTableFile;
        break;
      case mozc::commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII:
        table_file_name = k12keysHalfwidthasciiTableFile;
        break;
      case mozc::commands::Request::TWELVE_KEYS_TO_NUMBER:
        table_file_name = k12keysNumberTableFile;
        break;
      case mozc::commands::Request::FLICK_TO_HIRAGANA:
        table_file_name = kFlickHiraganaTableFile;
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
      case mozc::commands::Request::TOGGLE_FLICK_TO_HALFWIDTHASCII:
        table_file_name = kToggleFlickHalfwidthasciiTableFile;
        break;
      case mozc::commands::Request::TOGGLE_FLICK_TO_NUMBER:
        table_file_name = kToggleFlickNumberTableFile;
        break;
      case mozc::commands::Request::QWERTY_MOBILE_TO_HIRAGANA:
        // This table is almost as same as "romaji-hiragana.tsv",
        // and the diff should be only the behavior of ','.
        // So probably we'd like to share the table, but we keep this way
        // for now, as this is still internal code.
        // TODO(hidehiko): refactor this code to clean up.
        table_file_name = kQwertyMobileHiraganaTableFile;
        break;
      case mozc::commands::Request::QWERTY_MOBILE_TO_HIRAGANA_NUMBER:
        table_file_name = kQwertyMobileHiraganaNumberTableFile;
        break;
      case mozc::commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII:
        table_file_name = kQwertyMobileHalfwidthasciiTableFile;
        break;
      case mozc::commands::Request::GODAN_TO_HIRAGANA:
        table_file_name = kGodanHiraganaTableFile;
        break;
      default:
        table_file_name = NULL;
    }
    if (table_file_name && LoadFromFile(table_file_name)) {
      return true;
    }
  }
  switch (config.preedit_method()) {
    case config::Config::ROMAN:
      result = (config.has_custom_roman_table() &&
                !config.custom_roman_table().empty()) ?
          LoadFromString(config.custom_roman_table()) :
          LoadFromFile(kRomajiPreeditTableFile);
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
  const mozc::composer::Entry *entry = NULL;

  // Comma / Kuten
  entry = LookUp(",");
  if (entry == NULL ||
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
  if (entry == NULL ||
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
  if (entry == NULL ||
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
  if (entry == NULL ||
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
  if (entry == NULL ||
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

bool Table::IsLoopingEntry(const string &input,
                           const string &pending) const {
  if (input.empty() || pending.empty()) {
    return false;
  }

  string key = pending;
  do {
    // If input is a prefix of key, it should be looping.
    // (ex. input="a", pending="abc").
    if (Util::StartsWith(key, input)) {
      return true;
    }

    size_t key_length = 0;
    bool fixed = false;
    const Entry *entry = LookUpPrefix(key, &key_length, &fixed);
    if (entry == NULL) {
      return false;
    }
    DCHECK_LE(key_length, key.size());
    key = entry->pending() + key.substr(key_length);
  } while (!key.empty());

  return false;
}

const Entry *Table::AddRule(const string &input,
                            const string &output,
                            const string &pending) {
  return AddRuleWithAttributes(input, output, pending, NO_TABLE_ATTRIBUTE);
}

const Entry *Table::AddRuleWithAttributes(const string &escaped_input,
                                          const string &output,
                                          const string &escaped_pending,
                                          const TableAttributes attributes) {
  if (attributes & NEW_CHUNK) {
    // TODO(komatsu): Make a new trie tree for checking the new chunk
    // attribute rather than reusing the conversion trie.
    const string additional_input = kNewChunkPrefix + escaped_input;
    AddRuleWithAttributes(additional_input, output, escaped_pending,
                          NO_TABLE_ATTRIBUTE);
  }

  const size_t kMaxSize = 300;
  if (escaped_input.size() >= kMaxSize ||
      output.size() >= kMaxSize ||
      escaped_pending.size() >= kMaxSize) {
    LOG(ERROR) << "Invalid input/output/pending";
    return NULL;
  }

  const string input = ParseSpecialKey(escaped_input);
  const string pending = ParseSpecialKey(escaped_pending);
  if (IsLoopingEntry(input, pending)) {
    LOG(WARNING) << "Entry "
                 << input << " " << output << " " << pending
                 << " is removed, since the rule is looping";
    return NULL;
  }

  const Entry *old_entry = NULL;
  if (entries_->LookUp(input, &old_entry)) {
    DeleteEntry(old_entry);
  }

  Entry *entry = new Entry(input, output, pending, attributes);
  entries_->AddEntry(input, entry);
  entry_set_.insert(entry);

  // Check if the input has a large captal character.
  // Invisible character is exception.
  if (!case_sensitive_) {
    const string trimed_input = DeleteSpecialKey(input);
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

void Table::DeleteRule(const string &input) {
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

bool Table::LoadFromString(const string &str) {
  istringstream is(str);
  return LoadFromStream(&is);
}

bool Table::LoadFromFile(const char *filepath) {
  scoped_ptr<istream> ifs(ConfigFileStream::LegacyOpen(filepath));
  if (ifs.get() == NULL) {
    return false;
  }
  return LoadFromStream(ifs.get());
}

const TypingModel* Table::typing_model() const {
  return typing_model_;
}

namespace {
const char kAttributeDelimiter[] = " ";

TableAttributes ParseAttributes(const string &input) {
  TableAttributes attributes = NO_TABLE_ATTRIBUTE;

  vector<string> attribute_strings;
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
}  // anonymous namespace

bool Table::LoadFromStream(istream *is) {
  DCHECK(is);
  string line;
  const string empty_pending("");
  while (!is->eof()) {
    getline(*is, line);
    Util::ChopReturns(&line);
    if (line.empty()) {
      continue;
    }

    vector<string> rules;
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

const Entry *Table::LookUp(const string &input) const {
  const Entry *entry = NULL;
  if (case_sensitive_) {
    entries_->LookUp(input, &entry);
  } else {
    string normalized_input = input;
    Util::LowerString(&normalized_input);
    entries_->LookUp(normalized_input, &entry);
  }
  return entry;
}

const Entry *Table::LookUpPrefix(const string &input,
                                 size_t *key_length,
                                 bool *fixed) const {
  const Entry *entry = NULL;
  if (case_sensitive_) {
    entries_->LookUpPrefix(input, &entry, key_length, fixed);
  } else {
    string normalized_input = input;
    Util::LowerString(&normalized_input);
    entries_->LookUpPrefix(normalized_input, &entry, key_length, fixed);
  }
  return entry;
}

void Table::LookUpPredictiveAll(const string &input,
                                vector<const Entry *> *results) const {
  if (case_sensitive_) {
    entries_->LookUpPredictiveAll(input, results);
  } else {
    string normalized_input = input;
    Util::LowerString(&normalized_input);
    entries_->LookUpPredictiveAll(normalized_input, results);
  }
}

bool Table::HasNewChunkEntry(const string &input) const {
  if (input.empty()) {
    return false;
  }

  const string key = kNewChunkPrefix + input;
  size_t key_length = 0;
  bool fixed = false;
  LookUpPrefix(key, &key_length, &fixed);
  if (key_length > 1) {
    return true;
  }

  return false;
}

bool Table::HasSubRules(const string &input) const {
  if (case_sensitive_) {
    return entries_->HasSubTrie(input);
  } else {
    string normalized_input = input;
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

bool Table::case_sensitive() const {
  return case_sensitive_;
}

void Table::set_case_sensitive(const bool case_sensitive) {
  case_sensitive_ = case_sensitive;
}

namespace {
bool FindBlock(const string &input, const string &open, const string &close,
               const size_t offset, size_t *open_pos, size_t *close_pos) {
  DCHECK(open_pos);
  DCHECK(close_pos);

  *open_pos = input.find(open, offset);
  if (*open_pos == string::npos) {
    return false;
  }

  *close_pos = input.find(close, *open_pos);
  if (*close_pos == string::npos) {
    return false;
  }

  return true;
}
}  // anonymous namespace

// static
string Table::ParseSpecialKey(const string &input) {
  string output;
  size_t open_pos = 0;
  size_t close_pos = 0;
  for (size_t cursor = 0; cursor < input.size();) {
    if (!FindBlock(input, "{", "}", cursor, &open_pos, &close_pos)) {
      output.append(input, cursor, input.size() - cursor);
      break;
    }

    output.append(input, cursor, open_pos - cursor);

    // The both sizes of "{" and "}" is 1.
    const string key(input, open_pos + 1, close_pos - open_pos - 1);
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
string Table::DeleteSpecialKey(const string &input) {
  string output;
  size_t open_pos = 0;
  size_t close_pos = 0;
  for (size_t cursor = 0; cursor < input.size();) {
    if (!FindBlock(input, kSpecialKeyOpen, kSpecialKeyClose,
                   cursor, &open_pos, &close_pos)) {
      output.append(input, cursor, input.size() - cursor);
      break;
    }

    output.append(input, cursor, open_pos - cursor);
    // The size of kSpecialKeyClose is 1.
    cursor = close_pos + 1;
  }
  return output;
}

// ========================================
// TableContainer
// ========================================
TableManager::TableManager() {
}

TableManager::~TableManager() {
  for (map<uint32, const Table*>::iterator iterator = table_map_.begin();
      iterator != table_map_.end();
      ++iterator) {
    delete iterator->second;
  }
}

const Table *TableManager::GetTable(const mozc::commands::Request &request,
                                    const mozc::config::Config &config) {
  // calculate the hash depending on the request and the config
  uint32 hash = request.special_romanji_table();
  hash = hash * (mozc::config::Config_PreeditMethod_PreeditMethod_MAX + 1)
      + config.preedit_method();
  hash =
      hash * (mozc::config::Config_PunctuationMethod_PunctuationMethod_MAX + 1)
      + config.punctuation_method();
  hash = hash * (mozc::config::Config_SymbolMethod_SymbolMethod_MAX + 1)
      + config.symbol_method();

  // When custom_roman_table is set, force to create new table.
  bool update_custom_roman_table = false;
  if ((config.preedit_method() == config::Config::ROMAN) &&
      config.has_custom_roman_table() &&
      !config.custom_roman_table().empty()) {
    const uint32 custom_roman_table_fingerprint =
        Util::Fingerprint32(config.custom_roman_table());
    if (custom_roman_table_fingerprint != custom_roman_table_fingerprint_) {
      update_custom_roman_table = true;
      custom_roman_table_fingerprint_ = custom_roman_table_fingerprint;
    }
  }

  map<uint32, const Table*>::iterator iterator = table_map_.find(hash);
  if (iterator != table_map_.end()) {
    if (update_custom_roman_table) {
      // Delete the previous table to update the table.
      delete iterator->second;
      table_map_.erase(iterator);
    } else {
      return iterator->second;
    }
  }

  scoped_ptr<Table> table(new Table());
  if (!table->InitializeWithRequestAndConfig(request, config)) {
    return NULL;
  }

  Table* table_to_cache = table.release();
  table_map_[hash] = table_to_cache;
  return table_to_cache;
}
}  // namespace composer
}  // namespace mozc
