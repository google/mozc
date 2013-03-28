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

package org.mozc.android.inputmethod.japanese.userdictionary;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionary.Entry;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionary.PosType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommand;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommand.CommandType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommandStatus;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommandStatus.Status;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryStorage;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor;

import android.net.Uri;

import java.util.AbstractList;
import java.util.List;
import java.util.zip.ZipFile;

/**
 */
public class UserDictionaryToolModel {
  private final SessionExecutor sessionExecutor;
  private long sessionId;

  private UserDictionaryStorage storage = null;
  private long selectedDictionaryId = 0;

  // TODO(hidehiko): Move this bit into the server side.
  private boolean dirty = false;

  // Unfortunately our target API level 7 doesn't support to passing any arguments to the dialog.
  // Thus, as a workaround, we relay the edit target entry's index by this model.
  private int editTargetIndex;

  // Pending status of import data.
  private Uri importUri;
  private String importData;
  private ZipFile zipFile;

  // List "view" by proxying dictionary names in the storage.
  private final List<String> dictionaryNameList = new AbstractList<String>() {
    @Override
    public String get(int index) {
      return storage.getDictionaries(index).getName();
    }

    @Override
    public int size() {
      return (storage == null) ? 0 : storage.getDictionariesCount();
    }
  };

  public UserDictionaryToolModel(SessionExecutor sessionExecutor) {
    this.sessionExecutor = sessionExecutor;
  }

  /**
   * Returns {@code true} if the storage is edited after loading.
   */
  public boolean isDirty() {
    return dirty;
  }

  /**
   * Creates a session to communicate with the native server about user dictionary editing.
   * This method should be called before any other methods.
   */
  public void createSession() {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.CREATE_SESSION)
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      throw new IllegalStateException("UserDictionaryCommand session should be created always.");
    }
    sessionId = status.getSessionId();
  }

  /**
   * Deletes the current session.
   */
  public void deleteSession() {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.DELETE_SESSION)
        .setSessionId(sessionId)
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      MozcLog.e("Failed to delete user dictionary command session.");
    }
    sessionId = 0;
  }

  /**
   * Resumes the current session by loading the data from storage,
   * and updates storage and selected id.
   * @params defautlDictionaryName the name of the default dictionary, which is created when,
   *   e.g., LOAD operation is failing, or the storage gets empty because of the deletion of
   *   the last dictionary.
   */
  public Status resumeSession(String defaultDictionaryName) {
    ensureSession();

    // Set default dictionary name.
    {
      UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
          .setType(CommandType.SET_DEFAULT_DICTIONARY_NAME)
          .setSessionId(sessionId)
          .setDictionaryName(defaultDictionaryName)
          .build();
      // Ignore any errors.
      sessionExecutor.sendUserDictionaryCommand(command);
    }

    // Load the dictionary.
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.LOAD)
        .setSessionId(sessionId)
        .setEnsureNonEmptyStorage(true)
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);

    // Update the dictionary list regardless of the result of the LOAD command.
    // Read new dictionary list.
    {
      Status updateStatus = updateStorage();
      if (updateStatus == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
        selectedDictionaryId = storage.getDictionaries(0).getId();
      }
    }

    // Especially on the first time, there are no user dictionary file, so "file not found"
    // is (a kind of) expected behavior. In order not to show "error" message to users,
    // ignore the error here.
    if (status.getStatus() == Status.FILE_NOT_FOUND) {
      return Status.USER_DICTIONARY_COMMAND_SUCCESS;
    }
    return status.getStatus();
  }

  private void ensureSession() {
    // First ping to the mozc server.
    {
      UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
          .setType(CommandType.NO_OPERATION)
          .setSessionId(sessionId)
          .build();
      UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
      if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
        return;
      }
    }

    // The session is somehow broken. Actually, because of Android architecture,
    // we can have multiple Activity instances, but due to resource limitation the current
    // limit of the number of sessions is only 1. So kick the other session out, and recreate
    // our session again.
    createSession();
  }

  /**
   * Treis to save if necessary, and also tries to reload the server.
   */
  public Status pauseSession() {
    checkSession();

    // Save the dictionary if necessary.
    if (dirty) {
      UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
          .setType(CommandType.SAVE)
          .setSessionId(sessionId)
          .build();
      UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
      if (status.getStatus() != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
        return status.getStatus();
      }

      // When the save is succeeded, we need to reload the mozc server.
      sessionExecutor.reload();
    }

    return Status.USER_DICTIONARY_COMMAND_SUCCESS;
  }

  /**
   * Returns the list "view" of dictionary names in the storage.
   * The contents of the returned instance should be automatically updated when the storage
   * is updated.
   */
  public List<String> getDictionaryNameList() {
    return dictionaryNameList;
  }

  /**
   * Sets the current selected dictionary based on the given index.
   */
  public void setSelectedDictionaryByIndex(int index) {
    if (storage == null || index < 0 || storage.getDictionariesCount() <= index) {
      // Invalid state.
      selectedDictionaryId = 0;
      return;
    }

    selectedDictionaryId = storage.getDictionaries(index).getId();
  }

  /**
   * Returns the index of the current selected dictionary.
   */
  public int getSelectedDictionaryIndex() {
    int index = getSelectedDictionaryIndexInternal();
    if (index < 0) {
      return 0;
    }
    return index;
  }

  /**
   * Returns the name of the current selected dictionary.
   */
  public String getSelectedDictionaryName() {
    int index = getSelectedDictionaryIndexInternal();
    if (index < -1) {
      return null;
    }
    return storage.getDictionaries(index).getName();
  }

  private int getSelectedDictionaryIndexInternal() {
    if (storage != null && selectedDictionaryId != 0) {
      for (int i = 0; i < storage.getDictionariesCount(); ++i) {
        if (storage.getDictionaries(i).getId() == selectedDictionaryId) {
          return i;
        }
      }
    }

    // Not found.
    return -1;
  }

  private void checkSession() {
    if (sessionId == 0) {
      throw new IllegalStateException("Session is not yet created.");
    }
  }

  /**
   * Checks if we can add another dictionary to the storage.
   */
  public Status checkNewDictionaryAvailability() {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.CHECK_NEW_DICTIONARY_AVAILABILITY)
        .setSessionId(sessionId)
        .build();
    return sessionExecutor.sendUserDictionaryCommand(command).getStatus();
  }

  public Status checkUndoability() {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.CHECK_UNDOABILITY)
        .setSessionId(sessionId)
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    return status.getStatus();
  }

  public Status undo() {
    int index = getSelectedDictionaryIndex();
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.UNDO)
        .setSessionId(sessionId)
        .build();

    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      dirty = true;

      Status updateStatus = updateStorage();
      if (updateStatus != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
        return updateStatus;
      }

      boolean found = false;
      for (int i = 0; i < storage.getDictionariesCount(); ++i) {
        if (storage.getDictionaries(i).getId() == selectedDictionaryId) {
          found = true;
          break;
        }
      }

      if (!found) {
        setSelectedDictionaryByIndex(Math.min(index, storage.getDictionariesCount() - 1));
      }
    }

    return status.getStatus();
  }

  /**
   * Creates a new empty dictionary with the given dictionary name.
   * Also update the selected dictionary to the created one.
   */
  public Status createDictionary(String dictionaryName) {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.CREATE_DICTIONARY)
        .setSessionId(sessionId)
        .setDictionaryName(dictionaryName)
        .build();
    UserDictionaryCommandStatus status =
        sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      dirty = true;

      Status updateStatus = updateStorage();
      if (updateStatus != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
        return updateStatus;
      }
      selectedDictionaryId = status.getDictionaryId();
    }
    return status.getStatus();
  }

  /**
   * Renames the currently selected dictionary to the given dictionary name.
   */
  public Status renameSelectedDictionary(String dictionaryName) {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.RENAME_DICTIONARY)
        .setSessionId(sessionId)
        .setDictionaryId(selectedDictionaryId)
        .setDictionaryName(dictionaryName)
        .build();
    UserDictionaryCommandStatus status =
        sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      dirty = true;
      Status updateStatus = updateStorage();
      if (updateStatus != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
        return updateStatus;
      }
    }
    return status.getStatus();
  }

  /**
   * Deletes the current selected dictionary.
   * The new selected dictionary should be the "same" position of the dictionaries in the storage.
   * If the dictionary gets empty, no dictionary is selected.
   * If the deleted dictionary is at the end of the storage,
   * the new selected dictionary will be the one at the end of the storage after
   * deleting operation.
   */
  public Status deleteSelectedDictionary() {
    int index = getSelectedDictionaryIndex();
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.DELETE_DICTIONARY)
        .setSessionId(sessionId)
        .setDictionaryId(selectedDictionaryId)
        .setEnsureNonEmptyStorage(true)
        .build();
    UserDictionaryCommandStatus status =
        sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      dirty = true;
      Status updateStatus = updateStorage();
      if (updateStatus != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
        return updateStatus;
      }
      selectedDictionaryId =
          storage.getDictionaries(Math.min(index, storage.getDictionariesCount() - 1)).getId();
    }
    return status.getStatus();
  }

  private Status updateStorage() {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.GET_USER_DICTIONARY_NAME_LIST)
        .setSessionId(sessionId)
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      storage = status.getStorage();
    } else {
      MozcLog.e("Failed to get dictionary name list: " + status.getStatus());
    }
    return status.getStatus();
  }


  /**
   * Returns the list "view" of entries in the current selected dictionary.
   * The contents of the returned instance should be automatically updated when the selected
   * dictionary is updated.
   */
  public List<Entry> getEntryList() {
    return new AbstractList<Entry>() {
      @Override
      public Entry get(int index) {
        return getEntryInternal(index);
      }

      @Override
      public int size() {
        if (selectedDictionaryId == 0) {
          return 0;
        }
        UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
            .setType(CommandType.GET_ENTRY_SIZE)
            .setSessionId(sessionId)
            .setDictionaryId(selectedDictionaryId)
            .build();
        UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
        if (status.getStatus() != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
          MozcLog.e("Unknown failure: " + status.getStatus());
          throw new RuntimeException("Unknown failure");
        }
        return status.getEntrySize();
      }};
  }

  public void setEditTargetIndex(int editTargetIndex) {
    this.editTargetIndex = editTargetIndex;
  }

  public int getEditTargetIndex() {
    return this.editTargetIndex;
  }

  /**
   * Returns the entry at current editTargetIndex in the selected dictionary.
   */
  public Entry getEditTargetEntry() {
    return getEntryInternal(editTargetIndex);
  }

  private Entry getEntryInternal(int index) {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.GET_ENTRY)
        .setSessionId(sessionId)
        .setDictionaryId(selectedDictionaryId)
        .addEntryIndex(index)
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      MozcLog.e("Unknown failure: " + status.getStatus());
      throw new RuntimeException("Unknown failure");
    }
    return status.getEntry();
  }

  /**
   * Checks if we can add another entry to the selected dictionary.
   */
  public Status checkNewEntryAvailability() {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.CHECK_NEW_ENTRY_AVAILABILITY)
        .setSessionId(sessionId)
        .setDictionaryId(selectedDictionaryId)
        .build();
    return sessionExecutor.sendUserDictionaryCommand(command).getStatus();
  }

  /**
   * Adds an entry to the selected dictionary.
   */
  public Status addEntry(String word, String reading, PosType pos) {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.ADD_ENTRY)
        .setSessionId(sessionId)
        .setDictionaryId(selectedDictionaryId)
        .setEntry(Entry.newBuilder()
            .setKey(reading)
            .setValue(word)
            .setPos(pos))
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      dirty = true;
    }
    return status.getStatus();
  }

  /**
   * Edits the entry, which is specified by editTargetIndex, in the selected dictionary.
   */
  public Status editEntry(String word, String reading, PosType pos) {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.EDIT_ENTRY)
        .setSessionId(sessionId)
        .setDictionaryId(selectedDictionaryId)
        .addEntryIndex(editTargetIndex)
        .setEntry(Entry.newBuilder()
            .setKey(reading)
            .setValue(word)
            .setPos(pos))
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      dirty = true;
    }
    return status.getStatus();
  }

  /**
   * Deletes the entries specified by the given {@code indexList} in the selected dictionary.
   */
  public Status deleteEntry(List<Integer> indexList) {
    UserDictionaryCommand command = UserDictionaryCommand.newBuilder()
        .setType(CommandType.DELETE_ENTRY)
        .setSessionId(sessionId)
        .setDictionaryId(selectedDictionaryId)
        .addAllEntryIndex(indexList)
        .build();
    UserDictionaryCommandStatus status = sessionExecutor.sendUserDictionaryCommand(command);
    if (status.getStatus() == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      dirty = true;
    }
    return status.getStatus();
  }

  public Uri getImportUri() {
    return importUri;
  }

  public void setImportUri(Uri uri) {
    this.importUri = uri;
  }

  public String getImportData() {
    return importData;
  }

  public void setImportData(String data) {
    this.importData = data;
  }

  public void resetImportState() {
    importUri = null;
    importData = null;
    clearZipFile();
  }

  public ZipFile getZipFile() {
    return zipFile;
  }

  /**
   * By the method, this instance takes the ownership of resource management of {@code zipFile}.
   * (i.e., it must not to invoke close for the zipFile by the caller).
   */
  public void setZipFile(ZipFile zipFile) {
    clearZipFile();
    this.zipFile = zipFile;
  }

  /**
   * By the method, this instance releases the ownership of resource management of the
   * current ZipFile. I.e., the caller needs to invoke close method if the returned ZipFile
   * instance.
   */
  public ZipFile releaseZipFile() {
    ZipFile zipFile = this.zipFile;
    this.zipFile = null;
    return zipFile;
  }

  private void clearZipFile() {
    MozcUtil.closeIgnoringIOException(zipFile);
    zipFile = null;
  }

  /**
   * Invokes to import the data into a dictionary.
   * After all operations, regardless of whether the operation is successfully done or not,
   * resets the pending import state.
   * @param dictionaryIndex is a position of the import destination dictionary in the storage.
   *   if it is set to -1, this method tries to create a new dictionary with guessing a
   *   dictionary name.
   */
  public Status importData(int dictionaryIndex) {
    try {
      // Both importData and importUri should be set before this method's invocation.
      if (importData == null || importUri == null) {
        throw new NullPointerException();
      }

      UserDictionaryCommand.Builder builder = UserDictionaryCommand.newBuilder()
          .setType(CommandType.IMPORT_DATA)
          .setSessionId(sessionId)
          .setData(importData);
      if (dictionaryIndex < 0) {
        builder.setDictionaryName(
            UserDictionaryUtil.generateDictionaryNameByUri(importUri, dictionaryNameList));
      } else {
        builder.setDictionaryId(storage.getDictionaries(dictionaryIndex).getId());
      }

      UserDictionaryCommandStatus status =
          sessionExecutor.sendUserDictionaryCommand(builder.build());

      // Regardless of the status, set dirty flag to be conservative,
      // because even if the status is not SUCCESS, some entries might be imported.
      dirty = true;
      if (status.hasDictionaryId()) {
        // Update the view.
        Status updateStatus = updateStorage();
        if (updateStatus != Status.USER_DICTIONARY_COMMAND_SUCCESS) {
          return updateStatus;
        }
        selectedDictionaryId = status.getDictionaryId();
      }
      return status.getStatus();
    } finally {
      // Reset the importing state regardless of whether the import operation is succeeded or not.
      resetImportState();
    }
  }
}
