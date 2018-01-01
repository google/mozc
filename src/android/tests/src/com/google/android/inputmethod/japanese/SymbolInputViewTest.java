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

import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolMajorCategory;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;

import android.app.AlertDialog;
import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.View.OnClickListener;

import java.util.Collections;

/**
 */
public class SymbolInputViewTest extends InstrumentationTestCaseWithMock {
  // TODO(hidehiko): Add tests for switchToPreviousCandidateView for b/5772179, as it's much
  //   difficult to test the method, and now we don't have the test for it.
  //   Refactoring, which I'm working on, would make it possible,
  //   but fixing the bug is prioritiezed.

  @SmallTest
  public void testMajorCategoryButtonClickListener() {
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);

    for (SymbolMajorCategory majorCategory : SymbolMajorCategory.values()) {
      resetAll();
      symbolInputView.setMajorCategory(majorCategory);
      replayAll();

      symbolInputView.new MajorCategoryButtonClickListener(majorCategory).onClick(null);

      verifyAll();
    }
  }

  @SmallTest
  public void testMajorCategoryButtonClickListenerForNoneProvider() {
    Context context = getInstrumentation().getTargetContext();
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);

    symbolInputView.emojiEnabled = true;
    symbolInputView.emojiProviderType = EmojiProviderType.NONE;
    // Set sharedPreferences null to emulate detection failure.
    symbolInputView.sharedPreferences = null;

    AlertDialog dialog = createDialogMock(AlertDialog.class);
    symbolInputView.emojiProviderDialog = Optional.of(dialog);

    symbolInputView.maybeInitializeEmojiProviderDialog(context);
    expect(symbolInputView.getWindowToken()).andReturn(null);
    // The dialog should be shown.
    dialog.show();
    replayAll();

    symbolInputView.new MajorCategoryButtonClickListener(SymbolMajorCategory.EMOJI).onClick(null);

    verifyAll();
    assertSame(EmojiProviderType.NONE, symbolInputView.emojiProviderType);
  }

  private void checkConsistency(SymbolInputView view) {
    SymbolMajorCategory major = view.currentMajorCategory;
    for (SymbolMajorCategory category : SymbolMajorCategory.values()) {
      // Check if only the selected major category's button is "selected" and "enabled".
      View majorCategoryButton = view.getMajorCategoryButton(category);
      assertEquals(category == major, majorCategoryButton.isSelected());
      assertEquals(category != major, majorCategoryButton.isEnabled());
    }
  }

  @SmallTest
  public void testSetMajorCategoryWithEmptyHistory() {
    SymbolInputView view = new SymbolInputView(getInstrumentation().getTargetContext());
    SymbolHistoryStorage historyStorage = createMock(SymbolHistoryStorage.class);
    expect(historyStorage.getAllHistory(isA(SymbolMajorCategory.class)))
        .andStubReturn(Collections.<String>emptyList());
    replayAll();
    view.setSymbolCandidateStorage(new SymbolCandidateStorage(historyStorage));
    view.inflateSelf();
    view.reset();

    ViewEventListener listener = createMock(ViewEventListener.class);
    view.setEventListener(
        listener, createNiceMock(OnClickListener.class), createNiceMock(OnClickListener.class));

    checkConsistency(view);

    // NUMBER
    resetAll();
    replayAll();

    view.setMajorCategory(SymbolMajorCategory.NUMBER);

    verifyAll();
    checkConsistency(view);

    // The others
    for (SymbolMajorCategory category : SymbolMajorCategory.values()) {
      if (category == SymbolMajorCategory.NUMBER) {
        continue;
      }

      resetAll();
      expect(historyStorage.getAllHistory(category)).andReturn(Collections.<String>emptyList());
      listener.onSubmitPreedit();
      replayAll();

      view.setMajorCategory(category);

      verifyAll();
      checkConsistency(view);
      assertEquals(category.toString(), 1, view.getTabHost().getCurrentTab());
    }
  }

  @SmallTest
  public void testSetMajorCategoryWithHistory() {
    SymbolInputView view = new SymbolInputView(getInstrumentation().getTargetContext());
    SymbolHistoryStorage historyStorage = createMock(SymbolHistoryStorage.class);
    expect(historyStorage.getAllHistory(isA(SymbolMajorCategory.class)))
        .andStubReturn(Collections.<String>emptyList());
    replayAll();
    view.setSymbolCandidateStorage(new SymbolCandidateStorage(historyStorage));
    view.inflateSelf();
    view.reset();

    ViewEventListener listener = createMock(ViewEventListener.class);
    view.setEventListener(
        listener, createNiceMock(OnClickListener.class), createNiceMock(OnClickListener.class));
    checkConsistency(view);

    // NUMBER
    resetAll();
    replayAll();

    view.setMajorCategory(SymbolMajorCategory.NUMBER);

    verifyAll();
    checkConsistency(view);

    // The others
    for (final SymbolMajorCategory category : SymbolMajorCategory.values()) {
      if (category == SymbolMajorCategory.NUMBER) {
        continue;
      }

      resetAll();
      expect(historyStorage.getAllHistory(category))
          .andReturn(Collections.singletonList(category.name()));
      listener.onFireFeedbackEvent(FeedbackEvent.SYMBOL_INPUTVIEW_MINOR_CATEGORY_SELECTED);
      expectLastCall().asStub();
      listener.onSubmitPreedit();
      replayAll();

      view.setMajorCategory(category);

      verifyAll();
      checkConsistency(view);
      assertEquals(category.toString(), 0, view.getTabHost().getCurrentTab());
    }
  }

  @SmallTest
  public void testSymbolCandidateSelectListener() {
    SymbolInputView view = new SymbolInputView(getInstrumentation().getTargetContext());
    SymbolHistoryStorage historyStorage = createMock(SymbolHistoryStorage.class);
    expect(historyStorage.getAllHistory(isA(SymbolMajorCategory.class)))
        .andStubReturn(Collections.<String>emptyList());
    replayAll();
    view.setSymbolCandidateStorage(new SymbolCandidateStorage(historyStorage));

    view.inflateSelf();
    view.setMajorCategory(SymbolMajorCategory.EMOTICON);

    ViewEventListener viewEventListener = createMock(ViewEventListener.class);
    view.setEventListener(viewEventListener, createNiceMock(OnClickListener.class),
        createNiceMock(OnClickListener.class));

    resetAll();
    viewEventListener.onSymbolCandidateSelected(SymbolMajorCategory.EMOTICON, "(^_^)", true);
    replayAll();

    view.new SymbolCandidateSelectListener().onCandidateSelected(
        CandidateWord.newBuilder().setValue("(^_^)").buildPartial(), Optional.<Integer>absent());

    verifyAll();
  }

  @SmallTest
  public void testEnableEmoji() {
    Context context = getInstrumentation().getTargetContext();
    SymbolInputView view = new SymbolInputView(context);
    SymbolHistoryStorage historyStorage = createMock(SymbolHistoryStorage.class);
    expect(historyStorage.getAllHistory(isA(SymbolMajorCategory.class)))
        .andStubReturn(Collections.<String>emptyList());
    replayAll();
    view.setSymbolCandidateStorage(new SymbolCandidateStorage(historyStorage));
    view.inflateSelf();

    class TestData extends Parameter {
      private final boolean isUnicodeEmojiEnabled;
      private final boolean isCarrierEmojiEnabled;
      private final SymbolMajorCategory majorCategory;
      private final int expectedEmojiDisabledMessageViewVisibility;

      TestData(boolean isUnicodeEmojiEnabled, boolean isCarrierEmojiEnabled,
               SymbolMajorCategory majorCategory, int expectedEmojiDisabledMessageViewVisibility) {
        this.isUnicodeEmojiEnabled = isUnicodeEmojiEnabled;
        this.isCarrierEmojiEnabled = isCarrierEmojiEnabled;
        this.majorCategory = majorCategory;
        this.expectedEmojiDisabledMessageViewVisibility =
            expectedEmojiDisabledMessageViewVisibility;
      }
    }
    TestData[] testDataList = {
        new TestData(true, true, SymbolMajorCategory.SYMBOL, View.GONE),
        new TestData(true, true, SymbolMajorCategory.EMOTICON, View.GONE),
        new TestData(true, true, SymbolMajorCategory.EMOJI, View.GONE),
        new TestData(true, false, SymbolMajorCategory.SYMBOL, View.GONE),
        new TestData(true, false, SymbolMajorCategory.EMOTICON, View.GONE),
        new TestData(true, false, SymbolMajorCategory.EMOJI, View.GONE),
        new TestData(false, true, SymbolMajorCategory.SYMBOL, View.GONE),
        new TestData(false, true, SymbolMajorCategory.EMOTICON, View.GONE),
        new TestData(false, true, SymbolMajorCategory.EMOJI, View.GONE),
        new TestData(false, false, SymbolMajorCategory.SYMBOL, View.GONE),
        new TestData(false, false, SymbolMajorCategory.EMOTICON, View.GONE),
        new TestData(false, false, SymbolMajorCategory.EMOJI, View.VISIBLE),
    };

    for (TestData testData : testDataList) {
      view.setEmojiEnabled(testData.isUnicodeEmojiEnabled, testData.isCarrierEmojiEnabled);
      view.setMajorCategory(testData.majorCategory);
      // Now, the emoji major-category button is always clickable.
      assertTrue(testData.toString(),
                 view.getMajorCategoryButton(SymbolMajorCategory.EMOJI).isClickable());
      assertEquals(testData.toString(),
                   testData.expectedEmojiDisabledMessageViewVisibility,
                   view.getEmojiDisabledMessageView().getVisibility());
    }
  }
}
