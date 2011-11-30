// Copyright 2010-2011, Google Inc.
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

#include "mac/UserHistoryTransition/user_history_transition.h"

#include <launch.h>

#include "base/base.h"
#include "base/mac_util.h"
#include "base/util.h"
#include "mac/UserHistoryTransition/deprecated_user_storage.h"
#include "prediction/user_history_predictor.h"
#include "sync/user_history_sync_util.h"

namespace mozc {
namespace {
bool StopConverter() {
  const string label = mozc::MacUtil::GetLabelForSuffix("Converter");

  launch_data_t stop_command =
      launch_data_alloc(LAUNCH_DATA_DICTIONARY);
  launch_data_dict_insert(stop_command,
                          launch_data_new_string(label.c_str()),
                          LAUNCH_KEY_STOPJOB);
  launch_data_t result_data = launch_msg(stop_command);
  launch_data_free(stop_command);
  if (result_data == NULL) {
    LOG(ERROR) << "Failed to stop the converter";
    return false;
  }
  launch_data_free(result_data);

  const int kTrial = 3;
  for (int i = 0; i < kTrial; ++i) {
    // Getting PID by using launch_msg API.  If the process stops, the
    // launch job data does not have PID info.
    launch_data_t get_info_command =
        launch_data_alloc(LAUNCH_DATA_DICTIONARY);
    launch_data_dict_insert(get_info_command,
                            launch_data_new_string(label.c_str()),
                            LAUNCH_KEY_GETJOB);
    launch_data_t process_info = launch_msg(get_info_command);
    launch_data_free(get_info_command);
    if (process_info == NULL) {
      LOG(ERROR) << "Unexpected error: launchd doesn't return the data "
                 << "for the service.  But it means that the process is "
                 << "not running at this time.";
      return true;
    }

    launch_data_t pid_data = launch_data_dict_lookup(
        process_info, LAUNCH_JOBKEY_PID);
    if (pid_data == NULL) {
      launch_data_free(process_info);
      return true;
    }
    launch_data_free(process_info);
    mozc::Util::Sleep(500);
  }

  // Then trials are all failed.
  return false;
}
}  // anonymous namespace

bool UserHistoryTransition::DoTransition(
    const string &deprecated_file, bool remove_when_done) {
  if (deprecated_file.empty()) {
    LOG(ERROR) << "deprecated_file is not specified";
    return false;
  }

  if (!mozc::Util::FileExists(deprecated_file)) {
    LOG(ERROR) << "the specified deprecated_file does not exist";
    return false;
  }

  mozc::mac::DeprecatedUserHistoryStorage deprecated_storage(deprecated_file);
  if (!deprecated_storage.Load()) {
    LOG(INFO) << "Failed to load the deprecated data.  It means that the user "
              << "explicitly does not allow the access of keychain, or the "
              << "storage is already new.  Anyway we can't go deeper.";
    return false;
  }

  LOG(INFO) << "successfully load the deprecated storage with "
            << deprecated_storage.entries_size() << " entries";

  mozc::UserHistoryStorage storage(
      mozc::UserHistoryPredictor::GetUserHistoryFileName());
  if (!storage.Load()) {
    LOG(INFO) << "Failed to load the user history data.  It means that the "
              << "existing data is formatted in the deprecated way, but "
              << "it's okay to proceed in such case to convert from the "
              << "old format to new";
  } else {
    LOG(INFO) << "loaded the current storage with "
              << storage.entries_size() << " entries";
  }

  vector<const mozc::user_history_predictor::UserHistory *> history;
  history.push_back(&deprecated_storage);
  history.push_back(&storage);

  mozc::UserHistoryStorage new_storage(
      mozc::UserHistoryPredictor::GetUserHistoryFileName());
  mozc::sync::UserHistorySyncUtil::MergeUpdates(history, &new_storage);
  LOG(INFO) << "Merged storage with "
            << new_storage.entries_size() << " entries";
  if (new_storage.Save()) {
    LOG(INFO) << "Saved";
    if (StopConverter()) {
      if (remove_when_done) {
        mozc::Util::Unlink(deprecated_file);
      }
    } else {
      LOG(WARNING) << "Failed to stop converter.  Does not remove the file "
                   << "for safety";
    }
  }

  return true;
}
}  // namespace mozc
