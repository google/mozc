// Copyright 2010, Google Inc.
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
#include "base/util.h"
#include "session/config.pb.h"
#include "session/config_handler.h"

namespace mozc {
namespace composer {
namespace {
static const char kDefaultPreeditTableFile[] = "system://romanji-hiragana.tsv";
static const char kRomajiPreeditTableFile[] = "system://romanji-hiragana.tsv";
// Table for Kana combinations like "か゛" → "が".
static const char kKanaCombinationTableFile[] = "system://kana.tsv";
}  // anonymous namespace

// ========================================
// Entry
// ========================================
Entry::Entry(const string& input, const string& result, const string& pending)
    : input_(input), result_(result), pending_(pending) {}

// ========================================
// Table
// ========================================
Table::Table()
    : entries_(new EntryTrie), case_sensitive_(false) {}

Table::~Table() {
  EntrySet::iterator it;
  for (it = entry_set_.begin(); it != entry_set_.end(); ++it) {
    const Entry* entry = *it;
    delete entry;
  }
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

bool Table::Initialize() {
  bool result = false;
  const config::Config &config = config::ConfigHandler::GetConfig();

  // If shift_key_mode_switch option is disabled,
  // table looking up is executed in case sensitive mode.
  // This makes power users who use extremely customized Romaji input table
  // happy.
  set_case_sensitive(
      config.shift_key_mode_switch() == mozc::config::Config::OFF);

  switch(config.preedit_method()) {
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

bool Table::Reload() {
  entries_.reset(new EntryTrie);
  return Initialize();
}

void Table::AddRule(const string& input,
                    const string& output,
                    const string& pending) {
  const size_t kMaxSize = 32;
  if (input.size() >= kMaxSize ||
      output.size() >= kMaxSize ||
      pending.size() >= kMaxSize) {
    LOG(ERROR) << "Invalid input/output/pending";
    return;
  }

  const Entry* old_entry = NULL;
  if (!pending.empty() &&
      (entries_->LookUp(pending, &old_entry) || input == pending)) {
    LOG(WARNING) << "Entry "
                 << input << " " << output << " " << pending
                 << " is removed, since the rule is looping";
    return;
  }

  const Entry* entry = new Entry(input, output, pending);
  if (entries_->LookUp(input, &old_entry)) {
    DeleteEntry(old_entry);
  }
  entries_->AddEntry(input, entry);
  entry_set_.insert(entry);
}

void Table::DeleteRule(const string& input) {
  const Entry* old_entry;
  if (entries_->LookUp(input, &old_entry)) {
    DeleteEntry(old_entry);
  }
  entries_->DeleteEntry(input);
}

bool Table::LoadFromString(const string &str) {
  istringstream is(str);
  return LoadFromStream(&is);
}

bool Table::LoadFromFile(const char* filepath) {
  scoped_ptr<istream> ifs(ConfigFileStream::Open(filepath));
  if (ifs.get() == NULL) {
    return false;
  }
  return LoadFromStream(ifs.get());
}

bool Table::LoadFromStream(istream *is) {
  DCHECK(is);
  string line;
  const string empty_pending("");
  while (!is->eof()) {
    getline(*is, line);
    Util::ChopReturns(&line);
    if (line.empty() || line[0] == '#') {
      continue;
    }

    vector<string> rules;
    Util::SplitStringAllowEmpty(line, "\t", &rules);
    if (rules.size() == 3) {
      AddRule(rules[0], rules[1], rules[2]);
    } else if (rules.size() == 2) {
      AddRule(rules[0], rules[1], empty_pending);
    } else {
      LOG(ERROR) << "Format error: " << line;
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

bool Table::HasSubRules(const string& input) const {
  if (case_sensitive_) {
    return entries_->HasSubTrie(input);
  } else {
    string normalized_input = input;
    Util::LowerString(&normalized_input);
    return entries_->HasSubTrie(normalized_input);
  }
}

void Table::DeleteEntry(const Entry* entry) {
  entry_set_.erase(entry);
  delete entry;
}

bool Table::case_sensitive() const {
  return case_sensitive_;
}

void Table::set_case_sensitive(const bool case_sensitive) {
  case_sensitive_ = case_sensitive;
}
}  // namespace composer
}  // namespace mozc
