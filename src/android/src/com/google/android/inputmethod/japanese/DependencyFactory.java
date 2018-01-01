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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.preference.MozcFragmentPreferenceActivity;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.ImeSwitcher;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.app.Activity;
import android.content.Context;
import android.os.Build;

/**
 * Factory class for dependency.
 *
 * Here "Depenency" is for "Dependency Injection".
 * Dependency is resolved on runtime.
 * For example preference activity is dependent on the API level.
 * Previously the same thing is done by string resources (create instances by reflection).
 * But it is hard to test and sometime cause crash bugs which are found after external launch.
 */
public class DependencyFactory {


  /**
   * Dependencies.
   */
  public interface Dependency {

    /**
     * Creates a ViewManager.
     */
    public ViewManagerInterface createViewManager(
        Context context,
        ViewEventListener listener,
        SymbolHistoryStorage symbolHistoryStorage,
        ImeSwitcher imeSwitcher,
        MenuDialogListener menuDialogListener);

    /**
     * Returns a class for preference activity.
     */
    public Class<? extends Activity> getPreferenceActivityClass();

    public boolean isWelcomeActivityPreferrable();
  }

  /**
   * Dependency for standard (with fragment) UI.
   */
  @VisibleForTesting
  static final Dependency TOUCH_FRAGMENT_PREF = new Dependency() {

    @Override
    public ViewManagerInterface createViewManager(Context context, ViewEventListener listener,
        SymbolHistoryStorage symbolHistoryStorage, ImeSwitcher imeSwitcher,
        MenuDialogListener menuDialogListener) {
      return new ViewManager(
          Preconditions.checkNotNull(context),
          Preconditions.checkNotNull(listener),
          Preconditions.checkNotNull(symbolHistoryStorage),
          Preconditions.checkNotNull(imeSwitcher),
          Preconditions.checkNotNull(menuDialogListener));
    }

    @Override
    public Class<? extends Activity> getPreferenceActivityClass() {
      return MozcFragmentPreferenceActivity.class;
    }

    @Override
    public boolean isWelcomeActivityPreferrable() {
      return true;
    }
  };


  // For testing
  private static Optional<Dependency> dependencyForTesting = Optional.absent();

  @VisibleForTesting
  public static void setDependency(Optional<Dependency> dependency) {
    Preconditions.checkNotNull(dependency);
    dependencyForTesting = dependency;
  }

  public static Dependency getDependency(Context context) {
    Preconditions.checkNotNull(context);
    if (dependencyForTesting.isPresent()) {
      return dependencyForTesting.get();
    }
    return TOUCH_FRAGMENT_PREF;
  }

  private DependencyFactory() {}
}
