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
#include "base/singleton.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/rx/rx_trie.h"
#include "storage/encrypted_string_storage.h"

// TODO(hsumita): Locks user dictionary file.

namespace mozc {
namespace pinyin {
namespace english {

namespace {
#include "languages/pinyin/pinyin_embedded_english_dictionary_data.h"

const char *kUserDictionaryFileName = "user://pinyin_english.db";
// The last printable character in ASCII code
const char kSentinelValueForAlphabet = '~';
// It should be less than storage size (64MByte) / entry size (~85Byte)
const size_t kMaxUserDictionarySize = 50000;
const size_t kMaxWordLength = 80;

// Format is | key_length (1byte) | key (~80bytes) | used_count (4bytes) |
bool SerializeUserDictionary(const UserDictionary &dictionary,
                             string *output) {
  DCHECK(output);
  output->clear();

  for (UserDictionary::const_iterator it = dictionary.begin();
       it != dictionary.end(); ++it) {
    const string &key = it->first;
    const uint32 used_count = it->second;
    const char *used_count_ptr = reinterpret_cast<const char *>(&used_count);

    // The length of the key should be <= 80.
    // So we can save it as uint8.
    DCHECK_GE(80, key.size());
    output->append(1, static_cast<uint8>(key.size()));
    output->append(key);
    output->append(used_count_ptr, used_count_ptr + 4);
  }

  return true;
}

// Format is | key_length (1byte) | key (~80bytes) | used_count (4bytes) |
bool DeserializeUserDictionary(const string &input,
                               UserDictionary *dictionary) {
  DCHECK(dictionary);
  dictionary->clear();

  size_t index = 0;
  while (index < input.size()) {
    if (index + 1 > input.size()) {
      LOG(ERROR) << "Cannot parse user dicitonary.";
      dictionary->clear();
      return false;
    }

    const size_t key_length = static_cast<size_t>(input[index]);
    index += 1;

    if (index + key_length + 4 > input.size()) {
      LOG(ERROR) << "Cannot parse user dicitonary.";
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

struct  DictionaryEntry {
  string key;
  float priority;
};

bool DictionaryEntryComparator(const DictionaryEntry &lhs,
                               const DictionaryEntry &rhs) {
  if (lhs.priority != rhs.priority) {
    return lhs.priority > rhs.priority;
  }
  return lhs.key < rhs.key;
}
}  // namespace

EnglishDictionary::EnglishDictionary()
    : word_trie_(new rx::RxTrie),
      storage_(new storage::EncryptedStringStorage(
          EnglishDictionary::file_name())) {
  Init();
}

EnglishDictionary::~EnglishDictionary() {
}

bool EnglishDictionary::GetSuggestions(const string &input_prefix,
                                       vector<string> *output) const {
  DCHECK(output);
  output->clear();

  string prefix = input_prefix;
  Util::LowerString(&prefix);

  if (prefix.empty()) {
    return false;
  }

  vector<rx::RxEntry> system_entries;
  word_trie_->PredictiveSearch(prefix, &system_entries);

  map<string, float> merged_entries;
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
    const rx::RxEntry &entry = system_entries[i];
    merged_entries[entry.key] += priority_table_[entry.id];
  }

  vector<DictionaryEntry> merged_vector;
  for (map<string, float>::const_iterator iter = merged_entries.begin();
       iter != merged_entries.end(); ++iter) {
    DictionaryEntry entry = {
      iter->first,
      iter->second,
    };
    merged_vector.push_back(entry);
  }

  sort(merged_vector.begin(), merged_vector.end(), DictionaryEntryComparator);

  for (size_t i = 0; i < merged_vector.size(); ++i) {
    output->push_back(merged_vector[i].key);
  }

  if (output->empty()) {
    return false;
  }

  return true;
}

void EnglishDictionary::LearnWord(const string &input_word) {
  if (input_word.empty()) {
    LOG(ERROR) << "Cannot learn an empty word.";
    return;
  }

  if (input_word.size() >= kMaxWordLength) {
    LOG(ERROR) << "Cannot learn a too long word.";
    return;
  }

  // TODO(hsumita): Introduces LRU algorithm. http://b/6047022
  if (user_dictionary_.size() < kMaxUserDictionarySize) {
    string word = input_word;
    Util::LowerString(&word);

    // If |word| is not registered on |user_dictionary_| yet, the value of the
    // entry will be 1 since constructor of std::map initializes it with 0.
    ++user_dictionary_[word];
  }

  Sync();
}

void EnglishDictionary::Init() {
  vector<DictionaryFileSection> sections;
  DictionaryFileCodecInterface *codec =
      DictionaryFileCodecFactory::GetCodec();
  if (!codec->ReadSections(
          kPinyinEnglishDictionary_data, kPinyinEnglishDictionary_size,
          &sections)) {
    LOG(ERROR) << "Cannot open English dictionary"
        " because section data is not found.";
    return;
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
        LOG(ERROR) << "Failed to create ngram dictionary";
      }
    } else if (section.name == priority_table_section_name) {
      const float *p = reinterpret_cast<const float *>(section.ptr);
      const int length = section.len / sizeof(*p);
      priority_table_.assign(p, p + length);
    } else if (section.name == learning_multiplier_section_name) {
      learning_multiplier_ = *reinterpret_cast<const float *>(section.ptr);
    } else {
      DCHECK(false) << "Unknown section name: " << section.name;
    }
  }

  ReloadUserDictionary();
}

bool EnglishDictionary::ReloadUserDictionary() {
  user_dictionary_.clear();

  string serialized_data;
  storage_->Load(&serialized_data);
  if (!DeserializeUserDictionary(serialized_data, &user_dictionary_)) {
    LOG(ERROR) << "Cannot deserialize data.";
    return false;
  }

  return true;
}

bool EnglishDictionary::Sync() {
  string serialized_data;
  if (!SerializeUserDictionary(user_dictionary_, &serialized_data)) {
    LOG(ERROR) << "Cannot sync.";
    return false;
  }
  return storage_->Save(serialized_data);
}

// static
string EnglishDictionary::file_name() {
  return ConfigFileStream::GetFileName(kUserDictionaryFileName);
}

}  // namespace english
}  // namespace pinyin
}  // namespace mozc
