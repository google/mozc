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

package org.mozc.android.inputmethod.japanese.mushroom;

import org.mozc.android.inputmethod.japanese.resources.R;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

/**
 * This is the activity to select a Mushroom application to be launched.
 * Also, this class proxies the {@code replace_key} between MozcService and
 * the mushroom application.
 *
 */
public class MushroomSelectionActivity extends Activity {

  /**
   * ListAdapter to use custom view (Application Icon followed by Application Name)
   * for ListView entry.
   */
  static class MushroomApplicationListAdapter extends ArrayAdapter<ResolveInfo> {
    MushroomApplicationListAdapter(Context context) {
      super(context, 0, 0, MushroomUtil.getMushroomApplicationList(context.getPackageManager()));
    }

    @Override
    public View getView(int position, View contentView, ViewGroup parent) {
      if(contentView == null) {
        LayoutInflater inflater = LayoutInflater.from(getContext());
        contentView = inflater.inflate(R.layout.mushroom_selection_dialog_entry, parent, false);
      }

      // Set appropriate application icon and its name.
      PackageManager packageManager = getContext().getPackageManager();
      ResolveInfo resolveInfo = getItem(position);
      ImageView icon =
          ImageView.class.cast(contentView.findViewById(R.id.mushroom_application_icon));
      icon.setImageDrawable(resolveInfo.loadIcon(packageManager));
      TextView text =
          TextView.class.cast(contentView.findViewById(R.id.mushroom_application_label));
      text.setText(resolveInfo.loadLabel(packageManager));
      return contentView;
    }
  }

  /**
   * ClickListener to launch the target activity.
   */
  static class MushroomApplicationListClickListener implements OnItemClickListener {
    private Activity activity;

    MushroomApplicationListClickListener(Activity activity) {
      this.activity = activity;
    }

    @Override
    public void onItemClick(AdapterView<?> adapter, View view, int position, long id) {
      ResolveInfo resolveInfo = ResolveInfo.class.cast(adapter.getItemAtPosition(position));
      ActivityInfo activityInfo = resolveInfo.activityInfo;
      activity.startActivityForResult(
          MushroomUtil.createMushroomLaunchingIntent(
              activityInfo.packageName, activityInfo.name,
              MushroomUtil.getReplaceKey(activity.getIntent())),
          REQUEST_CODE);
    }
  }

  static final int REQUEST_CODE = 1;

  @Override
  protected void onCreate(Bundle savedInstance) {
    super.onCreate(savedInstance);
    setContentView(R.layout.mushroom_selection_dialog);

    ListView view = ListView.class.cast(findViewById(R.id.mushroom_selection_list_view));
    view.setOnItemClickListener(new MushroomApplicationListClickListener(this));
  }

  @Override
  protected void onResume() {
    super.onResume();

    // Reset application list for every onResume.
    // It is because this activity is launched in singleTask mode, so that the onCreate may be
    // skipped for second (or later) launching.
    ListView view = ListView.class.cast(findViewById(R.id.mushroom_selection_list_view));
    view.setAdapter(new MushroomApplicationListAdapter(this));
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    if (requestCode == REQUEST_CODE && resultCode == RESULT_OK) {
      MushroomUtil.sendReplaceKey(getIntent(), data);
    }
    finish();
  }
}
