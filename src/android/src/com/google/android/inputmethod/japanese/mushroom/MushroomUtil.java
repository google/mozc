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

package org.mozc.android.inputmethod.japanese.mushroom;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import java.util.List;

/**
 * Utilities to handle Mushroom protocol.
 *
 */
public class MushroomUtil {

  // Constants to handle Mushroom applications.
  public static final String ACTION = "com.adamrocker.android.simeji.ACTION_INTERCEPT";
  public static final String CATEGORY = "com.adamrocker.android.simeji.REPLACE";
  public static final String KEY = "replace_key";
  public static final String FIELD_ID = "field_id";

  // Disallow instantiation.
  private MushroomUtil() {
  }

  /**
   * @return the List of applications which support Mushroom protocol.
   */
  public static List<ResolveInfo> getMushroomApplicationList(PackageManager packageManager) {
    Intent intent = new Intent();
    intent.setAction(ACTION);
    intent.addCategory(CATEGORY);
    return packageManager.queryIntentActivities(intent, 0);
  }

  /**
   * @return an {@code Intent} to launch {@code MushroomSelectionActivity} with
   * the given parameters.
   */
  public static Intent createMushroomSelectionActivityLaunchingIntent(
      Context context, int fieldId, String replaceKey) {
    Intent intent = new Intent(context, MushroomSelectionActivity.class);
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    intent.putExtra(FIELD_ID, fieldId);
    intent.putExtra(KEY, replaceKey);
    return intent;
  }

  /**
   * @return an {@code Intent} to launch the given {@code packageName, name} with {@code replaceKey}.
   */
  public static Intent createMushroomLaunchingIntent(
      String packageName, String name, String replaceKey) {
    Intent intent = new Intent(ACTION);
    intent.setComponent(new ComponentName(packageName, name));
    intent.addCategory(CATEGORY);
    intent.putExtra(KEY, replaceKey);
    return intent;
  }

  public static String getReplaceKey(Intent intent) {
    if (intent == null) {
      return null;
    }
    return intent.getStringExtra(KEY);
  }

  /**
   * @return the id of the field in which the result to be filled. Or, {@code -1} for error.
   */
  public static int getFieldId(Intent intent) {
    if (intent == null) {
      return -1;
    }
    return intent.getIntExtra(FIELD_ID, -1);
  }

  /**
   * Sends the result from Mushroom Application to ImeService via MushroomResultProxy.
   */
  static void sendReplaceKey(Intent originalIntent, Intent resultIntent) {
    // Retrieve fieldId from the original intent.
    int fieldId = MushroomUtil.getFieldId(originalIntent);
    if (fieldId == -1) {
      return;
    }

    String result = MushroomUtil.getReplaceKey(resultIntent);
    if (result == null) {
      return;
    }

    // Because this activity doesn't know the MozcService instance we cannot talk to the instance
    // directly. Also, binding doesn't work as it is InputMethodService, unfortunately.
    // As a workaround, the result will be sent via MushroomResultProxy.
    MushroomResultProxy resultProxy = MushroomResultProxy.getInstance();
    synchronized (resultProxy) {
      resultProxy.addReplaceKey(fieldId, result);
    }
  }
}
