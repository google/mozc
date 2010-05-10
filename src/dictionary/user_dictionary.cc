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

#include "dictionary/user_dictionary.h"

#include <algorithm>
#include <set>
#include <string>
#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "converter/converter_data.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace {

void ReloadUserDictionary() {
  VLOG(1) << "Reloading user dictionary";
  UserDictionary::GetUserDictionary()->AsyncReload();
  // Sync version:
  // UserDictionary::GetUserDictionary()->Reload();
}

// ReloadUserDictionary() is called by Session::Reload()
REGISTER_MODULE_RELOADER(reload_user_dictionary,
                         ReloadUserDictionary());

class POSTokenLess {
 public:
  bool operator()(const POS::Token *lhs, const POS::Token *rhs) const {
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

class UserDictionaryReloader : public Thread {
 public:
  UserDictionaryReloader(UserDictionary *dic)
      : dic_(dic) {
    DCHECK(dic_);
  }

  virtual ~UserDictionaryReloader() {
    Join();
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

    dic_->Load(*(storage.get()));
  }

 private:
  UserDictionary *dic_;
};

UserDictionary::UserDictionary() {
  AsyncReload();
}

UserDictionary::~UserDictionary() {
  if (reloader_.get() != NULL) {
    reloader_->Join();
  }
  Clear();
}

bool UserDictionary::CheckReloaderAndDelete() const {
  if (reloader_.get() != NULL) {
    if (reloader_->IsRunning()) {
      return false;
    } else {
      reloader_.reset(NULL);  // remove
    }
  }

  return true;
}

Node *UserDictionary::LookupPredictive(const char *str, int size,
                                       ConverterData *data) const {
  if (size == 0) {
    LOG(WARNING) << "string of length zero is passed.";
    return NULL;
  }

  if (tokens_.empty()) {
    return NULL;
  }

  if (!CheckReloaderAndDelete()) {
    LOG(WARNING) << "Reloader is running";
    return NULL;
  }

  DCHECK(data != NULL);
  Node *result_node = NULL;
  string key(str, size);

  // Look for a starting point of iteration over dictionary contents.
  POS::Token key_token;
  key_token.key = key;
  vector<POS::Token *>::const_iterator it =
      lower_bound(tokens_.begin(), tokens_.end(), &key_token, POSTokenLess());

  for (; it != tokens_.end(); ++it) {
    if ((*it)->key.find(key) != 0) {
      break;
    }

    Node *new_node = data->NewNode();
    new_node->lid = (*it)->id;
    new_node->rid = (*it)->id;
    new_node->wcost = (*it)->cost;
    new_node->key = (*it)->key;
    new_node->value = (*it)->value;
    new_node->node_type = Node::NOR_NODE;
    new_node->normalization_type = Node::NO_NORMALIZATION;
    new_node->bnext = result_node;
    result_node = new_node;
  }
  return result_node;
}

Node *UserDictionary::LookupExact(const char *str, int size,
                                  ConverterData *data) const {
  if (!CheckReloaderAndDelete()) {
    LOG(WARNING) << "Reloader is running";
    return NULL;
  }

  return NULL;
}

Node *UserDictionary::LookupPrefix(const char *str, int size,
                                   ConverterData *data) const {
  if (size == 0) {
    LOG(WARNING) << "string of length zero is passed.";
    return NULL;
  }

  if (tokens_.empty()) {
    return NULL;
  }

  if (!CheckReloaderAndDelete()) {
    LOG(WARNING) << "Reloader is running";
    return NULL;
  }

  DCHECK(data != NULL);
  Node *result_node = NULL;
  string key(str, size);

  // Look for a starting point of iteration over dictionary contents.
  POS::Token key_token;
  key_token.key = key.substr(0, Util::OneCharLen(key.c_str()));
  vector<POS::Token *>::const_iterator it =
      lower_bound(tokens_.begin(), tokens_.end(), &key_token, POSTokenLess());

  for (; it != tokens_.end(); ++it) {
    if ((*it)->key > key) {
      break;
    }
    if (key.find((*it)->key) != 0) {
      continue;
    }

    Node *new_node = data->NewNode();
    new_node->lid = (*it)->id;
    new_node->rid = (*it)->id;
    new_node->wcost = (*it)->cost;
    new_node->key = (*it)->key;
    new_node->value = (*it)->value;
    new_node->node_type = Node::NOR_NODE;
    new_node->normalization_type = Node::NO_NORMALIZATION;
    new_node->bnext = result_node;
    result_node = new_node;
  }

  return result_node;
}

Node *UserDictionary::LookupReverse(const char *str, int size,
                                    ConverterData *data) const {
  if (!CheckReloaderAndDelete()) {
    LOG(WARNING) << "Reloader is running";
    return NULL;
  }

  return NULL;
}

bool UserDictionary::Reload() {
  Clear();

  if (GET_CONFIG(incognito_mode)) {
    return true;
  }

  scoped_ptr<UserDictionaryStorage>
      storage(
          new UserDictionaryStorage
          (Singleton<UserDictionaryFileManager>::get()->GetFileName()));
  // Load from file
  if (!storage->Load()) {
    return false;
  }

  return Load(*(storage.get()));
}

bool UserDictionary::AsyncReload() {
  if (GET_CONFIG(incognito_mode)) {
    return true;
  }

  // now loading
  if (!CheckReloaderAndDelete()) {
    return true;
  }

  reloader_.reset(new UserDictionaryReloader(this));
  reloader_->Start();

  return true;
}

void UserDictionary::WaitForReloader() {
  if (reloader_.get() != NULL) {
    reloader_->Join();
    reloader_.reset(NULL);
  }
}

bool UserDictionary::Load(const UserDictionaryStorage &storage) {
  Clear();

  set<uint64> seen;
  vector<POS::Token> tokens;

  for (size_t i = 0; i < storage.dictionaries_size(); ++i) {
    const UserDictionaryStorage::UserDictionary &dic =
        storage.dictionaries(i);
    if (!dic.enabled() || dic.entries_size() == 0) {
      continue;
    }

    for (size_t j = 0; j < dic.entries_size(); ++j) {
      const UserDictionaryStorage::UserDictionaryEntry &entry =
          dic.entries(j);
      if (!UserDictionaryUtil::IsValidEntry(entry)) {
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

      tokens.clear();
      POS::GetTokens(reading, entry.value(), entry.pos(),
                     POS::DEFAULT, &tokens);
      for (size_t k = 0; k < tokens.size(); ++k) {
        tokens_.push_back(new POS::Token(tokens[k]));
      }
    }
  }

  sort(tokens_.begin(), tokens_.end(), POSTokenLess());

  VLOG(1) << tokens_.size() << " user dic entries loaded";

  usage_stats::UsageStats::SetInteger("UserRegisteredWord",
                                      static_cast<int>(tokens_.size()));

  return true;
}

void UserDictionary::Clear() {
  for (vector<POS::Token *>::iterator it = tokens_.begin();
       it != tokens_.end(); it++) {
    delete *it;
  }
  tokens_.clear();
}

void UserDictionary::SetUserDictionaryName(const string &filename) {
  Singleton<UserDictionaryFileManager>::get()->SetFileName(filename);
}

UserDictionary *UserDictionary::GetUserDictionary() {
  return Singleton<UserDictionary>::get();
}
}  // namespace mozc
