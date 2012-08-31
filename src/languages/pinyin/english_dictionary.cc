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

#include "languages/pinyin/english_dictionary.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/config_file_stream.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/dictionary_file.h"
// Includes generated dictionary data.
#include "languages/pinyin/pinyin_embedded_english_dictionary_data.h"
#include "storage/encrypted_string_storage.h"

#ifdef MOZC_USE_MOZC_LOUDS
#include "dictionary/louds/louds_trie_adapter.h"
#else
#include "dictionary/rx/rx_trie.h"
#endif  // MOZC_USE_MOZC_LOUDS

// TODO(hsumita): Lock user dictionary file.

namespace mozc {
namespace pinyin {
namespace english {

namespace {
#ifdef MOZC_USE_MOZC_LOUDS
typedef dictionary::louds::Entry EntryType;
#else
typedef rx::RxEntry EntryType;
#endif  // MOZC_USE_MOZC_LOUDS

const char *kUserDictionaryFileName = "user://pinyin_english.db";
// The last printable character in ASCII code
const char kSentinelValueForAlphabet = '~';
// It should be less than storage size (64MByte) / entry size (~85Byte)
const size_t kMaxUserDictionarySize = 50000;
const size_t kMaxWordLength = 80;

// Serialized user dictionary format is as following.
// Dictionary : array of Entry
// Entry      : | key_length (1byte) | key (~80bytes) | used_count (4bytes) |
//
// key_length : length of key.
// key:       : word registered on user dictionary entry.
// used_count : number that how many times this entry is learned.
//
// |key| should be smaller than or equal to 80 bytes, and we can store the
// length of |key| within 1byte w/o worry about signed vs unsigned conversion.

void SerializeUserDictionary(const UserDictionary &dictionary,
                             string *output) {
  CHECK(output);
  output->clear();

  for (UserDictionary::const_iterator it = dictionary.begin();
       it != dictionary.end(); ++it) {
    const string &key = it->first;
    const uint32 used_count = it->second;
    const char *used_count_ptr = reinterpret_cast<const char *>(&used_count);

    // The length of the key should be <= 80.
    // So we can save it as uint8.
    CHECK_GE(80, key.size());
    output->append(1, static_cast<uint8>(key.size()));
    output->append(key);
    output->append(used_count_ptr, used_count_ptr + 4);
  }
}

bool DeserializeUserDictionary(const string &input,
                               UserDictionary *dictionary) {
  CHECK(dictionary);
  dictionary->clear();

  size_t index = 0;
  while (index < input.size()) {
    const size_t key_length = input[index];
    index += 1;

    if (index + key_length + 4 > input.size()) {
      LOG(ERROR) << "Cannot parse user dictionary.";
      dictionary->clear();
      return false;
    }

    const string key = input.substr(index, key_length);
    index += key_length;
    const uint32 used_count = *reinterpret_cast<const uint32 *>(
        input.substr(index, 4).c_str());
    index += 4;

    dictionary->insert(make_pair(key, used_count));
  }

  return true;
}

// <key, priority>
typedef pair<string, float> DictionaryEntry;
typedef map<string, float> DictionaryMap;

bool DictionaryEntryComparator(const DictionaryEntry &lhs,
                               const DictionaryEntry &rhs) {
  if (lhs.second != rhs.second) {
    return lhs.second > rhs.second;
  }
  return lhs.first < rhs.first;
}
}  // namespace

EnglishDictionary::EnglishDictionary()
    : word_trie_(new TrieType),
      storage_(new storage::EncryptedStringStorage(
          EnglishDictionary::user_dictionary_file_path())) {
  Init();
}

EnglishDictionary::~EnglishDictionary() {
}

void EnglishDictionary::GetSuggestions(const string &input_prefix,
                                       vector<string> *output) const {
  CHECK(output);
  output->clear();

  if (input_prefix.empty()) {
    return;
  }

  string prefix = input_prefix;
  Util::LowerString(&prefix);

  vector<EntryType> system_entries;
  word_trie_->PredictiveSearch(prefix, &system_entries);

  DictionaryMap merged_entries;
  {
    const UserDictionary::const_iterator it_begin =
        user_dictionary_.lower_bound(prefix);
    const UserDictionary::const_iterator it_end =
        user_dictionary_.lower_bound(prefix + kSentinelValueForAlphabet);
    for (UserDictionary::const_iterator it = it_begin; it != it_end; ++it) {
      merged_entries[it->first] = learning_multiplier_ * it->second;
    }
  }
  for (size_t i = 0; i < system_entries.size(); ++i) {
    const EntryType &entry = system_entries[i];
    merged_entries[entry.key] += priority_table_[entry.id];
  }

  vector<DictionaryEntry> merged_vector(merged_entries.size());
  copy(merged_entries.begin(), merged_entries.end(), merged_vector.begin());
  sort(merged_vector.begin(), merged_vector.end(), DictionaryEntryComparator);

  for (size_t i = 0; i < merged_vector.size(); ++i) {
    output->push_back(merged_vector[i].first);
  }
}

bool EnglishDictionary::LearnWord(const string &input_word) {
  if (input_word.empty()) {
    LOG(ERROR) << "Cannot learn an empty word.";
    return false;
  }

  if (input_word.size() > kMaxWordLength) {
    LOG(ERROR) << "Cannot learn a too long word.";
    return false;
  }

  // TODO(hsumita): Introduce LRU algorithm. http://b/6047022
  if (user_dictionary_.size() < kMaxUserDictionarySize) {
    string word = input_word;
    Util::LowerString(&word);

    // If |word| is not registered on |user_dictionary_| yet, the value of the
    // entry will be 1 since constructor of std::map initializes it with 0.
    ++user_dictionary_[word];
  }

  return Sync();
}

void EnglishDictionary::Init() {
  CHECK(priority_table_.empty());

  vector<DictionaryFileSection> sections;
  const DictionaryFileCodecInterface *codec =
      DictionaryFileCodecFactory::GetCodec();
  if (!codec->ReadSections(kPinyinEnglishDictionary_data,
                          kPinyinEnglishDictionary_size, &sections)) {
    LOG(FATAL)
        << "Cannot open English dictionary because section data is not found.";
  }

  const string word_trie_section_name =
      codec->GetSectionName("english_dictionary_trie");
  const string priority_table_section_name =
      codec->GetSectionName("english_word_priority_table");
  const string learning_multiplier_section_name =
      codec->GetSectionName("learning_multiplier");
  for (size_t i = 0; i < sections.size(); ++i) {
    const DictionaryFileSection &section = sections[i];
    if (section.name == word_trie_section_name) {
      if (!word_trie_->OpenImage(
              reinterpret_cast<const unsigned char *>(section.ptr))) {
        LOG(FATAL) << "Failed to open trie section data.";
      }
    } else if (section.name == priority_table_section_name) {
      const float *p = reinterpret_cast<const float *>(section.ptr);
      const int length = section.len / sizeof(*p);
      priority_table_.assign(p, p + length);
    } else if (section.name == learning_multiplier_section_name) {
      learning_multiplier_ = *reinterpret_cast<const float *>(section.ptr);
    } else {
      LOG(FATAL) << "Unknown section name: " << section.name;
    }
  }

  ReloadUserDictionary();
}

bool EnglishDictionary::ReloadUserDictionary() {
  user_dictionary_.clear();

  string serialized_data;
  if (!storage_->Load(&serialized_data) ||
      !DeserializeUserDictionary(serialized_data, &user_dictionary_)) {
    LOG(ERROR) << "Cannot deserialize data.";
    return false;
  }

  return true;
}

bool EnglishDictionary::Sync() {
  string serialized_data;
  SerializeUserDictionary(user_dictionary_, &serialized_data);
  return storage_->Save(serialized_data);
}

// static
string EnglishDictionary::user_dictionary_file_path() {
  return ConfigFileStream::GetFileName(kUserDictionaryFileName);
}

}  // namespace english
}  // namespace pinyin
}  // namespace mozc
