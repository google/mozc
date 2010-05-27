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

#include "prediction/user_history_predictor.h"

#include <algorithm>
#include <time.h>
#include <string>
#include "base/base.h"
#include "base/encryptor.h"
#include "base/mmap.h"
#include "base/init.h"
#include "base/thread.h"
#include "base/util.h"
#include "base/password_manager.h"
#include "base/config_file_stream.h"
#include "converter/segments.h"
#include "storage/lru_cache.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.pb.h"
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "usage_stats/usage_stats.h"

namespace mozc {

namespace {
// find suggestion candidates from the most recent 1000 history in LRU.
// We don't check all history, since suggestion is called every key event
const size_t kMaxSuggestionTrial = 1000;

// cache size
const size_t kLRUCacheSize = 10000;

// don't save key/value that are
// longer than kMaxCandidateSize to avoid memory explosion
const size_t kMaxStringLength = 128;

// salt size for encryption
const size_t kSaltSize = 32;

// 64Mbyte
// Maximum file size for history
const size_t kMaxFileSize = 64 * 1024 * 1024;

// revert id for user_history_predictor
const uint16 kRevertId = 1;

// file name for the history
#ifdef OS_WINDOWS
const char kFileName[] = "user://history.db";
#else
const char kFileName[] = "user://.history.db";
#endif

// use '\t' as a key/value delimiter
const char kDelimiter[] = "\t";

bool IsValidSuggestion(uint32 prefix_len,
                       uint32 word_len,
                       uint32 suggestion_freq,
                       uint32 conversion_freq) {
  // Handle suggestion_freq and conversion_freq differently.
  // conversion_freq is less aggressively affecting to the final decision.
  const uint32 freq = max(suggestion_freq, conversion_freq / 4);

  const uint32 base_prefix_len = 3 - min(static_cast<uint32>(2), freq);
  return (prefix_len >= base_prefix_len);
}
}  // namespace

class UserHistoryPredictorSyncer : public Thread {
 public:
  enum RequestType {
    LOAD,
    SAVE
  };

  UserHistoryPredictorSyncer(UserHistoryPredictor *predictor,
                             RequestType type)
      : predictor_(predictor), type_(type) {
    DCHECK(predictor_);
  }

  virtual void Run() {
    switch (type_) {
      case LOAD:
        VLOG(1) << "Executing Reload method";
        predictor_->Load();
        break;
      case SAVE:
        VLOG(1) << "Executing Sync method";
        predictor_->Save();
        break;
      default:
        LOG(ERROR) << "Unknown request: " << static_cast<int>(type_);
    }
  }

  virtual ~UserHistoryPredictorSyncer() {
    Join();
  }

  UserHistoryPredictor *predictor_;
  RequestType type_;
};

UserHistoryPredictor::UserHistoryPredictor()
    : updated_(false), dic_(new DicCache(kLRUCacheSize)) {
  AsyncLoad();  // non-blocking
  // Load()  blocking version can be used if any
}

UserHistoryPredictor::~UserHistoryPredictor() {
  // In destructor, must call blocking version
  WaitForSyncer();
  Save();   // blocking
}

// return revert id
uint16 UserHistoryPredictor::revert_id() {
  return kRevertId;
}

void UserHistoryPredictor::WaitForSyncer() {
  if (syncer_.get() != NULL) {
    syncer_->Join();
    syncer_.reset(NULL);
  }
}

bool UserHistoryPredictor::CheckSyncerAndDelete() const {
  if (syncer_.get() != NULL) {
    if (syncer_->IsRunning()) {
      return false;
    } else {
      syncer_.reset(NULL);  // remove
    }
  }

  return true;
}

bool UserHistoryPredictor::Sync() {
  return AsyncSave();
  // return Save();   blocking version
}

bool UserHistoryPredictor::AsyncLoad() {
  if (!CheckSyncerAndDelete()) {  // now loading/saving
    return true;
  }

  syncer_.reset(new UserHistoryPredictorSyncer(
      this,
      UserHistoryPredictorSyncer::LOAD));
  syncer_->Start();

  return true;
}

bool UserHistoryPredictor::AsyncSave() {
  if (!CheckSyncerAndDelete()) {  // now loading/saving
    return true;
  }

  syncer_.reset(new UserHistoryPredictorSyncer(
      this,
      UserHistoryPredictorSyncer::SAVE));
  syncer_->Start();

  return true;
}

bool UserHistoryPredictor::Load() {
  mozc::user_history_predictor::UserHistory history;
  {
    string input;
    string salt;

    // read encrypted message and salt from local file
    {
      const string filename = ConfigFileStream::GetFileName(kFileName);
      Mmap<char> mmap;
      if (!mmap.Open(filename.c_str(), "r")) {
        LOG(ERROR) << "cannot open user history file";
        return false;
      }

      if (mmap.GetFileSize() < kSaltSize) {
        LOG(ERROR) << "file size is too small";
        return false;
      }

      if (mmap.GetFileSize() > kMaxFileSize) {
        LOG(ERROR) << "file size is too big.";
        return false;
      }

      // copy salt
      char tmp[kSaltSize];
      memcpy(tmp, mmap.begin(), kSaltSize);
      salt.assign(tmp, kSaltSize);

      // copy body
      input.assign(mmap.begin() + kSaltSize,
                   mmap.GetFileSize() - kSaltSize);
    }

    string password;
    if (!PasswordManager::GetPassword(&password)) {
      LOG(ERROR) << "PasswordManager::GetPassword() failed";
      return false;
    }

    if (password.empty()) {
      LOG(ERROR) << "password is empty";
      return false;
    }

    // Decrypt message
    Encryptor::Key key;
    if (!key.DeriveFromPassword(password, salt)) {
      LOG(ERROR) << "Encryptor::Key::DeriveFromPassword failed";
      return false;
    }

    if (!Encryptor::DecryptString(key, &input)) {
      LOG(ERROR) << "Encryptor::DecryptString() failed";
      return false;
    }

    if (!history.ParseFromString(input)) {
      LOG(ERROR) << "ParseFromString failed. message looks broken";
      return false;
    }
  }

  Entry entry;
  for (size_t i = 0; i < history.entries_size(); ++i) {
    entry.suggestion_freq = history.entries(i).suggestion_freq();
    entry.conversion_freq = history.entries(i).conversion_freq();
    entry.last_access_time =  history.entries(i).last_access_time();
    entry.length = Util::CharsLen(history.entries(i).value());
    entry.description.clear();
    const string dic_key =
        history.entries(i).key() + kDelimiter + history.entries(i).value();
    if (history.entries(i).has_description() &&
        history.entries(i).description().size() > 0) {
      entry.description = history.entries(i).description();
    }
    dic_->Insert(dic_key, entry);
  }

  VLOG(1) << "Loaded user histroy, size=" << history.entries_size();

  return true;
}

bool UserHistoryPredictor::Save() {
  if (!updated_) {
    return true;
  }

  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return true;
  }

  if (!GET_CONFIG(use_history_suggest)) {
    VLOG(2) << "no history suggest";
    return true;
  }

  const DicElement *tail = dic_->Tail();
  if (tail == NULL) {
    return true;
  }

  string salt;
  string output;

  vector<string> tokens;
  {
    mozc::user_history_predictor::UserHistory history;
    for (const DicElement *elm = tail; elm != NULL; elm = elm->prev) {
      tokens.clear();
      Util::SplitStringUsing(elm->key, kDelimiter, &tokens);
      if (tokens.size() != 2) {
        LOG(ERROR) << "Format error: " << elm->key;
        continue;
      }
      const string &key = tokens[0];
      const string &value = tokens[1];
      if (key.empty() || value.empty()) {
        LOG(ERROR) << "key or value is empty";
        continue;
      }
      mozc::user_history_predictor::UserHistory::Entry *entry
          = history.add_entries();
      if (entry == NULL) {
        LOG(ERROR) << "entry is NULL";
        continue;
      }
      entry->set_key(key);
      entry->set_value(value);
      entry->set_description(elm->value.description);
      entry->set_suggestion_freq(elm->value.suggestion_freq);
      entry->set_conversion_freq(elm->value.conversion_freq);
      entry->set_last_access_time(elm->value.last_access_time);
    }

    // update usage stats here.
    usage_stats::UsageStats::SetInteger("UserHistoryPredictorEntrySize",
                                        static_cast<int>(history.entries_size()));

    if (history.entries_size() == 0) {
      LOG(WARNING) << "etries size is 0. Not saved";
      return false;
    }

    if (!history.AppendToString(&output)) {
      LOG(ERROR) << "AppendToString failed";
      return false;
    }
  }

  {
    string password;
    if (!PasswordManager::GetPassword(&password)) {
      LOG(ERROR) << "PasswordManager::GetPassword() failed";
      return false;
    }

    if (password.empty()) {
      LOG(ERROR) << "password is empty";
      return false;
    }

    char tmp[kSaltSize];
    memset(tmp, '\0', sizeof(tmp));
    Util::GetSecureRandomSequence(tmp, sizeof(tmp));
    salt.assign(tmp, sizeof(tmp));

    Encryptor::Key key;
    if (!key.DeriveFromPassword(password, salt)) {
      LOG(ERROR) << "Encryptor::Key::DeriveFromPassword() failed";
      return false;
    }

    if (!Encryptor::EncryptString(key, &output)) {
      LOG(ERROR) << "Encryptor::EncryptString() failed";
      return false;
    }
  }

  // Even if histoy is empty, save to them into a file to
  // make the file empty
  const string filename = ConfigFileStream::GetFileName(kFileName);
  const string tmp_filename = filename + ".tmp";

  {
    OutputFileStream ofs(tmp_filename.c_str(), ios::out | ios::binary);
    if (!ofs) {
      LOG(ERROR) << "failed to write: " << tmp_filename;
      return false;
    }

    VLOG(1) << "Syncing user history to: " << filename;
    ofs.write(salt.data(), salt.size());
    ofs.write(output.data(), output.size());
  }

  if (!Util::AtomicRename(tmp_filename, filename)) {
    LOG(ERROR) << "AtomicRename failed";
  }

#ifdef OS_WINDOWS
  wstring wfilename;
  Util::UTF8ToWide(filename.c_str(), &wfilename);
  if (!::SetFileAttributes(wfilename.c_str(),
                           FILE_ATTRIBUTE_HIDDEN |
                           FILE_ATTRIBUTE_SYSTEM)) {
    LOG(ERROR) << "Cannot make hidden: " << filename
               << " " << ::GetLastError();
  }
#endif

  updated_ = false;

  return true;
}

bool UserHistoryPredictor::ClearAllHistory() {
  // Wait until syncer finishes
  WaitForSyncer();

  VLOG(1) << "Clearing user prediction";
  // renew DicCache as LRUCache tries to reuse the internal value by
  // using FreeList
  dic_.reset(new DicCache(kLRUCacheSize));
  // remove file
  Util::Unlink(ConfigFileStream::GetFileName(kFileName));
  return true;
}

bool UserHistoryPredictor::ClearUnusedHistory() {
  // Wait until syncer finishes
  WaitForSyncer();

  VLOG(1) << "Clearing unused prediction";
  const DicElement *head = dic_->Head();
  if (head == NULL) {
    VLOG(2) << "dic head is NULL";
    return false;
  }

  vector<string> keys;
  for (const DicElement *elm = head; elm != NULL; elm = elm->next) {
    VLOG(3) << elm->key << " " << elm->value.suggestion_freq;
    if (elm->value.suggestion_freq == 0) {
      keys.push_back(elm->key);
    }
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    VLOG(2) << "Removing: " << keys[i];
    if (!dic_->Erase(keys[i])) {
      LOG(ERROR) << "cannot erase " << keys[i];
    }
  }

  if (!keys.empty()) {
    VLOG(1) << "Syncing to file";
    updated_ = true;
    Sync();
  }

  VLOG(1) << keys.size() << " removed";

  return true;
}

bool UserHistoryPredictor::Lookup(Segments *segments) const {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return false;
  }

  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return false;
  }

  if (segments->request_type() == Segments::CONVERSION) {
    VLOG(2) << "request type is CONVERSION";
    return false;
  }

  if (!GET_CONFIG(use_history_suggest) &&
      segments->request_type() == Segments::SUGGESTION) {
    VLOG(2) << "no history suggest";
    return false;
  }

  if (segments->conversion_segments_size() < 1) {
    VLOG(2) << "segment size < 1";
    return false;
  }

  const string &key = segments->conversion_segment(0).key();
  const size_t key_len = Util::CharsLen(key);
  if (key_len == 0) {
    VLOG(2) << "key length is 0";
    return false;
  }

  const DicElement *head = dic_->Head();
  if (head == NULL) {
    VLOG(2) << "dic head is NULL";
    return false;
  }

  // Do linear search
  Segment *segment = segments->mutable_conversion_segment(0);

  if (segment == NULL) {
    LOG(ERROR) << "segment is NULL";
    return false;
  }

  size_t size = segments->max_prediction_candidates_size();

  int trial = 0;
  for (const DicElement *elm = head; size > 0 && elm != NULL; elm = elm->next) {
    if (segments->request_type() == Segments::SUGGESTION &&
        trial++ >= kMaxSuggestionTrial) {
      VLOG(2) << "too many trials";
      break;
    }

    // format is:
    // <key><delimiter><value>
    const string::size_type key_size = elm->key.find(kDelimiter);

    if (key_size == string::npos ||
        key_size == 0 ||
        key.size() > key_size ||
        memcmp(elm->key.data(), key.data(), key.size()) != 0) {
      continue;
    }

    const string value = elm->key.substr(key_size + 1,
                                         elm->key.size() - key_size - 1);
    const string description = elm->value.description;

    // don't suggest exactly the same candidate
    if (key == value) {
      continue;
    }

    Segment::Candidate *candidate = NULL;
    if (segments->request_type() == Segments::PREDICTION) {
      candidate = segment->push_back_candidate();
    } else if (segments->request_type() == Segments::SUGGESTION) {
      // The top result of suggestion should be a VALID suggestion candidate.
      // i.e., SuggestionTrigerFunc should return true for the first
      // candidate.
      // If user types "デスノート" too many times, "デスノート" will be
      // suggested when user types "で". It is expected, but if user types
      // "です" after that,  showing "デスノート" is annoying.
      // In this situation, "です" is in the LRU, but SuggestionTrigerFunc
      // returns false for "です", since it is short.
      if (IsValidSuggestion(key_len, elm->value.length,
                            elm->value.suggestion_freq,
                            elm->value.conversion_freq)) {
        candidate = segment->push_back_candidate();
      } else if (segment->candidates_size() == 0) {
        VLOG(2) << "candidates size is 0";
        return false;
      }
    } else {
      LOG(ERROR) << "Unknown mode";
      return false;
    }

    if (candidate == NULL) {
      VLOG(2) << "candiate is NULL";
      continue;
    }

    candidate->Init();
    candidate->content_key = elm->key.substr(0, key_size);
    candidate->value = value;
    candidate->content_value = value;
    // If we have stored description, set it exactly.
    if (!description.empty()) {
      candidate->description = description;
    } else {
      candidate->SetDefaultDescription(
          Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);
    }

    // decrease the total size of candidates
    --size;
  }

  return (segment->candidates_size() > 0);
}

bool UserHistoryPredictor::Predict(Segments *segments) const {
  return Lookup(segments);
}

bool UserHistoryPredictor::Suggest(Segments *segments) const {
  return Lookup(segments);
}

void UserHistoryPredictor::Insert(const string &key,
                                  const string &value,
                                  const string &description,
                                  bool is_suggestion_selected,
                                  Segments *segments) {
  if (key.size() > kMaxStringLength ||
      value.size() > kMaxStringLength ||
      description.size() > kMaxStringLength) {
    return;
  }
  const string dic_key = key + kDelimiter + value;

  if (!dic_->HasKey(dic_key)) {
    // the key is a new key inserted in the last Finish method.
    // Here we push a new RevertEntry so that the new "key" can be
    // removed when Revert() method is called.
    Segments::RevertEntry *revert_entry = segments->push_back_revert_entry();
    DCHECK(revert_entry);
    revert_entry->key = dic_key;
    revert_entry->id = UserHistoryPredictor::revert_id();
    revert_entry->revert_entry_type = Segments::RevertEntry::CREATE_ENTRY;
  } else {
    // the key is a old key not inserted in the last Finish method
    // TODO(taku):
    // add a treatment for UPDATE_ENTRY mode
  }

  DicElement *e = dic_->Insert(dic_key);
  if (e == NULL) {
    VLOG(2) << "insert failed";
    return;
  }

  e->value.length = Util::CharsLen(value);
  e->value.last_access_time = static_cast<uint32>(time(NULL));
  if (!description.empty()) {
    e->value.description = description;
  }

  if (is_suggestion_selected) {
    e->value.suggestion_freq++;
  } else {
    e->value.conversion_freq++;
  }

  VLOG(3) << dic_key << " has inserted: "
          << e->value.suggestion_freq << " "
          << e->value.conversion_freq;

  // new entry is inserted to the cache
  updated_ = true;
}

void UserHistoryPredictor::Finish(Segments *segments) {
  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return;
  }

  if (!GET_CONFIG(use_history_suggest)) {
    VLOG(2) << "no history suggest";
    return;
  }

  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  const bool kInsertConversion = false;
  const bool kInsertSuggestion = true;

  if (segments->request_type() == Segments::CONVERSION) {
    const size_t history_segments_size = segments->history_segments_size();
    string content_key, content_value, key, value;
    for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
      const Segment &segment = segments->segment(i);
      if (segment.candidates_size() < 1) {
        VLOG(2) << "candidates size < 1";
        return;
      }
      if (segment.candidate(0).learning_type &
          Segment::Candidate::NO_SUGGEST_LEARNING) {
        VLOG(2) << "NO_SUGGEST_LEARNING";
        return;
      }

      // Just get the prefix of segments having FIXED_VALUE type.
      if (segment.segment_type() != Segment::FIXED_VALUE) {
        VLOG(2) << "segment is not FIXED_VALUE";
        break;
      }
      // remove the last functional word
      if (i + 1 == segments->segments_size()) {
        content_key += segment.candidate(0).content_key;
        content_value += segment.candidate(0).content_value;
      } else {
        content_key += segment.key();
        content_value += segment.candidate(0).value;
      }
      key += segment.key();
      value += segment.candidate(0).value;
    }

    if (key.empty() || value.empty()) {
      VLOG(2) << "key or value is empty";
      return;
    }

    string description;
    if (history_segments_size + 1 == segments->segments_size()) {
      description =
          segments->segment(history_segments_size).candidate(0).description;
    }

    if (content_value != value) {
      // do not save description for content
      Insert(content_key, content_value, "", kInsertConversion, segments);
    }

    Insert(key, value, description, kInsertConversion, segments);

  // Store user-history from predictoin
  } else {
    if (segments->conversion_segments_size() < 1) {
      return;
    }
    const Segment &segment = segments->conversion_segment(0);
    if (segment.candidates_size() < 1) {
      return;
    }

    const string &key = segment.candidate(0).content_key;
    const string &value = segment.candidate(0).content_value;
    const string &description = segment.candidate(0).description;

    Insert(key, value, description, kInsertSuggestion, segments);
  }

  return;
}

void UserHistoryPredictor::Revert(Segments *segments) {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  for (size_t i = 0; i < segments->revert_entries_size(); ++i) {
    const Segments::RevertEntry &revert_entry =
        segments->revert_entry(i);
    if (revert_entry.id == UserHistoryPredictor::revert_id() &&
        revert_entry.revert_entry_type == Segments::RevertEntry::CREATE_ENTRY) {
      VLOG(2) << "Erasing the key: " << revert_entry.key;
      dic_->Erase(revert_entry.key);
    }
  }
}
}  // namespace mozc
