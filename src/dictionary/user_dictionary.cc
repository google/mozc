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

#include "dictionary/user_dictionary.h"

#include <algorithm>
#include <set>
#include <string>
#include "base/base.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/stl_util.h"
#include "base/thread.h"
#include "base/trie.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos_interface.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace {

class POSTokenLess {
 public:
  bool operator()(const UserPOS::Token *lhs,
                  const UserPOS::Token *rhs) const {
    return lhs->key < rhs->key;
  }
};

class UserDictionaryFileManager {
 public:
  UserDictionaryFileManager() {}

  const string GetFileName() {
    scoped_lock l(&mutex_);
    if (filename_.empty()) {
      return UserDictionaryUtil::GetUserDictionaryFileName();
    } else {
      return filename_;
    }
  }

  void SetFileName(const string &filename) {
    scoped_lock l(&mutex_);
    filename_ = filename;
  }

 private:
  string filename_;
  Mutex mutex_;
  DISALLOW_COPY_AND_ASSIGN(UserDictionaryFileManager);
};
}  // namespace

class TokensIndex : public vector<UserPOS::Token *> {
 public:
  explicit TokensIndex(const UserPOSInterface *user_pos,
                       SuppressionDictionary *suppression_dictionary)
      : user_pos_(user_pos),
        suppression_dictionary_(suppression_dictionary) {}
  virtual ~TokensIndex() {
    Clear();
  }

  void Clear() {
    STLDeleteElements(this);
    clear();
  }

  void Load(const UserDictionaryStorage &storage) {
    Clear();
    set<uint64> seen;
    vector<UserPOS::Token> tokens;
    int sync_words_count = 0;

    if (!suppression_dictionary_->IsLocked()) {
      LOG(ERROR) << "SuppressionDictionary must be locked first";
    }
    suppression_dictionary_->Clear();

    for (size_t i = 0; i < storage.dictionaries_size(); ++i) {
      const UserDictionaryStorage::UserDictionary &dic =
          storage.dictionaries(i);
      if (!dic.enabled() || dic.entries_size() == 0) {
        continue;
      }

      if (dic.syncable()) {
        sync_words_count += dic.entries_size();
      }

      for (size_t j = 0; j < dic.entries_size(); ++j) {
        const UserDictionaryStorage::UserDictionaryEntry &entry =
            dic.entries(j);

        if (!UserDictionaryUtil::IsValidEntry(*user_pos_, entry)) {
          continue;
        }

        string tmp, reading;
        UserDictionaryUtil::NormalizeReading(entry.key(), &tmp);

        // We cannot call NormalizeVoiceSoundMark inside NormalizeReading,
        // because the normalization is user-visible.
        // http://b/2480844
        Util::NormalizeVoicedSoundMark(tmp, &reading);

        const uint64 fp = Util::Fingerprint(reading +
                                            "\t" +
                                            entry.value() +
                                            "\t" +
                                            entry.pos());
        if (!seen.insert(fp).second) {
          VLOG(1) << "Found dup item";
          continue;
        }

        // "抑制単語"
        if (entry.pos() == "\xE6\x8A\x91\xE5\x88\xB6\xE5\x8D\x98\xE8\xAA\x9E") {
          suppression_dictionary_->AddEntry(reading, entry.value());
        } else {
          tokens.clear();
          user_pos_->GetTokens(reading, entry.value(), entry.pos(), &tokens);
          for (size_t k = 0; k < tokens.size(); ++k) {
            this->push_back(new UserPOS::Token(tokens[k]));
          }
        }
      }
    }

    sort(this->begin(), this->end(), POSTokenLess());

    suppression_dictionary_->UnLock();

    VLOG(1) << this->size() << " user dic entries loaded";

    usage_stats::UsageStats::SetInteger("UserRegisteredWord",
                                        static_cast<int>(this->size()));
    usage_stats::UsageStats::SetInteger("UserRegisteredSyncWord",
                                        sync_words_count);
  }

 private:
  const UserPOSInterface *user_pos_;
  SuppressionDictionary *suppression_dictionary_;
};

class UserDictionaryReloader : public Thread {
 public:
  explicit UserDictionaryReloader(UserDictionary *dic)
      : auto_register_mode_(false), dic_(dic) {
    DCHECK(dic_);
  }

  virtual ~UserDictionaryReloader() {
    Join();
  }

  void StartAutoRegistration(const string &key,
                             const string &value,
                             const string &pos) {
    {
      scoped_lock l(&mutex_);
      auto_register_mode_ = true;
      key_ = key;
      value_ = value;
      pos_ = pos;
    }
    Start();
  }

  void StartReload() {
    Start();
  }

  virtual void Run() {
    scoped_ptr<UserDictionaryStorage>
        storage(
            new UserDictionaryStorage
            (Singleton<UserDictionaryFileManager>::get()->GetFileName()));
    // Load from file
    if (!storage->Load()) {
      return;
    }

    if (auto_register_mode_ &&
        !storage->AddToAutoRegisteredDictionary(key_, value_, pos_)) {
      LOG(ERROR) << "failed to execute AddToAutoRegisteredDictionary";
      auto_register_mode_ = false;
      return;
    }

    auto_register_mode_ = false;
    dic_->Load(*(storage.get()));
  }

 private:
  Mutex mutex_;
  bool auto_register_mode_;
  UserDictionary *dic_;
  string key_;
  string value_;
  string pos_;
};

UserDictionary::UserDictionary(const UserPOSInterface *user_pos,
                               const POSMatcher *pos_matcher,
                               SuppressionDictionary *suppression_dictionary)
    : reloader_(new UserDictionaryReloader(this)),
      user_pos_(user_pos),
      pos_matcher_(pos_matcher),
      suppression_dictionary_(suppression_dictionary),
      empty_limit_(Limit()),
      tokens_(new TokensIndex(user_pos_, suppression_dictionary)) {
  DCHECK(user_pos_);
  DCHECK(pos_matcher_);
  DCHECK(suppression_dictionary_);
  Reload();
}

UserDictionary::~UserDictionary() {
  reloader_->Join();
  delete tokens_;
}

Node *UserDictionary::LookupPredictiveWithLimit(
    const char *str, int size, const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  scoped_reader_lock l(&mutex_);

  if (size == 0) {
    LOG(WARNING) << "string of length zero is passed.";
    return NULL;
  }

  if (tokens_->empty()) {
    return NULL;
  }

  if (GET_CONFIG(incognito_mode)) {
    return NULL;
  }

  DCHECK(allocator != NULL);
  Node *result_node = NULL;
  string key(str, size);

  // Look for a starting point of iteration over dictionary contents.
  UserPOS::Token key_token;
  key_token.key = key;
  vector<UserPOS::Token *>::const_iterator it =
      lower_bound(tokens_->begin(), tokens_->end(), &key_token, POSTokenLess());

  for (; it != tokens_->end(); ++it) {
    if (!Util::StartsWith((*it)->key, key)) {
      break;
    }
    // check begin with
    if (limit.begin_with_trie != NULL) {
      string value;
      size_t key_length = 0;
      bool has_subtrie = false;
      if (!limit.begin_with_trie->LookUpPrefix((*it)->key.data() + size, &value,
                                               &key_length, &has_subtrie)) {
        continue;
      }
    }

    Node *new_node = allocator->NewNode();
    DCHECK(new_node);
    if (pos_matcher_->IsSuggestOnlyWord((*it)->id)) {
      new_node->lid = pos_matcher_->GetUnknownId();
      new_node->rid = pos_matcher_->GetUnknownId();
    } else {
      new_node->lid = (*it)->id;
      new_node->rid = (*it)->id;
    }
    new_node->wcost = (*it)->cost;
    new_node->key = (*it)->key;
    new_node->value = (*it)->value;
    new_node->node_type = Node::NOR_NODE;
    new_node->attributes |= Node::NO_VARIANTS_EXPANSION;
    new_node->attributes |= Node::USER_DICTIONARY;
    new_node->bnext = result_node;
    result_node = new_node;
  }
  return result_node;
}

Node *UserDictionary::LookupPredictive(
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  return LookupPredictiveWithLimit(str, size, empty_limit_, allocator);
}

Node *UserDictionary::LookupPrefixWithLimit(
    const char *str,
    int size,
    const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  scoped_reader_lock l(&mutex_);

  if (size == 0) {
    LOG(WARNING) << "string of length zero is passed.";
    return NULL;
  }

  if (tokens_->empty()) {
    return NULL;
  }

  if (GET_CONFIG(incognito_mode)) {
    return NULL;
  }


  DCHECK(allocator != NULL);
  Node *result_node = NULL;
  string key(str, size);

  // Look for a starting point of iteration over dictionary contents.
  UserPOS::Token key_token;
  key_token.key = key.substr(0, Util::OneCharLen(key.c_str()));
  vector<UserPOS::Token *>::const_iterator it =
      lower_bound(tokens_->begin(), tokens_->end(), &key_token, POSTokenLess());

  for (; it != tokens_->end(); ++it) {
    if ((*it)->key > key) {
      break;
    }

    if (pos_matcher_->IsSuggestOnlyWord((*it)->id)) {
      continue;
    }

    if (!Util::StartsWith(key, (*it)->key)) {
      continue;
    }

    // check the lower limit of key length
    if ((*it)->key.size() < limit.key_len_lower_limit) {
      continue;
    }

    Node *new_node = allocator->NewNode();
    DCHECK(new_node);
    new_node->lid = (*it)->id;
    new_node->rid = (*it)->id;
    new_node->wcost = (*it)->cost;
    new_node->key = (*it)->key;
    new_node->value = (*it)->value;
    new_node->node_type = Node::NOR_NODE;
    new_node->attributes |= Node::NO_VARIANTS_EXPANSION;
    new_node->attributes |= Node::USER_DICTIONARY;
    new_node->bnext = result_node;
    result_node = new_node;
  }

  return result_node;
}

Node *UserDictionary::LookupPrefix(const char *str,
                                   int size,
                                   NodeAllocatorInterface *allocator) const {
  return LookupPrefixWithLimit(str, size, empty_limit_, allocator);
}

Node *UserDictionary::LookupReverse(const char *str, int size,
                                    NodeAllocatorInterface *allocator) const {
  if (GET_CONFIG(incognito_mode)) {
    return NULL;
  }

  return NULL;
}

bool UserDictionary::Reload() {
  if (reloader_->IsRunning()) {
    return false;
  }

  suppression_dictionary_->Lock();
  DCHECK(suppression_dictionary_->IsLocked());
  reloader_->StartReload();

  return true;
}

bool UserDictionary::AddToAutoRegisteredDictionary(
    const string &key, const string &value, const string &pos) {
  if (reloader_->IsRunning()) {
    return false;
  }

  scoped_ptr<NodeAllocator> allocator(new NodeAllocator);
  Node *result = LookupPrefix(key.data(), key.size(), allocator.get());
  for (Node *node = result; node != NULL; node = node->bnext) {
    if (node->key == key && node->value == value) {
      // Already registered
      return false;
    }
  }

  suppression_dictionary_->Lock();
  DCHECK(suppression_dictionary_->IsLocked());
  reloader_->StartAutoRegistration(key, value, pos);

  return true;
}

void UserDictionary::WaitForReloader() {
  reloader_->Join();
}

void UserDictionary::Swap(TokensIndex *new_tokens) {
  DCHECK(new_tokens);
  TokensIndex *old_tokens = tokens_;
  {
    scoped_writer_lock l(&mutex_);
    tokens_ = new_tokens;
  }
  delete old_tokens;
}

bool UserDictionary::Load(const UserDictionaryStorage &storage) {
  size_t size = 0;
  {
    scoped_reader_lock l(&mutex_);
    size = tokens_->size();
  }

  // If UserDictionary is pretty big, we first remove the
  // current dictionary to save memory usage.
  const size_t kVeryBigUserDictionarySize = 100000;

  if (size >= kVeryBigUserDictionarySize) {
    TokensIndex *dummy_empty_tokens = new TokensIndex(user_pos_,
                                                      suppression_dictionary_);
    Swap(dummy_empty_tokens);
  }

  TokensIndex *tokens = new TokensIndex(user_pos_, suppression_dictionary_);
  tokens->Load(storage);
  Swap(tokens);
  return true;
}

void UserDictionary::SetUserDictionaryName(const string &filename) {
  Singleton<UserDictionaryFileManager>::get()->SetFileName(filename);
}
}  // namespace mozc
