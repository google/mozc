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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoUserDictionaryStorage.UserDictionaryCommandStatus.Status;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.content.Context;
import android.widget.Toast;

import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;

/**
 * Manages toast message, especially, resolves when we want to try to show a new toast message
 * during another toast message is showing.
 *
 */
public class ToastManager {

  /**
   * Mapping from Status to the resource id.
   */
  private static Map<Status, Integer> ERROR_MESSAGE_MAP;
  static {
    EnumMap<Status, Integer> map = new EnumMap<Status, Integer>(Status.class);
    map.put(Status.FILE_NOT_FOUND, R.string.user_dictionary_tool_status_error_file_not_found);
    map.put(Status.INVALID_FILE_FORMAT,
            R.string.user_dictionary_tool_status_error_invalid_file_format);
    map.put(Status.FILE_SIZE_LIMIT_EXCEEDED,
            R.string.user_dictionary_tool_status_error_file_size_limit_exceeded);
    map.put(Status.DICTIONARY_SIZE_LIMIT_EXCEEDED,
            R.string.user_dictionary_tool_status_error_dictionary_size_limit_exceeded);
    map.put(Status.ENTRY_SIZE_LIMIT_EXCEEDED,
            R.string.user_dictionary_tool_status_error_entry_size_limit_exceeded);
    map.put(Status.DICTIONARY_NAME_EMPTY,
            R.string.user_dictionary_tool_status_error_dictionary_name_empty);
    map.put(Status.DICTIONARY_NAME_TOO_LONG,
            R.string.user_dictionary_tool_status_error_dictionary_name_too_long);
    map.put(Status.DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER,
            R.string.user_dictionary_tool_status_error_dictionary_name_contains_invalid_character);
    map.put(Status.DICTIONARY_NAME_DUPLICATED,
            R.string.user_dictionary_tool_status_error_dictionary_name_duplicated);
    map.put(Status.READING_EMPTY, R.string.user_dictionary_tool_status_error_reading_empty);
    map.put(Status.READING_TOO_LONG, R.string.user_dictionary_tool_status_error_reading_too_long);
    map.put(Status.READING_CONTAINS_INVALID_CHARACTER,
            R.string.user_dictionary_tool_status_error_reading_contains_invalid_character);
    map.put(Status.WORD_EMPTY, R.string.user_dictionary_tool_status_error_word_empty);
    map.put(Status.WORD_TOO_LONG, R.string.user_dictionary_tool_status_error_word_too_long);
    map.put(Status.WORD_CONTAINS_INVALID_CHARACTER,
            R.string.user_dictionary_tool_status_error_word_contains_invalid_character);
    map.put(Status.IMPORT_TOO_MANY_WORDS,
            R.string.user_dictionary_tool_status_error_import_too_many_words);
    map.put(Status.IMPORT_INVALID_ENTRIES,
            R.string.user_dictionary_tool_status_error_import_invalid_entries);
    map.put(Status.NO_UNDO_HISTORY, R.string.user_dictionary_tool_status_error_no_undo_history);
    ERROR_MESSAGE_MAP = Collections.unmodifiableMap(map);
  }

  private final Context context;
  private final Toast toast;

  public ToastManager(Context context) {
    this.context = context;
    this.toast = new Toast(context);
  }

  /**
   * Displays the message of the {@code resourceId} with short duration.
   */
  public void showMessageShortly(int resourceId) {
    showMessageShortlyInternal(resourceId);
  }

  /**
   * Displays the message for the {@code status} with short duration.
   */
  public void maybeShowMessageShortly(Status status) {
    if (status == Status.USER_DICTIONARY_COMMAND_SUCCESS) {
      return;
    }

    Integer value = ERROR_MESSAGE_MAP.get(status);
    if (value == null) {
      showMessageShortlyInternal(R.string.user_dictionary_tool_status_error_general);
    } else {
      showMessageShortlyInternal(value.intValue());
    }
  }

  private void showMessageShortlyInternal(int resourceId) {
    // In order to resolve the conflicting of the showing Toast message,
    // we hack-up the Toast and its View.
    // First, creat a dummy toast message, in order to delegate the creation of its View.
    Toast toast = Toast.makeText(context, resourceId, Toast.LENGTH_SHORT);

    // Delegate the View. Note that it seems to ok to set it even if toast is currently visible.
    this.toast.setView(toast.getView());
    toast.setView(null);

    // Again, invoke the show method, so that the Toast message will gradually change to the
    // new one.
    this.toast.show();
  }
}
