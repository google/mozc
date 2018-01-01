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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Annotation;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates.Candidate;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Category;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit;
import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.ui.FloatingCandidateLayoutRenderer;
import org.mozc.android.inputmethod.japanese.ui.FloatingModeIndicator;
import org.mozc.android.inputmethod.japanese.util.CursorAnchorInfoWrapper;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.InputType;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.widget.PopupWindow;

/**
 * Test for FloatingCandidateView.
 */
@ApiLevel(21)
@TargetApi(21)
public class FloatingCandidateViewTest extends InstrumentationTestCaseWithMock {

  private class FloatingCandidateLayoutRendererMock extends FloatingCandidateLayoutRenderer {

    private Optional<Rect> windowRect = Optional.absent();

    public FloatingCandidateLayoutRendererMock(Resources resources) {
      super(resources);
    }

    public void setWindowRect(Optional<Rect> rect) {
      this.windowRect = Preconditions.checkNotNull(rect);
    }

    @Override
    public Optional<Rect> getWindowRect() {
      if (windowRect.isPresent()) {
        return Optional.of(new Rect(windowRect.get()));
      }
      return Optional.absent();
    }
  }

  private Command generateCommandsProto(
      int candidatesNum, int highlightedPosition, Category category) {
    Preconditions.checkArgument(0 <= candidatesNum && candidatesNum < 10);
    Preconditions.checkNotNull(category);

    Candidates.Builder candidatesBuilder = Candidates.newBuilder();
    for (int i = 0; i < candidatesNum; ++i) {
      candidatesBuilder.addCandidate(Candidate.newBuilder()
          .setIndex(i)
          .setValue("cand_" + Integer.toString(i))
          .setId(i)
          .setAnnotation(Annotation.newBuilder().setShortcut(Integer.toString(i + 1))));
    }
    candidatesBuilder.setSize(candidatesNum);
    candidatesBuilder.setPosition(0);
    candidatesBuilder.setCategory(category);

    return Command.newBuilder()
        .setOutput(Output.newBuilder()
            .setCandidates(candidatesBuilder)
            .setPreedit(Preedit.newBuilder()
                .setHighlightedPosition(highlightedPosition)
                .setCursor(0)))
        .buildPartial();
  }

  /**
   * Pop-up window mock to avoid BadTokenException on {@link PopupWindow#showAtLocation} on test.
   */
  private class PopupWindowMock extends PopupWindow {
    private boolean isShowing;

    @Override
    public void showAtLocation(View parent, int gravity, int x, int y) {
      isShowing = true;
    }

    @Override
    public void update(int left, int top, int right, int bottom) {
    }

    @Override
    public void dismiss() {
      isShowing = false;
    }

    @Override
    public boolean isShowing() {
      return isShowing;
    }
  }

  @SmallTest
  public void testCandidateWindowPositionWithMock() {
    Context context = getInstrumentation().getTargetContext();
    Resources resources = context.getResources();
    FloatingCandidateLayoutRendererMock layoutRendererMock =
        new FloatingCandidateLayoutRendererMock(resources);
    int windowVerticalMargin =
        resources.getDimensionPixelSize(R.dimen.floating_candidate_window_vertical_margin);
    int windowHorizontalMargin =
        resources.getDimensionPixelSize(R.dimen.floating_candidate_window_horizontal_margin);

    FloatingModeIndicator modeIndicatorMock = createMockBuilder(FloatingModeIndicator.class)
        .withConstructor(createViewMockBuilder(View.class).createNiceMock())
        .createNiceMock();
    FloatingCandidateView view = new FloatingCandidateView(
        context, new PopupWindowMock(), layoutRendererMock, modeIndicatorMock);

    // A candidate window is invisible.
    layoutRendererMock.setWindowRect(Optional.<Rect>absent());
    view.layout(0, 0, 1000, 2000);
    assertFalse(view.getVisibleRect().isPresent());

    // A candidate window is visible.
    Rect windowRect = new Rect(-10, 0, 90, 50);
    layoutRendererMock.setWindowRect(Optional.of(windowRect));

    Command command = generateCommandsProto(1, 0, Category.CONVERSION);
    view.setCandidates(command);

    // No composition. (insertion marker location only)
    CursorAnchorInfo.Builder builder = new CursorAnchorInfo.Builder();
    builder.setInsertionMarkerLocation(200, 1000, 250, 1020,
                                       CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION);
    builder.setMatrix(new Matrix());
    view.setCursorAnchorInfo(new CursorAnchorInfoWrapper(builder.build()));
    Rect rect = view.getVisibleRect().get();
    assertEquals(190, rect.left);
    assertEquals(1020 + windowVerticalMargin, rect.top);
    assertEquals(windowRect.width(), rect.width());
    assertEquals(windowRect.height(), rect.height());

    // One composition character.
    // The left corner of a candidate window touches view.
    builder.addCharacterBounds(0, 0, 1000, 100, 1020,
                               CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION);
    builder.setComposingText(0, "a");
    view.setCursorAnchorInfo(new CursorAnchorInfoWrapper(builder.build()));
    rect = view.getVisibleRect().get();
    assertEquals(windowHorizontalMargin, rect.left);
    assertEquals(1020 + windowVerticalMargin, rect.top);
    assertEquals(windowRect.width(), rect.width());
    assertEquals(windowRect.height(), rect.height());

    // Two composition characters. (1st character is highlighted)
    // The left corner of a candidate window touches view.
    builder.addCharacterBounds(1, 100, 1000, 150, 1020,
                               CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION);
    view.setCursorAnchorInfo(new CursorAnchorInfoWrapper(builder.build()));
    rect = view.getVisibleRect().get();
    assertEquals(windowHorizontalMargin, rect.left);
    assertEquals(1020 + windowVerticalMargin, rect.top);
    assertEquals(windowRect.width(), rect.width());
    assertEquals(windowRect.height(), rect.height());

    // Two composition characters (2nd character is highlighted)
    command = generateCommandsProto(1, 1, Category.CONVERSION);
    view.setCandidates(command);
    rect = view.getVisibleRect().get();
    assertEquals(90, rect.left);
    assertEquals(1020 + windowVerticalMargin, rect.top);
    assertEquals(windowRect.width(), rect.width());
    assertEquals(windowRect.height(), rect.height());

    // Change view width.
    // The right corner of a candidate window touches view.
    view.layout(0, 0, 50 + windowRect.width(), 2000);
    rect = view.getVisibleRect().get();
    assertEquals(50 - windowHorizontalMargin, rect.left);
    assertEquals(1020 + windowVerticalMargin, rect.top);
    assertEquals(windowRect.width(), rect.width());
    assertEquals(windowRect.height(), rect.height());

    // Change view height.
    // A candidate window is over a composition text.
    view.layout(0, 0, 50 + windowRect.width(), 1020);
    rect = view.getVisibleRect().get();
    assertEquals(50 - windowHorizontalMargin, rect.left);
    assertEquals(1000 - windowRect.height() - windowVerticalMargin, rect.top);
    assertEquals(windowRect.width(), rect.width());
    assertEquals(windowRect.height(), rect.height());
  }

  @SmallTest
  public void testCandidateWindowPositionWithoutMock() {
    FloatingCandidateView view = new FloatingCandidateView(
        getInstrumentation().getTargetContext(), new PopupWindowMock());
    view.layout(0, 0, 1000, 2000);

    assertFalse(view.getVisibleRect().isPresent());

    view.setCandidates(generateCommandsProto(1, 0, Category.CONVERSION));
    assertTrue(view.getVisibleRect().isPresent());

    view.setCandidates(generateCommandsProto(0, 0, Category.CONVERSION));
    assertFalse(view.getVisibleRect().isPresent());
  }

  @SmallTest
  public void testSuppressSuggestion() {
    FloatingCandidateView view = new FloatingCandidateView(
        getInstrumentation().getTargetContext(), new PopupWindowMock());
    view.layout(0, 0, 1000, 2000);

    Command suggestionCommand = generateCommandsProto(1, 0, Category.SUGGESTION);
    Command conversionCommand = generateCommandsProto(1, 0, Category.CONVERSION);

    view.setCandidates(suggestionCommand);
    assertTrue(view.getVisibleRect().isPresent());

    class Data extends Parameter {
      public final int inputClass;
      public final int flags;
      public final int variations;
      public final boolean expectedSuppressSuggestions;
      public Data(int inputClass, int flags, int variations, boolean expectedSuppressSuggestions) {
        Preconditions.checkArgument(inputClass == (inputClass & InputType.TYPE_MASK_CLASS));
        Preconditions.checkArgument(flags == (flags & InputType.TYPE_MASK_FLAGS));
        Preconditions.checkArgument(variations == (variations & InputType.TYPE_MASK_VARIATION));
        this.inputClass = inputClass;
        this.flags = flags;
        this.variations = variations;
        this.expectedSuppressSuggestions = expectedSuppressSuggestions;
      }
    }

    Data testDataArray[] = {
        new Data(InputType.TYPE_CLASS_DATETIME, 0, InputType.TYPE_TEXT_VARIATION_NORMAL, true),
        new Data(InputType.TYPE_CLASS_DATETIME, 0, InputType.TYPE_TEXT_VARIATION_PASSWORD, true),
        new Data(InputType.TYPE_CLASS_DATETIME, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, true),
        new Data(InputType.TYPE_CLASS_DATETIME, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_PASSWORD, true),

        new Data(InputType.TYPE_CLASS_NUMBER, 0, InputType.TYPE_TEXT_VARIATION_NORMAL, true),
        new Data(InputType.TYPE_CLASS_NUMBER, 0, InputType.TYPE_TEXT_VARIATION_PASSWORD, true),
        new Data(InputType.TYPE_CLASS_NUMBER, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, true),
        new Data(InputType.TYPE_CLASS_NUMBER, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_PASSWORD, true),

        new Data(InputType.TYPE_CLASS_PHONE, 0, InputType.TYPE_TEXT_VARIATION_NORMAL, true),
        new Data(InputType.TYPE_CLASS_PHONE, 0, InputType.TYPE_TEXT_VARIATION_PASSWORD, true),
        new Data(InputType.TYPE_CLASS_PHONE, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, true),
        new Data(InputType.TYPE_CLASS_PHONE, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_PASSWORD, true),

        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_NORMAL, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_PASSWORD, true),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, true),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_PASSWORD, true),

        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_AUTO_COMPLETE,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, true),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_AUTO_CORRECT,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, false),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, false),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_CAP_SENTENCES,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, false),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_CAP_WORDS,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, false),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_IME_MULTI_LINE,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, false),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_MULTI_LINE,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, false),
        new Data(InputType.TYPE_CLASS_TEXT, InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS,
                 InputType.TYPE_TEXT_VARIATION_NORMAL, true),

        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS, true),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_EMAIL_SUBJECT, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_FILTER, true),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_LONG_MESSAGE, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_NORMAL, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_PASSWORD, true),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_PERSON_NAME, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_PHONETIC, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_SHORT_MESSAGE, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_URI, true),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD,
                 true),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT, false),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS,
                 true),
        new Data(InputType.TYPE_CLASS_TEXT, 0, InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD, true),
    };

    for (Data data : testDataArray) {
    EditorInfo editorInfo = new EditorInfo();
      editorInfo.inputType = data.inputClass | data.flags | data.variations;
      view.setEditorInfo(editorInfo);
      view.setCandidates(suggestionCommand);
      assertEquals(data.toString(),
                   data.expectedSuppressSuggestions, !view.getVisibleRect().isPresent());

      view.setCandidates(conversionCommand);
      assertTrue(view.getVisibleRect().isPresent());
    }
  }
}
