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

package org.mozc.android.inputmethod.japanese;

import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.DependencyFactory.Dependency;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.mushroom.MushroomUtil;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;
import org.mozc.android.inputmethod.japanese.util.ImeSwitcherFactory.ImeSwitcher;
import com.google.common.base.Optional;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.inputmethodservice.InputMethodService;
import android.test.mock.MockContext;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.easymock.Capture;

/**
 * Test for MozcMenuDialogListenerImpl.
 *
 */
public class MozcMenuDialogListenerImplTest extends InstrumentationTestCaseWithMock {
  private Context context;
  private InputMethodService inputMethodService;
  private MenuDialogListener listener;

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    context = createMock(MockContext.class);
    inputMethodService = createMock(InputMethodService.class);
    listener = new MozcMenuDialogListenerImpl(inputMethodService);
  }

  @Override
  protected void tearDown() throws Exception {
    // Try to cancel input method picker showing in case.
    MozcUtil.cancelShowInputMethodPicker(context);

    listener = null;
    inputMethodService = null;
    context = null;
    super.tearDown();
  }

  @SmallTest
  public void testCancel() {
    replayAll();

    listener.onShow(context);
    listener.onDismiss(context);

    verifyAll();
    // Shouldn't request input method picker dialog showing.
    assertFalse(MozcUtil.hasShowInputMethodPickerRequest(context));
  }

  @SmallTest
  public void testShowInputMethodPickerSelected() {
    replayAll();

    listener.onShow(context);
    listener.onShowInputMethodPickerSelected(context);
    listener.onDismiss(context);

    verifyAll();
    // The input method picker should be shown.
    assertTrue(MozcUtil.hasShowInputMethodPickerRequest(context));
  }

  @SmallTest
  public void testLaunchPreferenceActivitySelected() {
    final ViewManagerInterface stubViewManager = createNiceMock(ViewManagerInterface.class);
    final Activity stubPreferenceActivity = createNiceMock(Activity.class);
    final Activity stubSoftwareKeyboardAdvancedSettingActivity = createNiceMock(Activity.class);

    Context context = createNiceMock(Context.class);
    String packageName = "test.package.name";

    try {
      DependencyFactory.setDependency(
          Optional.<Dependency>of(new Dependency(){

            @Override
            public ViewManagerInterface createViewManager(Context context,
                ViewEventListener listener, SymbolHistoryStorage symbolHistoryStorage,
                ImeSwitcher imeSwitcher, MenuDialogListener menuDialogListener) {
              return stubViewManager;
            }

            @Override
            public Class<? extends Activity> getPreferenceActivityClass() {
              return stubPreferenceActivity.getClass();
            }

            @Override
            public Optional<Class<? extends Activity>>
                getSoftwareKeyboardAdvancedSettingActivityClass() {
              return Optional.<Class<? extends Activity>>of(
                  stubSoftwareKeyboardAdvancedSettingActivity.getClass());
            }

            @Override
            public boolean isWelcomeActivityPreferrable() {
              return true;
            }
          }));
      Capture<Intent> intentCapture = new Capture<Intent>();
      expect(context.getPackageName()).andStubReturn(packageName);
      context.startActivity(capture(intentCapture));
      replayAll();

      listener.onShow(context);
      listener.onLaunchPreferenceActivitySelected(context);
      listener.onDismiss(context);

      verifyAll();

      // Make sure that the preference activity is launched.
      Intent intent = intentCapture.getValue();
      assertEquals(stubPreferenceActivity.getClass().getName(),
                   intent.getComponent().getClassName());
      assertEquals(packageName, intent.getComponent().getPackageName());
      assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
    } finally {
      DependencyFactory.setDependency(Optional.<Dependency>absent());
    }
  }

  @SmallTest
  public void testShowMushroomSelectionDialogSelected() {
    InputConnection baseConnection = createNiceMock(InputConnection.class);
    ComposingTextTrackingInputConnection inputConnection =
        new ComposingTextTrackingInputConnection(baseConnection);
    expect(inputMethodService.getCurrentInputConnection()).andStubReturn(inputConnection);
    EditorInfo editorInfo = new EditorInfo();
    editorInfo.fieldId = 10;
    expect(inputMethodService.getCurrentInputEditorInfo()).andStubReturn(editorInfo);
    expect(context.getPackageName()).andStubReturn("org.mozc.android.inputmethod.japanese");
    Capture<Intent> intentCapture = new Capture<Intent>();
    context.startActivity(capture(intentCapture));
    replayAll();

    inputConnection.setComposingText("abcde", MozcUtil.CURSOR_POSITION_TAIL);

    listener.onShow(context);
    listener.onShowMushroomSelectionDialogSelected(context);
    listener.onDismiss(context);

    verifyAll();

    // Make sure the MushroomSelectionActivity is launched with appropriate arguments.
    Intent intent = intentCapture.getValue();
    ComponentName componentName = intent.getComponent();
    assertEquals(
        "org.mozc.android.inputmethod.japanese", componentName.getPackageName());
    assertEquals(
        "org.mozc.android.inputmethod.japanese.mushroom.MushroomSelectionActivity",
        componentName.getClassName());
    assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
    assertEquals(10, MushroomUtil.getFieldId(intent));
    assertEquals("abcde", MushroomUtil.getReplaceKey(intent));

    // The composing text should be reset.
    assertEquals("", inputConnection.getComposingText());
  }
}
