// Copyright 2010-2018, Google Inc.
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

package org.mozc.android.inputmethod.japanese.session;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import com.google.common.base.Preconditions;
import com.google.protobuf.InvalidProtocolBufferException;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager.NameNotFoundException;

import java.io.File;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Concrete SessionHandler. Calls JNI.
 *
 */
class LocalSessionHandler implements SessionHandler {

  private static final String USER_PROFILE_DIRECTORY_NAME = ".mozc";

  @Override
  public void initialize(Context context) {
    try {
      ApplicationInfo info = Preconditions.checkNotNull(context).getApplicationInfo();

      // Ensure the user profile directory exists.
      File userProfileDirectory = new File(info.dataDir, USER_PROFILE_DIRECTORY_NAME);
      if (!userProfileDirectory.exists()) {
        // No profile directory is found. Create the one.
        if (!userProfileDirectory.mkdirs()) {
          // Failed to create a directory. The mozc conversion engine will be able to run
          // even in this case, but no persistent data (e.g. user history, user dictionary)
          // will be stored, so some fuctions using them won't work well.
          MozcLog.e(
              "Failed to create user profile directory: " + userProfileDirectory.getAbsolutePath());
        }
      }

      // Get Java package's version name, to check the version consistency with libmozc.so
      // Note that obtained version name is suffixed by android architecture (e.g., -arm).
      String versionName =
          context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
      Matcher matcher = Pattern.compile("^(\\d+\\.\\d+\\.\\d+\\.\\d+)-\\w+$").matcher(versionName);
      if (!matcher.matches()) {
        throw new RuntimeException("Invalid version name: " + versionName);
      }

      // Load the shared object.
      MozcJNI.load(userProfileDirectory.getAbsolutePath(), null, matcher.group(1));
    } catch (NameNotFoundException e) {
      throw new RuntimeException(e);
    }
  }

  @Override
  public Command evalCommand(Command command) {
    byte[] inBytes = Preconditions.checkNotNull(command).toByteArray();
    byte[] outBytes = null;
    outBytes = MozcJNI.evalCommand(inBytes);
    try {
      return Command.parseFrom(outBytes);
    } catch (InvalidProtocolBufferException e) {
      MozcLog.w("InvalidProtocolBufferException is thrown."
          + "We can do nothing so just return default instance.");
      MozcLog.w(e.toString());
      return Command.getDefaultInstance();
    }
  }
}
