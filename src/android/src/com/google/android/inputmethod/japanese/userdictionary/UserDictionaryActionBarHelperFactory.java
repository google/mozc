// Copyright 2010-2014, Google Inc.
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

import org.mozc.android.inputmethod.japanese.resources.R;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;

/**
 * Utility to support Action Bar like UI for all the devices.
 *
 * Android 3.0 (or later) supports a new UI named "Action Bar", and it is recommended to be used.
 * However, Google Japanese Input supports also Android 2.3 or earlier.
 * To provide quite similar user experiences for all the platforms, we implement "mimic" of
 * Action Bar for such earlier devices.
 * On the other hand, on later devices, we use Action Bar itself (supported by Android OS).
 *
 * This class provides utilities to fill the gaps between earlier and later devices.
 *
 */
class UserDictionaryActionBarHelperFactory {

  /**
   * Hook-methods of Activity to support ActionBar like UI for earlier devices.
   */
  interface ActionBarHelper {
    void onCreate(Bundle savedInstance);
    public void onPostCreate(
        Bundle savedInstance,
        OnClickListener addEntryClickListener,
        OnClickListener deleteEntryClickListener,
        OnClickListener undoClickListener);
    void onConfigurationChanged(Configuration configuration);
    void onCreateOptionsMenu(Menu menu);
  }

  /**
   * Implementation of ActionBar like ui for earlier devices.
   */
  private static class MozcActionBarHelper implements ActionBarHelper {
    private final Activity activity;

    MozcActionBarHelper(Activity activity) {
      this.activity = activity;
    }

    @Override
    public void onCreate(Bundle savedInstance) {
      // We'll use "custom title" for the ActionBar like UI.
      activity.getWindow().requestFeature(Window.FEATURE_CUSTOM_TITLE);
    }

    @Override
    public void onPostCreate(Bundle savedInstance,
                             OnClickListener addEntryClickListener,
                             OnClickListener deleteEntryClickListener,
                             OnClickListener undoClickListener) {
      activity.getWindow().setFeatureInt(
          Window.FEATURE_CUSTOM_TITLE,
          R.layout.user_dictionary_tool_action_bar_view);

      activity.findViewById(R.id.user_dictionary_tool_action_bar_add_entry)
          .setOnClickListener(addEntryClickListener);
      activity.findViewById(R.id.user_dictionary_tool_split_action_bar_add_entry)
          .setOnClickListener(addEntryClickListener);
      activity.findViewById(R.id.user_dictionary_tool_action_bar_delete_entry)
          .setOnClickListener(deleteEntryClickListener);
      activity.findViewById(R.id.user_dictionary_tool_split_action_bar_delete_entry)
          .setOnClickListener(deleteEntryClickListener);

      // Need to select either icons (on top action bar, or bottom split action bar)
      // should be shown.
      updateActionBar();
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
      updateActionBar();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu) {
      // These items should be put on the self implemented action bar.
      menu.findItem(R.id.user_dictionary_tool_menu_add_entry).setVisible(false);
      menu.findItem(R.id.user_dictionary_tool_menu_delete_entry).setVisible(false);
    }

    private void updateActionBar() {
      DisplayMetrics metrics = activity.getResources().getDisplayMetrics();
      if (metrics.widthPixels < 480 * metrics.scaledDensity) {
        // Show split action bar. Instead, hide icons on the main action bar.
        activity.findViewById(R.id.user_dictionary_tool_action_bar_add_entry)
            .setVisibility(View.GONE);
        activity.findViewById(R.id.user_dictionary_tool_action_bar_delete_entry)
            .setVisibility(View.GONE);
        activity.findViewById(R.id.user_dictionary_tool_split_action_bar)
            .setVisibility(View.VISIBLE);
      } else {
        // Hide split action bar. Instead, show icons on the main action bar.
        activity.findViewById(R.id.user_dictionary_tool_action_bar_add_entry)
            .setVisibility(View.VISIBLE);
        activity.findViewById(R.id.user_dictionary_tool_action_bar_delete_entry)
            .setVisibility(View.VISIBLE);
        activity.findViewById(R.id.user_dictionary_tool_split_action_bar)
            .setVisibility(View.GONE);
      }
    }
  }

  /**
   * Implement to use system Action Bar (which is supported Android 3.0 or later).
   */
  private static class SystemActionBarHelper implements ActionBarHelper {
    private final Activity activity;

    SystemActionBarHelper(Activity activity) {
      this.activity = activity;
    }

    @Override
    public void onCreate(Bundle savedInstance) {
      activity.getWindow().requestFeature(Window.FEATURE_ACTION_BAR);
    }

    @Override
    public void onPostCreate(Bundle savedInstance,
                             OnClickListener addEntryClickListener,
                             OnClickListener deleteEntryClickListener,
                             OnClickListener undoClickListener) {
      // Disable self implemented action bars.
      activity.findViewById(R.id.user_dictionary_tool_split_action_bar).setVisibility(View.GONE);
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
      // Do nothing.
    }

    @Override
    public void onCreateOptionsMenu(Menu menu) {
      // Do nothing.
    }
  }

  private UserDictionaryActionBarHelperFactory() {
  }

  static ActionBarHelper newInstance(Activity activity) {
    return (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB)
        ? new SystemActionBarHelper(activity)
        : new MozcActionBarHelper(activity);
  }
}
