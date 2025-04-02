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

#include "dictionary/user_dictionary_session_handler.h"

#include <cstdint>
#include <memory>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "base/protobuf/repeated_ptr_field.h"
#include "dictionary/user_dictionary_session.h"
#include "dictionary/user_dictionary_util.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace user_dictionary {

bool UserDictionarySessionHandler::Evaluate(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  if (!command.has_type()) {
    return false;
  }

  switch (command.type()) {
    case UserDictionaryCommand::NO_OPERATION:
      NoOperation(command, status);
      break;
    case UserDictionaryCommand::CREATE_SESSION:
      CreateSession(command, status);
      break;
    case UserDictionaryCommand::DELETE_SESSION:
      DeleteSession(command, status);
      break;
    case UserDictionaryCommand::SET_DEFAULT_DICTIONARY_NAME:
      SetDefaultDictionaryName(command, status);
      break;
    case UserDictionaryCommand::CHECK_UNDOABILITY:
      CheckUndoability(command, status);
      break;
    case UserDictionaryCommand::UNDO:
      Undo(command, status);
      break;
    case UserDictionaryCommand::LOAD:
      Load(command, status);
      break;
    case UserDictionaryCommand::SAVE:
      Save(command, status);
      break;
    case UserDictionaryCommand::CLEAR_STORAGE:
      ClearStorage(command, status);
      break;
    case UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST:
      GetUserDictionaryNameList(command, status);
      break;
    case UserDictionaryCommand::GET_ENTRY_SIZE:
      GetEntrySize(command, status);
      break;
    case UserDictionaryCommand::GET_ENTRIES:
      GetEntries(command, status);
      break;
    case UserDictionaryCommand::CHECK_NEW_DICTIONARY_AVAILABILITY:
      CheckNewDictionaryAvailability(command, status);
      break;
    case UserDictionaryCommand::CREATE_DICTIONARY:
      CreateDictionary(command, status);
      break;
    case UserDictionaryCommand::DELETE_DICTIONARY:
      DeleteDictionary(command, status);
      break;
    case UserDictionaryCommand::RENAME_DICTIONARY:
      RenameDictionary(command, status);
      break;
    case UserDictionaryCommand::CHECK_NEW_ENTRY_AVAILABILITY:
      CheckNewEntryAvailability(command, status);
      break;
    case UserDictionaryCommand::ADD_ENTRY:
      AddEntry(command, status);
      break;
    case UserDictionaryCommand::EDIT_ENTRY:
      EditEntry(command, status);
      break;
    case UserDictionaryCommand::DELETE_ENTRY:
      DeleteEntry(command, status);
      break;
    case UserDictionaryCommand::IMPORT_DATA:
      ImportData(command, status);
      break;
    case UserDictionaryCommand::GET_STORAGE:
      GetStorage(command, status);
      break;
    default:
      status->set_status(UserDictionaryCommandStatus::UNKNOWN_COMMAND);
      break;
  }

  return true;
}

void UserDictionarySessionHandler::NoOperation(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
}

void UserDictionarySessionHandler::ClearStorage(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  // Note: session_ might not be created when ClearStorage is called.  So create
  // a local session to clear the storage.
  UserDictionarySession session(dictionary_path_);
  session.ClearDictionariesAndUndoHistory();
  status->set_status(session.Save());
}

void UserDictionarySessionHandler::CreateSession(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  uint64_t new_id = CreateNewSessionId();

  session_id_ = new_id;
  session_ = std::make_unique<UserDictionarySession>(dictionary_path_);

  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  // Returns the created session's id.
  status->set_session_id(new_id);
}

void UserDictionarySessionHandler::DeleteSession(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  session_id_ = kInvalidSessionId;
  session_.reset();
  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
}

void UserDictionarySessionHandler::SetDefaultDictionaryName(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_name()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  status->set_status(
      session->SetDefaultDictionaryName(command.dictionary_name()));
}

void UserDictionarySessionHandler::CheckUndoability(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  status->set_status(
      session->has_undo_history()
          ? UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS
          : UserDictionaryCommandStatus::NO_UNDO_HISTORY);
}

void UserDictionarySessionHandler::Undo(const UserDictionaryCommand &command,
                                        UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  status->set_status(session->Undo());
}

void UserDictionarySessionHandler::Load(const UserDictionaryCommand &command,
                                        UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (command.ensure_non_empty_storage()) {
    status->set_status(session->LoadWithEnsuringNonEmptyStorage());
  } else {
    status->set_status(session->Load());
  }
}

void UserDictionarySessionHandler::Save(const UserDictionaryCommand &command,
                                        UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  status->set_status(session->Save());
}

void UserDictionarySessionHandler::GetUserDictionaryNameList(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  const UserDictionaryStorage &storage = session->storage();
  UserDictionaryStorage *result_storage = status->mutable_storage();
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    const UserDictionary &dictionary = storage.dictionaries(i);
    UserDictionary *result_dictionary = result_storage->add_dictionaries();
    if (dictionary.has_id()) {
      result_dictionary->set_id(dictionary.id());
    }
    if (dictionary.has_name()) {
      result_dictionary->set_name(dictionary.name());
    }
  }

  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
}

void UserDictionarySessionHandler::GetEntrySize(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_id()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  const UserDictionary *dictionary = UserDictionaryUtil::GetUserDictionaryById(
      session_->storage(), command.dictionary_id());
  if (dictionary == nullptr) {
    status->set_status(UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);
    return;
  }

  status->set_entry_size(dictionary->entries_size());
  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
}

void UserDictionarySessionHandler::GetEntries(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_id() || command.entry_index_size() <= 0) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  const UserDictionary *dictionary = UserDictionaryUtil::GetUserDictionaryById(
      session_->storage(), command.dictionary_id());
  if (dictionary == nullptr) {
    status->set_status(UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);
    return;
  }

  const int entry_index_size = command.entry_index_size();
  for (int i = 0; i < entry_index_size; ++i) {
    const int entry_index = command.entry_index(i);
    if (entry_index < 0 || dictionary->entries_size() <= entry_index) {
      status->set_status(UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE);
      return;
    }
  }

  mozc::protobuf::RepeatedPtrField<UserDictionary::Entry> *mutable_entries =
      status->mutable_entries();
  mutable_entries->Clear();
  mutable_entries->Reserve(entry_index_size);
  for (int i = 0; i < entry_index_size; ++i) {
    const int entry_index = command.entry_index(i);
    *mutable_entries->Add() = dictionary->entries(entry_index);
  }
  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
}

void UserDictionarySessionHandler::CheckNewDictionaryAvailability(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (UserDictionaryUtil::IsStorageFull(session->storage())) {
    status->set_status(
        UserDictionaryCommandStatus::DICTIONARY_SIZE_LIMIT_EXCEEDED);
    return;
  }

  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
}

void UserDictionarySessionHandler::CreateDictionary(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_name()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  uint64_t new_dictionary_id;
  status->set_status(
      session->CreateDictionary(command.dictionary_name(), &new_dictionary_id));
  if (status->status() ==
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    status->set_dictionary_id(new_dictionary_id);
  }
}

void UserDictionarySessionHandler::DeleteDictionary(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_id()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  if (command.ensure_non_empty_storage()) {
    status->set_status(session->DeleteDictionaryWithEnsuringNonEmptyStorage(
        command.dictionary_id()));
  } else {
    status->set_status(session->DeleteDictionary(command.dictionary_id()));
  }
}

void UserDictionarySessionHandler::RenameDictionary(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_id() || !command.has_dictionary_name()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  status->set_status(session->RenameDictionary(command.dictionary_id(),
                                               command.dictionary_name()));
}

void UserDictionarySessionHandler::CheckNewEntryAvailability(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_id()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  const UserDictionary *dictionary = UserDictionaryUtil::GetUserDictionaryById(
      session->storage(), command.dictionary_id());
  if (dictionary == nullptr) {
    status->set_status(UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);
    return;
  }

  if (UserDictionaryUtil::IsDictionaryFull(*dictionary)) {
    status->set_status(UserDictionaryCommandStatus::ENTRY_SIZE_LIMIT_EXCEEDED);
    return;
  }

  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
}

void UserDictionarySessionHandler::AddEntry(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_id() || !command.has_entry()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  status->set_status(
      session->AddEntry(command.dictionary_id(), command.entry()));
}

void UserDictionarySessionHandler::EditEntry(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_id() || command.entry_index_size() != 1 ||
      !command.has_entry()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  status->set_status(session->EditEntry(
      command.dictionary_id(), command.entry_index(0), command.entry()));
}

void UserDictionarySessionHandler::DeleteEntry(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!command.has_dictionary_id() || command.entry_index_size() == 0) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  status->set_status(session->DeleteEntry(
      command.dictionary_id(),
      {command.entry_index().begin(), command.entry_index().end()}));
}

void UserDictionarySessionHandler::ImportData(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }

  if (!(command.has_dictionary_id() || command.has_dictionary_name()) ||
      !command.has_data()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return;
  }

  uint64_t dictionary_id = 0;
  UserDictionaryCommandStatus::Status result_status;
  if (command.has_dictionary_id()) {
    result_status =
        session->ImportFromString(command.dictionary_id(), command.data());
    if (result_status != UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID) {
      dictionary_id = command.dictionary_id();
    }
  } else {
    result_status = session->ImportToNewDictionaryFromString(
        command.dictionary_name(), command.data(), &dictionary_id);
  }
  if (result_status == UserDictionaryCommandStatus::IMPORT_INVALID_ENTRIES &&
      command.ignore_invalid_entries()) {
    LOG(INFO) << "There are some invalid entries but ignored.";
    result_status =
        UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
  }

  if (dictionary_id != 0) {
    status->set_dictionary_id(dictionary_id);
  }
  status->set_status(result_status);
}

void UserDictionarySessionHandler::GetStorage(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  UserDictionarySession *session = GetSession(command, status);
  if (session == nullptr) {
    return;
  }
  *status->mutable_storage() = session->storage();
  status->set_status(
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
}

UserDictionarySession *UserDictionarySessionHandler::GetSession(
    const UserDictionaryCommand &command, UserDictionaryCommandStatus *status) {
  if (!command.has_session_id()) {
    status->set_status(UserDictionaryCommandStatus::INVALID_ARGUMENT);
    return nullptr;
  }

  if (session_ == nullptr || session_id_ != command.session_id()) {
    status->set_status(UserDictionaryCommandStatus::UNKNOWN_SESSION_ID);
    return nullptr;
  }

  return session_.get();
}

uint64_t UserDictionarySessionHandler::CreateNewSessionId() const {
  uint64_t id = kInvalidSessionId;
  absl::BitGen bitgen;
  while (true) {
    id = absl::Uniform<uint64_t>(bitgen);

    if (id != kInvalidSessionId && (session_ == nullptr || session_id_ != id)) {
      // New id is generated.
      break;
    }

    LOG(WARNING) << "User Dictionary Session Id " << id
                 << " is already used. Retry.";
  }

  DCHECK_NE(0, id);
  return id;
}

}  // namespace user_dictionary
}  // namespace mozc
