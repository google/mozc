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

import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MajorCategory;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MinorCategory;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage.SymbolHistoryStorage;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.app.AlertDialog;
import android.content.Context;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;

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
    Context context = getInstrumentation().getTargetContext();
    SymbolInputView symbolInputView = createViewMock(SymbolInputView.class);

    for (MajorCategory majorCategory : MajorCategory.values()) {
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
    symbolInputView.emojiProviderDialog = dialog;

    symbolInputView.maybeInitializeEmojiProviderDialog(context);
    expect(symbolInputView.getWindowToken()).andReturn(null);
    // The dialog should be shown.
    dialog.show();
    replayAll();

    symbolInputView.new MajorCategoryButtonClickListener(MajorCategory.EMOJI).onClick(null);

    verifyAll();
    assertSame(EmojiProviderType.NONE, symbolInputView.emojiProviderType);
  }

  @SmallTest
  public void testMajorCategoryGetMajorCategory() {
    for (MinorCategory minorCategory : MinorCategory.values()) {
      if (minorCategory.name().startsWith("EMOTICON")) {
        assertEquals(MajorCategory.EMOTICON, MajorCategory.findMajorCategory(minorCategory));
      } else if (minorCategory.name().startsWith("SYMBOL")) {
        assertEquals(MajorCategory.SYMBOL, MajorCategory.findMajorCategory(minorCategory));
      } else if (minorCategory.name().startsWith("EMOJI")) {
        assertEquals(MajorCategory.EMOJI, MajorCategory.findMajorCategory(minorCategory));
      } else {
        fail("Unexpected category name");
      }
    }
  }

  @SmallTest
  public void testMajorCategoryGetMinorCategoryByRelativeIndex() {
    assertEquals(
        MinorCategory.EMOJI_FACE,
        MajorCategory.EMOJI.getMinorCategoryByRelativeIndex(MinorCategory.EMOJI_HISTORY, +1));
    assertEquals(
        MinorCategory.EMOJI_HISTORY,
        MajorCategory.EMOJI.getMinorCategoryByRelativeIndex(MinorCategory.EMOJI_HISTORY, 0));
    assertEquals(
        MinorCategory.EMOJI_NATURE,
        MajorCategory.EMOJI.getMinorCategoryByRelativeIndex(MinorCategory.EMOJI_HISTORY, -1));
  }

  private void checkConsistency(SymbolInputView view) {
    MajorCategory major = view.currentMajorCategory;
    for (MajorCategory category : MajorCategory.values()) {
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
    expect(historyStorage.getAllHistory(isA(MajorCategory.class)))
        .andStubReturn(Collections.<String>emptyList());
    replayAll();
    view.setSymbolCandidateStorage(new SymbolCandidateStorage(historyStorage));
    view.inflateSelf();
    view.reset();

    ViewEventListener listener = createMock(ViewEventListener.class);
    view.setViewEventListener(listener, null);

    checkConsistency(view);

    for (MajorCategory category : MajorCategory.values()) {
      resetAll();
      expect(historyStorage.getAllHistory(category)).andReturn(Collections.<String>emptyList());
      listener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_EXPAND);
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
    expect(historyStorage.getAllHistory(isA(MajorCategory.class)))
        .andStubReturn(Collections.<String>emptyList());
    replayAll();
    view.setSymbolCandidateStorage(new SymbolCandidateStorage(historyStorage));
    view.inflateSelf();
    view.reset();

    ViewEventListener listener = createMock(ViewEventListener.class);
    view.setViewEventListener(listener, null);
    checkConsistency(view);
    for (final MajorCategory category : MajorCategory.values()) {
      resetAll();
      expect(historyStorage.getAllHistory(category))
          .andReturn(Collections.singletonList(category.name()));
      listener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_EXPAND);
      expectLastCall().asStub();
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
    expect(historyStorage.getAllHistory(isA(MajorCategory.class)))
        .andStubReturn(Collections.<String>emptyList());
    replayAll();
    view.setSymbolCandidateStorage(new SymbolCandidateStorage(historyStorage));

    view.inflateSelf();
    view.setMajorCategory(MajorCategory.EMOTICON);

    ViewEventListener viewEventListener = createMock(ViewEventListener.class);
    view.setViewEventListener(viewEventListener, null);

    resetAll();
    viewEventListener.onSymbolCandidateSelected(MajorCategory.EMOTICON, "(^_^)");
    replayAll();

    view.new SymbolCandidateSelectListener().onCandidateSelected(
        CandidateWord.newBuilder().setValue("(^_^)").buildPartial());

    verifyAll();
  }

  @SmallTest
  public void testEnableEmoji() {
    Context context = getInstrumentation().getTargetContext();
    SymbolInputView view = new SymbolInputView(context);
    SymbolHistoryStorage historyStorage = createMock(SymbolHistoryStorage.class);
    expect(historyStorage.getAllHistory(isA(MajorCategory.class)))
        .andStubReturn(Collections.<String>emptyList());
    replayAll();
    view.setSymbolCandidateStorage(new SymbolCandidateStorage(historyStorage));
    view.inflateSelf();

    class TestData extends Parameter {
      private final boolean isEmojiEnabled;
      private final MajorCategory majorCategory;
      private final int expectedEmojiDisabledMessageViewVisibility;

      TestData(boolean isEmojiEnabled, MajorCategory majorCategory,
               int expectedEmojiDisabledMessageViewVisibility) {
        this.isEmojiEnabled = isEmojiEnabled;
        this.majorCategory = majorCategory;
        this.expectedEmojiDisabledMessageViewVisibility =
            expectedEmojiDisabledMessageViewVisibility;
      }
    }
    TestData[] testDataList = {
        new TestData(true, MajorCategory.SYMBOL, View.GONE),
        new TestData(true, MajorCategory.EMOTICON, View.GONE),
        new TestData(true, MajorCategory.EMOJI, View.GONE),
        new TestData(false, MajorCategory.SYMBOL, View.GONE),
        new TestData(false, MajorCategory.EMOTICON, View.GONE),
        new TestData(false, MajorCategory.EMOJI, View.VISIBLE),
    };

    for (TestData testData : testDataList) {
      view.setEmojiEnabled(testData.isEmojiEnabled);
      view.setMajorCategory(testData.majorCategory);
      // Now, the emoji major-category button is always clickable.
      assertTrue(testData.toString(),
                 view.getMajorCategoryButton(MajorCategory.EMOJI).isClickable());
      assertEquals(testData.toString(),
                   testData.expectedEmojiDisabledMessageViewVisibility,
                   view.getEmojiDisabledMessageView().getVisibility());
    }
  }
}
