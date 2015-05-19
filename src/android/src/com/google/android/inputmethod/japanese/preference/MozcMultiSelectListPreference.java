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

package org.mozc.android.inputmethod.japanese.preference;

import org.mozc.android.inputmethod.japanese.R;

import android.app.Dialog;
import android.app.AlertDialog.Builder;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnMultiChoiceClickListener;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.res.TypedArray;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.preference.DialogPreference;
import android.util.AttributeSet;

/**
 * Preference widget with a dialog containing multi selectable items.
 *
 */
public class MozcMultiSelectListPreference extends DialogPreference {

  /**
   * State of the current dialog.
   */
  private static class SavedState extends BaseSavedState {
    boolean isDialogShowing;
    Bundle dialogBundle;

    public SavedState(Parcel source) {
      super(source);
      isDialogShowing = (source.readInt() != 0);
      dialogBundle = source.readBundle();
    }

    public SavedState(Parcelable superState) {
      super(superState);
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
      super.writeToParcel(dest, flags);
      dest.writeInt(isDialogShowing ? 1 : 0);
      dest.writeBundle(dialogBundle);
    }

    @SuppressWarnings({"hiding", "unused"})
    public static final Creator<SavedState> CREATOR = new Creator<SavedState>() {
      @Override
      public SavedState createFromParcel(Parcel in) {
        return new SavedState(in);
      }

      @Override
      public SavedState[] newArray(int size) {
        return new SavedState[size];
      }
    };
  }

  /** A list of entries shown on the dialog. */
  private CharSequence[] entryList;

  /** A list of SharedPreferences' keys. */
  private CharSequence[] keyList;

  /** A list of current values. */
  private boolean[] valueList;

  /** A copy of values for dialog. This may be just discarded when user clicks "cancel." */
  private boolean[] dialogValueList;

  public MozcMultiSelectListPreference(Context context, AttributeSet attrs) {
    super(context, attrs);
    initialize(attrs);
  }

  public MozcMultiSelectListPreference(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
    initialize(attrs);
  }

  private void initialize(AttributeSet attrs) {
    TypedArray a =
        getContext().obtainStyledAttributes(attrs, R.styleable.MozcMultiSelectListPreference);
    entryList = a.getTextArray(R.styleable.MozcMultiSelectListPreference_entries);
    keyList = a.getTextArray(R.styleable.MozcMultiSelectListPreference_entryKeys);
    a.recycle();
  }

  public void setKeys(CharSequence[] keyList) {
    if (isDialogShowing()) {
      throw new IllegalStateException("Keys cannot be set when dialog is showing.");
    }
    this.keyList = keyList;
  }

  public void setEntries(CharSequence[] entryList) {
    if (isDialogShowing()) {
      throw new IllegalStateException("Entries cannot be set when dialog is showing.");
    }
    this.entryList = entryList;
  }

  public void setValues(boolean[] valueList) {
    if (isDialogShowing()) {
      throw new IllegalStateException("Values cannot be set when dialog is showing.");
    }
    boolean[] oldValueList = this.valueList;
    this.valueList = valueList.clone();
    persistBooleanArray(keyList, oldValueList, valueList);
  }

  public boolean[] getValues() {
    return valueList.clone();
  }

  private boolean persistBooleanArray(
      CharSequence[] keyList, boolean[] oldValueList, boolean[] valueList) {
    if (!shouldPersist()) {
      return false;
    }

    Editor editor = getSharedPreferences().edit();
    for (int i = 0; i < keyList.length; ++i) {
      if (oldValueList == null || oldValueList[i] != valueList[i]) {
        editor.putBoolean(keyList[i].toString(), valueList[i]);
      }
    }
    editor.commit();

    return true;
  }

  @Override
  protected boolean shouldPersist() {
    // Look at keyList instead of Key.
    return getPreferenceManager() != null && isPersistent() && keyList != null;
  }

  private boolean isDialogShowing() {
    Dialog dialog = getDialog();
    return dialog != null && dialog.isShowing();
  }

  @Override
  protected void onPrepareDialogBuilder(Builder builder) {
    super.onPrepareDialogBuilder(builder);
    if (entryList == null || keyList == null || valueList == null) {
      throw new IllegalStateException();
    }

    if (entryList.length != keyList.length || entryList.length != valueList.length) {
      throw new IllegalStateException(
          "All entryList, keyList and valueList must have the same number of elements: "
              + entryList.length + ", " + keyList.length + ", " + valueList.length);
    }

    // Set multi selectable items and its handler.
    dialogValueList = valueList.clone();
    builder.setMultiChoiceItems(entryList, dialogValueList, new OnMultiChoiceClickListener() {
      @Override
      public void onClick(DialogInterface dialog, int which, boolean isChecked) {
        dialogValueList[which] = isChecked;
      }
    });
  }

  @Override
  protected void onDialogClosed(boolean positiveResult) {
    super.onDialogClosed(positiveResult);
    if (positiveResult) {
      // If user tap OK, set the value.
      setValues(dialogValueList);
    }
  }

  @Override
  protected Object onGetDefaultValue(TypedArray a, int index) {
    return toBooleanArray(a.getTextArray(index));
  }

  @Override
  protected void onSetInitialValue(boolean restoreValue, Object defaultValue) {
    if (!shouldPersist()) {
      // restoreValue may be the result of look up for the key, but in this class
      // what we need to check is keyList.
      setValues((boolean[]) defaultValue);
      return;
    }

    // Use persisted values if exist, otherwise use default value.
    boolean[] valueList = ((boolean[]) defaultValue).clone();
    SharedPreferences sharedPreferences = getSharedPreferences();
    for (int i = 0; i < keyList.length; ++i) {
      if (sharedPreferences.contains(keyList[i].toString())) {
        valueList[i] = sharedPreferences.getBoolean(keyList[i].toString(), valueList[i]);
      }
    }
    setValues(valueList);
  }

  /** Parses each text in {@code array} to boolean value, and returns as an array. */
  private static boolean[] toBooleanArray(CharSequence[] array) {
    if (array == null) {
      return null;
    }
    boolean[] result = new boolean[array.length];
    for (int i = 0; i < result.length; ++i) {
      if (array[i] != null) {
        result[i] = Boolean.parseBoolean(array[i].toString());
      }
    }
    return result;
  }

  @Override
  protected Parcelable onSaveInstanceState() {
    Parcelable superState = super.onSaveInstanceState();
    if (!isDialogShowing()) {
      return superState;
    }

    // Now the dialog is showing. Keep the current state.
    SavedState state = new SavedState(superState);
    state.isDialogShowing = true;
    state.dialogBundle = getDialog().onSaveInstanceState();
    return state;
  }

  @Override
  protected void onRestoreInstanceState(Parcelable parcelable) {
    if (!(parcelable instanceof SavedState)) {
      super.onRestoreInstanceState(parcelable);
      return;
    }

    SavedState state = SavedState.class.cast(parcelable);
    super.onRestoreInstanceState(state.getSuperState());
    if (state.isDialogShowing) {
      // The dialog was shown when the state was saved. Re-show the dialog.
      showDialog(state.dialogBundle);
    }
  }
}
