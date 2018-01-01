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

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardFactory;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardView;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.model.SymbolMajorCategory;
import org.mozc.android.inputmethod.japanese.model.SymbolMinorCategory;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer.DescriptionLayoutPolicy;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer.ValueScalingPolicy;
import org.mozc.android.inputmethod.japanese.ui.ScrollGuideView;
import org.mozc.android.inputmethod.japanese.ui.SpanFactory;
import org.mozc.android.inputmethod.japanese.ui.SymbolCandidateLayouter;
import org.mozc.android.inputmethod.japanese.view.MozcImageButton;
import org.mozc.android.inputmethod.japanese.view.MozcImageView;
import org.mozc.android.inputmethod.japanese.view.RoundRectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.Skin;
import org.mozc.android.inputmethod.japanese.view.SymbolMajorCategoryButtonDrawableFactory;
import org.mozc.android.inputmethod.japanese.view.TabSelectedBackgroundDrawable;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.IBinder;
import android.os.Looper;
import android.preference.PreferenceManager;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.support.v4.view.ViewPager.OnPageChangeListener;
import android.util.AttributeSet;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TabHost;
import android.widget.TabHost.OnTabChangeListener;
import android.widget.TabHost.TabSpec;
import android.widget.TabWidget;
import android.widget.TextView;

import java.util.Collections;
import java.util.List;

/**
 * This class is used to show symbol input view on which an user can
 * input emoticon/symbol/emoji.
 *
 * In this class,
 * "Emoticon" means "Kaomoji", like ＼(^o^)／
 * "Symbol" means symbol character, like $ % ! €
 * "Emoji" means a more graphical character of face, building, food etc.
 *
 * This class treats all Emoticon, Symbol and Emoji category as "SymbolMajorCategory".
 * A major category has several minor categories.
 * Each minor category belongs to only one major category.
 * Major-Minor relation is defined by using R.layout.symbol_minor_category_*.
 *
 */
public class SymbolInputView extends InOutAnimatedFrameLayout implements MemoryManageable {

  /**
   * Adapter for symbol candidate selection.
   */
  // TODO(hidehiko): make this class static.
  @VisibleForTesting class SymbolCandidateSelectListener implements CandidateSelectListener {
    @Override
    public void onCandidateSelected(CandidateWord candidateWord, Optional<Integer> row) {
      Preconditions.checkNotNull(candidateWord);
      // When current major category is NUMBER, CandidateView.ConversionCandidateSelectListener
      // should handle candidate selection event.
      Preconditions.checkState(currentMajorCategory != SymbolMajorCategory.NUMBER);
      if (viewEventListener.isPresent()) {
        // If we are on password field, history shouldn't be updated to protect privacy.
        viewEventListener.get().onSymbolCandidateSelected(
            currentMajorCategory, candidateWord.getValue(), !isPasswordField);
      }
    }
  }

  /**
   * Click handler of major category buttons.
   */
  @VisibleForTesting class MajorCategoryButtonClickListener implements OnClickListener {
    private final SymbolMajorCategory majorCategory;

    MajorCategoryButtonClickListener(SymbolMajorCategory majorCategory) {
      this.majorCategory = Preconditions.checkNotNull(majorCategory);
    }

    @Override
    public void onClick(View majorCategorySelectorButton) {
      if (viewEventListener.isPresent()) {
        viewEventListener.get().onFireFeedbackEvent(
            FeedbackEvent.SYMBOL_INPUTVIEW_MAJOR_CATEGORY_SELECTED);
      }

      if (emojiEnabled
          && majorCategory == SymbolMajorCategory.EMOJI
          && emojiProviderType == EmojiProviderType.NONE) {
        // Ask the user which emoji provider s/he'd like to use.
        // If the user cancels the dialog, do nothing.
        maybeInitializeEmojiProviderDialog(getContext());
        if (emojiProviderDialog.isPresent()) {
          IBinder token = getWindowToken();
          if (token != null) {
            MozcUtil.setWindowToken(token, emojiProviderDialog.get());
          } else {
            MozcLog.w("Unknown window token.");
          }

          // If a user selects a provider, the dialog handler will set major category
          // to EMOJI automatically. If s/he cancels, nothing will be happened.
          emojiProviderDialog.get().show();
          return;
        }
      }

      setMajorCategory(majorCategory);
    }
  }

  private static class SymbolTabWidgetViewPagerAdapter extends PagerAdapter
      implements OnTabChangeListener, OnPageChangeListener {

    private static final int HISTORY_INDEX = 0;

    private final Context context;
    private final SymbolCandidateStorage symbolCandidateStorage;
    private final Optional<ViewEventListener> viewEventListener;
    private final CandidateSelectListener candidateSelectListener;
    private final SymbolMajorCategory majorCategory;
    private Skin skin;
    private final EmojiProviderType emojiProviderType;
    private final TabHost tabHost;
    private final ViewPager viewPager;
    private final float candidateTextSize;
    private final float descriptionTextSize;

    private Optional<View> historyViewCache = Optional.absent();
    private int scrollState = ViewPager.SCROLL_STATE_IDLE;
    private boolean feedbackEnabled = true;

    SymbolTabWidgetViewPagerAdapter(
        Context context, SymbolCandidateStorage symbolCandidateStorage,
        Optional<ViewEventListener> viewEventListener,
        CandidateSelectListener candidateSelectListener, SymbolMajorCategory majorCategory,
        Skin skin, EmojiProviderType emojiProviderType, TabHost tabHost, ViewPager viewPager,
        float candidateTextSize, float descriptionTextSize) {
      this.context = Preconditions.checkNotNull(context);
      this.symbolCandidateStorage = Preconditions.checkNotNull(symbolCandidateStorage);
      this.viewEventListener = Preconditions.checkNotNull(viewEventListener);
      this.candidateSelectListener = Preconditions.checkNotNull(candidateSelectListener);
      this.majorCategory = Preconditions.checkNotNull(majorCategory);
      this.skin = Preconditions.checkNotNull(skin);
      this.emojiProviderType = Preconditions.checkNotNull(emojiProviderType);
      this.tabHost = Preconditions.checkNotNull(tabHost);
      this.viewPager = Preconditions.checkNotNull(viewPager);
      this.candidateTextSize = Preconditions.checkNotNull(candidateTextSize);
      this.descriptionTextSize = Preconditions.checkNotNull(descriptionTextSize);
    }

    public void setSkin(Skin skin) {
      Preconditions.checkNotNull(skin);
      this.skin = skin;
    }

    public void setFeedbackEnabled(boolean enabled) {
      feedbackEnabled = enabled;
    }

    private void maybeResetHistoryView() {
      if (viewPager.getCurrentItem() != HISTORY_INDEX && historyViewCache.isPresent()) {
        resetHistoryView();
      }
    }

    private void resetHistoryView() {
      if (!historyViewCache.isPresent()) {
        return;
      }
      CandidateList candidateList =
          symbolCandidateStorage.getCandidateList(majorCategory.minorCategories.get(0));
      View noHistoryView = historyViewCache.get().findViewById(R.id.symbol_input_no_history);
      if (candidateList.getCandidatesCount() == 0) {
        noHistoryView.setVisibility(View.VISIBLE);
        TextView.class.cast(historyViewCache.get().findViewById(R.id.symbol_input_no_history_text))
            .setTextColor(skin.candidateValueTextColor);
      } else {
        noHistoryView.setVisibility(View.GONE);
      }
      SymbolCandidateView.class.cast(historyViewCache.get().findViewById(
          R.id.symbol_input_candidate_view)).update(candidateList);
    }

    @Override
    public void onPageScrollStateChanged(int state) {
      if (scrollState == ViewPager.SCROLL_STATE_IDLE) {
        maybeResetHistoryView();
      }
      scrollState = state;
    }

    @Override
    public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
      // Do nothing.
    }

    @Override
    public void onPageSelected(int position) {
      tabHost.setOnTabChangedListener(null);
      tabHost.setCurrentTab(position);
      tabHost.setOnTabChangedListener(this);
    }

    @Override
    public void onTabChanged(String tabId) {
      int position = Integer.parseInt(tabId);

      if (position == HISTORY_INDEX) {
        maybeResetHistoryView();
      }
      viewPager.setCurrentItem(position, false);

      if (feedbackEnabled && viewEventListener.isPresent()) {
        viewEventListener.get().onFireFeedbackEvent(
            FeedbackEvent.SYMBOL_INPUTVIEW_MINOR_CATEGORY_SELECTED);
      }
    }

    @Override
    public int getCount() {
      return majorCategory.minorCategories.size();
    }

    @Override
    public boolean isViewFromObject(View view, Object item) {
      return view == item;
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
      View view = LayoutInflater.from(context).inflate(R.layout.symbol_candidate_view, null);
      SymbolCandidateView symbolCandidateView =
          SymbolCandidateView.class.cast(view.findViewById(R.id.symbol_input_candidate_view));
      symbolCandidateView.setCandidateSelectListener(candidateSelectListener);
      symbolCandidateView.setMinColumnWidth(
          context.getResources().getDimension(majorCategory.minColumnWidthResourceId));
      symbolCandidateView.setSkin(skin);
      symbolCandidateView.setEmojiProviderType(emojiProviderType);
      if (majorCategory.layoutPolicy == DescriptionLayoutPolicy.GONE) {
        // As it's guaranteed for descriptions not to be shown,
        // show values using additional space where is reserved for descriptions.
        // This makes Emoji bigger.
        symbolCandidateView.setCandidateTextDimension(candidateTextSize + descriptionTextSize, 0);
      } else {
        symbolCandidateView.setCandidateTextDimension(candidateTextSize, descriptionTextSize);
      }
      symbolCandidateView.setDescriptionLayoutPolicy(majorCategory.layoutPolicy);

      // Set candidate contents.
      if (position == HISTORY_INDEX) {
        historyViewCache = Optional.of(view);
        resetHistoryView();
      } else {
        symbolCandidateView.update(symbolCandidateStorage.getCandidateList(
            majorCategory.minorCategories.get(position)));
        symbolCandidateView.updateScrollPositionBasedOnFocusedIndex();
      }

      ScrollGuideView scrollGuideView =
          ScrollGuideView.class.cast(view.findViewById(R.id.symbol_input_scroll_guide_view));
      scrollGuideView.setSkin(skin);

      // Connect guide and candidate view.
      scrollGuideView.setScroller(symbolCandidateView.scroller);
      symbolCandidateView.setScrollIndicator(scrollGuideView);

      container.addView(view);
      return view;
    }

    @Override
    public void destroyItem(ViewGroup collection, int position, Object view) {
      if (position == HISTORY_INDEX) {
        historyViewCache = Optional.absent();
      }
      collection.removeView(View.class.cast(view));
    }
  }

  /**
   * An event listener for the menu dialog window.
   */
  private class EmojiProviderDialogListener implements DialogInterface.OnClickListener {
    private final Context context;

    EmojiProviderDialogListener(Context context) {
      this.context = Preconditions.checkNotNull(context);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
      String value;
      TypedArray typedArray =
          context.getResources().obtainTypedArray(R.array.pref_emoji_provider_type_values);
      try {
        value = typedArray.getString(which);
      } finally {
        typedArray.recycle();
      }
      sharedPreferences.edit()
          .putString(PreferenceUtil.PREF_EMOJI_PROVIDER_TYPE, value)
          .commit();
      setMajorCategory(SymbolMajorCategory.EMOJI);
    }
  }

  /**
   * The candidate view for SymbolInputView.
   *
   * The differences from CandidateView.CandidateWordViewForConversion are
   * 1) this class scrolls horizontally 2) the layout algorithm is simpler.
   */
  private static class SymbolCandidateView extends CandidateWordView {
    private static final String DESCRIPTION_DELIMITER = "\n";

    private Optional<View> scrollGuideView = Optional.absent();

    public SymbolCandidateView(Context context) {
      super(context, Orientation.VERTICAL);
    }

    public SymbolCandidateView(Context context, AttributeSet attributeSet) {
      super(context, attributeSet, Orientation.VERTICAL);
    }

    public SymbolCandidateView(Context context, AttributeSet attributeSet, int defaultStyle) {
      super(context, attributeSet, defaultStyle, Orientation.VERTICAL);
    }

    // Shared instance initializer.
    {
      setSpanBackgroundDrawableType(DrawableType.SYMBOL_CANDIDATE_BACKGROUND);
      Resources resources = getResources();
      scroller.setDecayRate(
          resources.getInteger(R.integer.symbol_input_scroller_velocity_decay_rate) / 1000000f);
      scroller.setMinimumVelocity(
          resources.getInteger(R.integer.symbol_input_scroller_minimum_velocity));
      layouter = new SymbolCandidateLayouter();
    }

    void setCandidateTextDimension(float textSize, float descriptionTextSize) {
      Preconditions.checkArgument(textSize >= 0);
      Preconditions.checkArgument(descriptionTextSize >= 0);

      Resources resources = getResources();

      float valueHorizontalPadding =
          resources.getDimension(R.dimen.symbol_candidate_horizontal_padding_size);
      float descriptionHorizontalPadding =
          resources.getDimension(R.dimen.symbol_description_right_padding);
      float descriptionVerticalPadding =
          resources.getDimension(R.dimen.symbol_description_bottom_padding);
      float separatorWidth = resources.getDimensionPixelSize(R.dimen.candidate_separator_width);

      carrierEmojiRenderHelper.setCandidateTextSize(textSize);
      candidateLayoutRenderer.setValueTextSize(textSize);
      candidateLayoutRenderer.setValueHorizontalPadding(valueHorizontalPadding);
      candidateLayoutRenderer.setValueScalingPolicy(ValueScalingPolicy.UNIFORM);
      candidateLayoutRenderer.setDescriptionTextSize(descriptionTextSize);
      candidateLayoutRenderer.setDescriptionHorizontalPadding(descriptionHorizontalPadding);
      candidateLayoutRenderer.setDescriptionVerticalPadding(descriptionVerticalPadding);
      candidateLayoutRenderer.setSeparatorWidth(separatorWidth);

      SpanFactory spanFactory = new SpanFactory();
      spanFactory.setValueTextSize(textSize);
      spanFactory.setDescriptionTextSize(descriptionTextSize);
      spanFactory.setDescriptionDelimiter(DESCRIPTION_DELIMITER);

      SymbolCandidateLayouter layouter = SymbolCandidateLayouter.class.cast(this.layouter);
      layouter.setSpanFactory(spanFactory);
      layouter.setRowHeight(resources.getDimensionPixelSize(R.dimen.symbol_view_candidate_height));
    }

    @Override
    SymbolCandidateLayouter getCandidateLayouter() {
      return SymbolCandidateLayouter.class.cast(super.getCandidateLayouter());
    }

    void setMinColumnWidth(float minColumnWidth) {
      getCandidateLayouter().setMinColumnWidth(minColumnWidth);
      updateLayouter();
    }

    @Override
    protected void onDraw(Canvas canvas) {
      super.onDraw(canvas);
      if (scrollGuideView.isPresent()) {
        scrollGuideView.get().invalidate();
      }
    }

    void setScrollIndicator(View scrollGuideView) {
      this.scrollGuideView = Optional.of(scrollGuideView);
    }

    @Override
    protected Drawable getViewBackgroundDrawable(Skin skin) {
      return Preconditions.checkNotNull(skin).symbolCandidateViewBackgroundDrawable;
    }

    @Override
    public void setSkin(Skin skin) {
      super.setSkin(skin);
      candidateLayoutRenderer.setSeparatorColor(skin.symbolCandidateBackgroundSeparatorColor);
    }

    public void setDescriptionLayoutPolicy(DescriptionLayoutPolicy policy) {
      candidateLayoutRenderer.setDescriptionLayoutPolicy(Preconditions.checkNotNull(policy));
    }
  }

  /**
   * Name to represent this view for logging.
   */
  static final KeyboardSpecificationName SPEC_NAME =
      new KeyboardSpecificationName("SYMBOL_INPUT_VIEW", 0, 1, 0);
  // Source ID of the delete/enter button for logging usage stats.
  private static final int DELETE_BUTTON_SOURCE_ID = 1;
  private static final int ENTER_BUTTON_SOURCE_ID = 2;

  private static final int NUM_TABS = 6;

  private Optional<Integer> viewHeight = Optional.absent();
  private Optional<Integer> numberKeyboardHeight = Optional.absent();
  private Optional<Float> keyboardHeightScale = Optional.absent();

  private Optional<SymbolCandidateStorage> symbolCandidateStorage = Optional.absent();

  @VisibleForTesting SymbolMajorCategory currentMajorCategory = SymbolMajorCategory.NUMBER;
  @VisibleForTesting boolean emojiEnabled;
  private boolean isPasswordField;
  @VisibleForTesting EmojiProviderType emojiProviderType = EmojiProviderType.NONE;

  @VisibleForTesting SharedPreferences sharedPreferences;
  @VisibleForTesting Optional<AlertDialog> emojiProviderDialog = Optional.absent();

  private Optional<ViewEventListener> viewEventListener = Optional.absent();
  private final KeyEventButtonTouchListener deleteKeyEventButtonTouchListener =
      createDeleteKeyEventButtonTouchListener(getResources());
  private final KeyEventButtonTouchListener enterKeyEventButtonTouchListener =
      createEnterKeyEventButtonTouchListener(getResources());
  private Optional<OnClickListener> closeButtonClickListener = Optional.absent();
  private Optional<OnClickListener> microphoneButtonClickListener = Optional.absent();
  private final SymbolCandidateSelectListener symbolCandidateSelectListener =
      new SymbolCandidateSelectListener();

  private Skin skin = Skin.getFallbackInstance();
  private final SymbolMajorCategoryButtonDrawableFactory majorCategoryButtonDrawableFactory =
      new SymbolMajorCategoryButtonDrawableFactory(getResources());
  // Candidate text size in dip.
  private float candidateTextSize;
  // Description text size in dip.
  private float desciptionTextSize;
  private Optional<KeyEventHandler> keyEventHandler = Optional.absent();
  private boolean isMicrophoneButtonEnabled;
  private boolean popupEnabled;

  public SymbolInputView(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
  }

  public SymbolInputView(Context context) {
    super(context);
  }

  public SymbolInputView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  {
    sharedPreferences =
        Preconditions.checkNotNull(PreferenceManager.getDefaultSharedPreferences(getContext()));
  }

  private static KeyEventButtonTouchListener createDeleteKeyEventButtonTouchListener(
      Resources resources) {
    return new KeyEventButtonTouchListener(
        DELETE_BUTTON_SOURCE_ID, resources.getInteger(R.integer.uchar_backspace));
  }

  private static KeyEventButtonTouchListener createEnterKeyEventButtonTouchListener(
      Resources resources) {
    return new KeyEventButtonTouchListener(
        ENTER_BUTTON_SOURCE_ID, resources.getInteger(R.integer.uchar_linefeed));
  }

  boolean isInflated() {
    return getChildCount() > 0;
  }

  void inflateSelf() {
    Preconditions.checkState(!isInflated(), "The symbol input view is already inflated.");

    LayoutInflater.from(getContext()).inflate(R.layout.symbol_view, this);
    // Note: onFinishInflate won't be invoked on android ver 3.0 or later, while it is invoked
    // on android 2.3 or earlier. So, we define another (but similar) method and invoke it here
    // manually.
    onFinishInflateSelf();
  }

  /**
   * Initializes the instance. Called only once.
   * {@code onFinishInflate()} is *not* invoked for the inflation of &lt;merge&gt; element we use.
   * So, instead, we define another onFinishInflate method and invoke this manually.
   */
  protected void onFinishInflateSelf() {
    if (viewHeight.isPresent() && keyboardHeightScale.isPresent()) {
      setVerticalDimension(viewHeight.get(), keyboardHeightScale.get());
    }

    initializeNumberKeyboard();
    initializeMinorCategoryTab();
    initializeCloseButton();
    initializeDeleteButton();
    initializeEnterButton();
    initializeMicrophoneButton();

    // Set TouchListener that does nothing. Without this hack, state_pressed event
    // will be propagated to close / enter key and the drawable will be changed to
    // state_pressed one unexpectedly. Note that those keys are NOT children of this view.
    // Setting ClickListener to the key seems to suppress this unexpected highlight, too,
    // but we want to keep the current TouchListener for the enter key.
    OnTouchListener doNothingOnTouchListener = new OnTouchListener() {
      @Override
      public boolean onTouch(View button, android.view.MotionEvent event) {
        return true;
      }
    };
    for (int id : new int[] {R.id.button_frame_in_symbol_view,
                             R.id.symbol_view_backspace_separator,
                             R.id.symbol_major_category,
                             R.id.symbol_separator_1,
                             R.id.symbol_separator_2,
                             R.id.symbol_separator_3,
                             R.id.symbol_view_close_button_separator,
                             R.id.symbol_view_enter_button_separator}) {
      findViewById(id).setOnTouchListener(doNothingOnTouchListener);
    }

    KeyboardView keyboardView = KeyboardView.class.cast(findViewById(R.id.number_keyboard));
    keyboardView.setPopupEnabled(popupEnabled);
    keyboardView.setKeyEventHandler(new KeyEventHandler(
        Looper.getMainLooper(),
        new KeyboardActionListener() {
          @Override
          public void onRelease(int keycode) {
          }

          @Override
          public void onPress(int keycode) {
            if (viewEventListener.isPresent()) {
              viewEventListener.get().onFireFeedbackEvent(FeedbackEvent.KEY_DOWN);
            }
          }

          @Override
          public void onKey(int primaryCode, List<TouchEvent> touchEventList) {
            if (keyEventHandler.isPresent()) {
              keyEventHandler.get().sendKey(primaryCode, touchEventList);
            }
          }

          @Override
          public void onCancel() {
          }
        },
        getResources().getInteger(R.integer.config_repeat_key_delay),
        getResources().getInteger(R.integer.config_repeat_key_interval),
        getResources().getInteger(R.integer.config_long_press_key_delay)));

    enableEmoji(emojiEnabled);

    updateSkinAwareDrawable();
    reset();
  }

  private static void setLayoutHeight(View view, int height) {
    ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
    layoutParams.height = height;
    view.setLayoutParams(layoutParams);
  }

  public void setVerticalDimension(int symbolInputViewHeight, float keyboardHeightScale) {
    this.viewHeight = Optional.of(symbolInputViewHeight);
    this.keyboardHeightScale = Optional.of(keyboardHeightScale);

    Resources resources = getResources();
    float originalMajorCategoryHeight =
        resources.getDimension(R.dimen.symbol_view_major_category_height);
    int majorCategoryHeight = Math.round(originalMajorCategoryHeight * keyboardHeightScale);
    this.numberKeyboardHeight = Optional.of(
        symbolInputViewHeight - majorCategoryHeight
        - resources.getDimensionPixelSize(R.dimen.button_frame_height));

    if (!isInflated()) {
      return;
    }

    setLayoutHeight(this, symbolInputViewHeight);
    setLayoutHeight(getMajorCategoryFrame(), majorCategoryHeight);
    setLayoutHeight(findViewById(R.id.number_keyboard), numberKeyboardHeight.get());
    setLayoutHeight(getNumberKeyboardFrame(), LayoutParams.WRAP_CONTENT);
  }

  public int getNumberKeyboardHeight() {
    return numberKeyboardHeight.get();
  }

  private void resetCandidateViewPager() {
    if (!isInflated()) {
      return;
    }

    ViewPager candidateViewPager = getCandidateViewPager();
    TabHost tabHost = getTabHost();
    Preconditions.checkState(symbolCandidateStorage.isPresent());

    SymbolTabWidgetViewPagerAdapter adapter = new SymbolTabWidgetViewPagerAdapter(
        getContext(),
        symbolCandidateStorage.get(), viewEventListener, symbolCandidateSelectListener,
        currentMajorCategory, skin, emojiProviderType, tabHost, candidateViewPager,
        candidateTextSize, desciptionTextSize);
    candidateViewPager.setAdapter(adapter);
    candidateViewPager.setOnPageChangeListener(adapter);
    tabHost.setOnTabChangedListener(adapter);
  }

  @SuppressWarnings("deprecation")
  private void updateMajorCategoryBackgroundSkin() {
    View view = getMajorCategoryFrame();
    if (view != null) {
      view.setBackgroundDrawable(
          skin.symbolMajorCategoryBackgroundDrawable.getConstantState().newDrawable());
    }
  }

  @SuppressWarnings("deprecation")
  private void updateMinorCategoryBackgroundSkin() {
    View view = getMinorCategoryFrame();
    if (view != null) {
      view.setBackgroundDrawable(
          skin.buttonFrameBackgroundDrawable.getConstantState().newDrawable());
    }
  }

  @SuppressWarnings("deprecation")
  private void updateNumberKeyboardSkin() {
    getNumberKeyboardView().setSkin(skin);
    findViewById(R.id.number_frame).setBackgroundDrawable(
        skin.windowBackgroundDrawable.getConstantState().newDrawable());
    findViewById(R.id.button_frame_in_symbol_view).setBackgroundDrawable(
        skin.buttonFrameBackgroundDrawable.getConstantState().newDrawable());
  }

  /**
   * Sets click event handlers to each major category button.
   * It is necessary that the inflation has been done before this method invocation.
   */
  @SuppressWarnings("deprecation")
  private void updateMajorCategoryButtonsSkin() {
    Resources resources = getResources();
    for (SymbolMajorCategory majorCategory : SymbolMajorCategory.values()) {
      MozcImageButton view = getMajorCategoryButton(majorCategory);
      Preconditions.checkState(
          view != null, "The view corresponding to " + majorCategory.name() + " is not found.");
      view.setOnClickListener(new MajorCategoryButtonClickListener(majorCategory));
      switch (majorCategory) {
        case NUMBER:
          view.setBackgroundDrawable(
              majorCategoryButtonDrawableFactory.createLeftButtonDrawable());
          break;
        case EMOJI:
          view.setBackgroundDrawable(
              majorCategoryButtonDrawableFactory.createRightButtonDrawable(emojiEnabled));
          break;
        default:
          view.setBackgroundDrawable(
              majorCategoryButtonDrawableFactory.createCenterButtonDrawable());
          break;
      }
      view.setImageDrawable(BackgroundDrawableFactory.createSelectableDrawable(
          skin.getDrawable(resources, majorCategory.buttonSelectedImageResourceId),
          Optional.of(skin.getDrawable(resources, majorCategory.buttonImageResourceId))));
      // Update the padding since setBackgroundDrawable() overwrites it.
      view.setMaxImageHeight(
          resources.getDimensionPixelSize(majorCategory.maxImageHeightResourceId));
    }
  }

  private void initializeNumberKeyboard() {
    final KeyboardFactory factory = new KeyboardFactory();

    getNumberKeyboardView().addOnLayoutChangeListener(new OnLayoutChangeListener() {
      @Override
      public void onLayoutChange(
          View view, int left, int top, int right, int bottom,
          int oldLeft, int oldTop, int oldRight, int oldBottom) {
        int width = right - left;
        int height = bottom - top;
        int oldWidth = oldRight - oldLeft;
        int oldHeight = oldBottom - oldTop;
        if (width == 0 || height == 0 || (width == oldWidth && height == oldHeight)) {
          return;
        }
        KeyboardView keyboardView = KeyboardView.class.cast(view);
        Keyboard keyboard =
            factory.get(getResources(), KeyboardSpecification.SYMBOL_NUMBER, width, height);
        keyboardView.setKeyboard(keyboard);
        keyboardView.invalidate();
      }
    });
  }

  private void initializeMinorCategoryTab() {
    TabHost tabhost = getTabHost();
    tabhost.setup();
    // Create NUM_TABS (= 6) tabs.
    // Note that we may want to change the number of tabs, however due to the limitation of
    // the current TabHost implementation, it is difficult. Fortunately, all major categories
    // have the same number of minor categories, so we use it as hard-coded value.
    for (int i = 0; i < NUM_TABS; ++i) {
      // The tab's id is the index of the tab.
      TabSpec tab = tabhost.newTabSpec(String.valueOf(i));
      MozcImageView view = new MozcImageView(getContext());
      view.setSoundEffectsEnabled(false);
      tab.setIndicator(view);
      // Set dummy view for the content. The actual content will be managed by ViewPager.
      tab.setContent(R.id.symbol_input_dummy);
      tabhost.addTab(tab);
    }

    // Hack: Set the current tab to the non-default (neither 0 nor 1) position,
    // so that the reset process will set the view's visibility appropriately.
    tabhost.setCurrentTab(2);
  }

  @SuppressWarnings("deprecation")
  private void updateTabBackgroundSkin() {
    if (!isInflated()) {
      return;
    }
    getTabHost().setBackgroundDrawable(
        skin.windowBackgroundDrawable.getConstantState().newDrawable());
    TabWidget tabWidget = getTabWidget();
    // Explicitly set non-transparent drawable to avoid strange background on
    // some devices.
    tabWidget.setBackgroundDrawable(
        skin.buttonFrameBackgroundDrawable.getConstantState().newDrawable());
    for (int i = 0; i < tabWidget.getTabCount(); ++i) {
      View view = tabWidget.getChildTabViewAt(i);
      view.setBackgroundDrawable(createTabBackgroundDrawable(skin));
    }
  }

  private static Drawable createTabBackgroundDrawable(Skin skin) {
    Preconditions.checkNotNull(skin);
    return new LayerDrawable(new Drawable[] {
        BackgroundDrawableFactory.createSelectableDrawable(
            new TabSelectedBackgroundDrawable(
                Math.round(skin.symbolMinorIndicatorHeightDimension),
                skin.symbolMinorCategoryTabSelectedColor),
            Optional.<Drawable>absent()),
        createMinorButtonBackgroundDrawable(skin)
    });
  }

  private void resetNumberKeyboard() {
    if (!isInflated()) {
      return;
    }
    // Resets candidate expansion.
    setLayoutHeight(getNumberKeyboardFrame(), LayoutParams.WRAP_CONTENT);
  }

  private void resetTabImageForMinorCategory() {
    if (!isInflated()) {
      return;
    }
    TabWidget tabWidget = getTabWidget();
    List<SymbolMinorCategory> minorCategoryList = currentMajorCategory.minorCategories;
    int definedTabSize = Math.min(minorCategoryList.size(), tabWidget.getChildCount());
    Resources resources = getResources();
    for (int i = 0; i < definedTabSize; ++i) {
      MozcImageView view = MozcImageView.class.cast(tabWidget.getChildTabViewAt(i));
      SymbolMinorCategory symbolMinorCategory = minorCategoryList.get(i);
      if (symbolMinorCategory.drawableResourceId != SymbolMinorCategory.INVALID_RESOURCE_ID
          && symbolMinorCategory.selectedDrawableResourceId
                 != SymbolMinorCategory.INVALID_RESOURCE_ID) {
        view.setImageDrawable(BackgroundDrawableFactory.createSelectableDrawable(
            skin.getDrawable(resources, symbolMinorCategory.selectedDrawableResourceId),
            Optional.of(skin.getDrawable(resources, symbolMinorCategory.drawableResourceId))));
      }
      if (symbolMinorCategory.maxImageHeightResourceId != SymbolMinorCategory.INVALID_RESOURCE_ID) {
        view.setMaxImageHeight(
            resources.getDimensionPixelSize(symbolMinorCategory.maxImageHeightResourceId));
      }
      if (symbolMinorCategory.contentDescriptionResourceId
          != SymbolMinorCategory.INVALID_RESOURCE_ID) {
        view.setContentDescription(
            resources.getString(symbolMinorCategory.contentDescriptionResourceId));
      }
    }
  }

  private static Drawable createMajorButtonBackgroundDrawable(Skin skin) {
    int padding = Math.round(skin.symbolMajorButtonPaddingDimension);
    int round = Math.round(skin.symbolMajorButtonRoundDimension);
    return BackgroundDrawableFactory.createPressableDrawable(
        new RoundRectKeyDrawable(
            padding, padding, padding, padding, round,
            skin.symbolPressedFunctionKeyTopColor,
            skin.symbolPressedFunctionKeyBottomColor,
            skin.symbolPressedFunctionKeyHighlightColor,
            skin.symbolPressedFunctionKeyShadowColor),
        Optional.<Drawable>of(new RoundRectKeyDrawable(
            padding, padding, padding, padding, round,
            skin.symbolReleasedFunctionKeyTopColor,
            skin.symbolReleasedFunctionKeyBottomColor,
            skin.symbolReleasedFunctionKeyHighlightColor,
            skin.symbolReleasedFunctionKeyShadowColor)));
  }

  private static Drawable createMinorButtonBackgroundDrawable(Skin skin) {
    return BackgroundDrawableFactory.createPressableDrawable(
        new ColorDrawable(skin.symbolMinorCategoryTabPressedColor),
        Optional.<Drawable>absent());
  }

  @SuppressWarnings("deprecation")
  private void initializeCloseButton() {
    ImageView closeButton = ImageView.class.cast(findViewById(R.id.symbol_view_close_button));
    if (closeButtonClickListener.isPresent()) {
      closeButton.setOnClickListener(closeButtonClickListener.get());
    }
  }

  /**
   * Sets a click event handler to the delete button.
   * It is necessary that the inflation has been done before this method invocation.
   */
  @SuppressWarnings("deprecation")
  private void initializeDeleteButton() {
    MozcImageView deleteButton =
        MozcImageView.class.cast(findViewById(R.id.symbol_view_delete_button));
    deleteButton.setOnTouchListener(deleteKeyEventButtonTouchListener);
  }

  /** c.f., {@code initializeDeleteButton}. */
  @SuppressWarnings("deprecation")
  private void initializeEnterButton() {
    ImageView enterButton = ImageView.class.cast(findViewById(R.id.symbol_view_enter_button));
    enterButton.setOnTouchListener(enterKeyEventButtonTouchListener);
  }

  private void initializeMicrophoneButton() {
    MozcImageView microphoneButton = getMicrophoneButton();
    if (microphoneButtonClickListener.isPresent()) {
      microphoneButton.setOnClickListener(microphoneButtonClickListener.get());
    }
    microphoneButton.setVisibility(isMicrophoneButtonEnabled ? VISIBLE : GONE);
  }

  @SuppressWarnings("deprecation")
  private void updateSeparatorsSkin() {
    Resources resources = getResources();
    int minorPaddingSize = (int) resources.getFraction(
        R.fraction.symbol_separator_padding_fraction,
        resources.getDimensionPixelSize(R.dimen.button_frame_height), 0);
    findViewById(R.id.symbol_view_backspace_separator).setBackgroundDrawable(
        new InsetDrawable(new ColorDrawable(skin.symbolSeparatorColor),
                          0, minorPaddingSize, 0, minorPaddingSize));
    int majorPaddingSize = (int) resources.getFraction(
        R.fraction.symbol_separator_padding_fraction,
        resources.getDimensionPixelSize(R.dimen.symbol_view_major_category_height), 0);
    InsetDrawable separator = new InsetDrawable(
        new ColorDrawable(skin.symbolSeparatorColor), 0, majorPaddingSize, 0, majorPaddingSize);
    for (int id : new int[] {R.id.symbol_view_close_button_separator,
                             R.id.symbol_view_enter_button_separator}) {
      findViewById(id).setBackgroundDrawable(separator.getConstantState().newDrawable());
    }

    for (int id : new int[] {R.id.symbol_separator_1,
                             R.id.symbol_separator_3}) {
      findViewById(id).setBackgroundDrawable(
          skin.keyboardFrameSeparatorBackgroundDrawable.getConstantState().newDrawable());
    }
    findViewById(R.id.symbol_separator_2).setBackgroundDrawable(
        skin.symbolSeparatorAboveMajorCategoryBackgroundDrawable
            .getConstantState().newDrawable());
  }

  @VisibleForTesting TabHost getTabHost() {
    return TabHost.class.cast(findViewById(android.R.id.tabhost));
  }

  private ViewPager getCandidateViewPager() {
    return ViewPager.class.cast(findViewById(R.id.symbol_input_candidate_view_pager));
  }

  @VisibleForTesting MozcImageButton getMajorCategoryButton(SymbolMajorCategory majorCategory) {
    Preconditions.checkNotNull(majorCategory);
    return MozcImageButton.class.cast(findViewById(majorCategory.buttonResourceId));
  }

  @VisibleForTesting View getEmojiDisabledMessageView() {
    return findViewById(R.id.symbol_emoji_disabled_message_view);
  }

  public void setEmojiEnabled(boolean unicodeEmojiEnabled, boolean carrierEmojiEnabled) {
    this.emojiEnabled = unicodeEmojiEnabled || carrierEmojiEnabled;
    enableEmoji(this.emojiEnabled);
    Preconditions.checkState(symbolCandidateStorage.isPresent());
    symbolCandidateStorage.get().setEmojiEnabled(unicodeEmojiEnabled, carrierEmojiEnabled);
  }

  public void setPasswordField(boolean isPasswordField) {
    this.isPasswordField = isPasswordField;
  }

  @SuppressWarnings("deprecation")
  private void enableEmoji(boolean enableEmoji) {
    if (!isInflated()) {
      return;
    }

    MozcImageButton imageButton = getMajorCategoryButton(SymbolMajorCategory.EMOJI);
    imageButton.setBackgroundDrawable(
        majorCategoryButtonDrawableFactory.createRightButtonDrawable(enableEmoji));
    // Update the padding since setBackgroundDrawable() overwrites it.
    imageButton.setMaxImageHeight(getResources().getDimensionPixelSize(
        SymbolMajorCategory.EMOJI.maxImageHeightResourceId));
  }

  void resetToMajorCategory(Optional<SymbolMajorCategory> category) {
    Preconditions.checkNotNull(category);
    setMajorCategory(category.or(currentMajorCategory));
    deleteKeyEventButtonTouchListener.reset();
    enterKeyEventButtonTouchListener.reset();
  }

  @VisibleForTesting void reset() {
    // the current minor category is also updated in setMajorCategory.
    resetToMajorCategory(Optional.of(SymbolMajorCategory.NUMBER));
  }

  @Override
  public void setVisibility(int visibility) {
    boolean isVisible = visibility == View.VISIBLE;
    boolean previousIsVisible = getVisibility() == View.VISIBLE;
    super.setVisibility(visibility);

    if (previousIsVisible == isVisible) {
      return;
    }

    if (!isVisible) {
      // Releases candidate resources. Also, on some devices, this cancels repeating invalidation
      // to support emoji related stuff.
      TabHost tabHost = getTabHost();
      if (tabHost != null) {
        tabHost.setOnTabChangedListener(null);
      }
      ViewPager candidateViewPager = getCandidateViewPager();
      if (candidateViewPager != null) {
        candidateViewPager.setAdapter(null);
        candidateViewPager.setOnPageChangeListener(null);
      }
    }

    if (viewEventListener.isPresent()) {
      if (isVisible) {
        viewEventListener.get().onShowSymbolInputView(Collections.<TouchEvent>emptyList());
      } else {
        viewEventListener.get().onCloseSymbolInputView();
      }
    }
  }

  void setSymbolCandidateStorage(SymbolCandidateStorage symbolCandidateStorage) {
    this.symbolCandidateStorage = Optional.of(symbolCandidateStorage);
  }

  void setKeyEventHandler(KeyEventHandler keyEventHandler) {
    this.keyEventHandler = Optional.of(keyEventHandler);
    deleteKeyEventButtonTouchListener.setKeyEventHandler(keyEventHandler);
    enterKeyEventButtonTouchListener.setKeyEventHandler(keyEventHandler);
  }

  void setCandidateTextDimension(float candidateTextSize, float descriptionTextSize) {
    Preconditions.checkArgument(candidateTextSize > 0);
    Preconditions.checkArgument(descriptionTextSize > 0);

    this.candidateTextSize = candidateTextSize;
    this.desciptionTextSize = descriptionTextSize;
  }

  void setPopupEnabled(boolean popupEnabled) {
    this.popupEnabled = popupEnabled;
    if (!isInflated()) {
      return;
    }
    getNumberKeyboardView().setPopupEnabled(popupEnabled);
  }

  /**
   * Initializes EmojiProvider selection dialog, if necessary.
   */
  @VisibleForTesting void maybeInitializeEmojiProviderDialog(Context context) {
    if (emojiProviderDialog.isPresent()) {
      return;
    }

    EmojiProviderDialogListener listener = new EmojiProviderDialogListener(context);
    try {
      AlertDialog dialog = new AlertDialog.Builder(context)
          .setTitle(R.string.pref_emoji_provider_type_title)
          .setItems(R.array.pref_emoji_provider_type_entries, listener)
          .create();
      this.emojiProviderDialog = Optional.of(dialog);
    } catch (InflateException e) {
      // Ignore the exception.
    }
  }

  /**
   * Sets the major category to show.
   *
   * The view is updated.
   * The active minor category is also updated.
   *
   * This method submit a preedit text except for a {@link SymbolMajorCategory#NUMBER} major
   * category since this class commit a candidate directly.
   *
   * @param newCategory the major category to show.
   */
  @VisibleForTesting void setMajorCategory(SymbolMajorCategory newCategory) {
    Preconditions.checkNotNull(newCategory);

    {
      SymbolCandidateView symbolCandidateView =
          SymbolCandidateView.class.cast(findViewById(R.id.symbol_input_candidate_view));
      if (symbolCandidateView != null) {
        symbolCandidateView.reset();
      }
    }

    if (newCategory != SymbolMajorCategory.NUMBER && viewEventListener.isPresent()) {
      viewEventListener.get().onSubmitPreedit();
    }

    if (newCategory == SymbolMajorCategory.NUMBER) {
      CandidateView candidateView =
          CandidateView.class.cast(findViewById(R.id.candidate_view_in_symbol_view));
      candidateView.clearAnimation();
      candidateView.setVisibility(View.GONE);
      candidateView.reset();
    }

    currentMajorCategory = newCategory;

    if (currentMajorCategory == SymbolMajorCategory.NUMBER) {
      findViewById(android.R.id.tabhost).setVisibility(View.GONE);
      findViewById(R.id.number_frame).setVisibility(View.VISIBLE);
      resetNumberKeyboard();
    } else {
      findViewById(android.R.id.tabhost).setVisibility(View.VISIBLE);
      findViewById(R.id.number_frame).setVisibility(View.GONE);
      updateMinorCategory();
    }

    // Hide overlapping separator
    if (currentMajorCategory == SymbolMajorCategory.NUMBER) {
      findViewById(R.id.symbol_view_close_button_separator).setVisibility(View.INVISIBLE);
    } else {
      findViewById(R.id.symbol_view_close_button_separator).setVisibility(View.VISIBLE);
    }

    if (currentMajorCategory == SymbolMajorCategory.EMOJI) {
      findViewById(R.id.symbol_view_enter_button_separator).setVisibility(View.INVISIBLE);
    } else {
      findViewById(R.id.symbol_view_enter_button_separator).setVisibility(View.VISIBLE);
    }

    // Update visibility relating attributes.
    for (SymbolMajorCategory majorCategory : SymbolMajorCategory.values()) {
      // Update major category selector button's look and feel.
      MozcImageButton button = getMajorCategoryButton(majorCategory);
      if (button != null) {
        button.setSelected(majorCategory == currentMajorCategory);
        button.setEnabled(majorCategory != currentMajorCategory);
      }
    }

    View emojiDisabledMessageView = getEmojiDisabledMessageView();
    if (emojiDisabledMessageView != null) {
      // Show messages about emoji-disabling, if necessary.
      emojiDisabledMessageView.setVisibility(
          currentMajorCategory == SymbolMajorCategory.EMOJI
          && !emojiEnabled ? View.VISIBLE : View.GONE);
    }
  }

  private void updateMinorCategory() {
    // Reset the minor category to the default value.
    resetTabImageForMinorCategory();
    resetCandidateViewPager();
    SymbolMinorCategory minorCategory = currentMajorCategory.getDefaultMinorCategory();
    Preconditions.checkState(symbolCandidateStorage.isPresent());
    if (symbolCandidateStorage.get().getCandidateList(minorCategory).getCandidatesCount() == 0) {
      minorCategory = currentMajorCategory.getMinorCategoryByRelativeIndex(minorCategory, 1);
    }
    int index = currentMajorCategory.minorCategories.indexOf(minorCategory);
    getCandidateViewPager().setCurrentItem(index);

    // Disable feedback before setting the current tab programatically.
    // Background: TabHost.setCurrentTab calls back onTabChanged, in which feedback event is fired.
    // However, we don't have ways to distinguish if onTabChanged is called through user click
    // event or by the call of setCurrentTab.  If we don't disable feedback here, the click sound
    // effect is fired twice; one is from the onClick event on major category tab and the other is
    // by the call of setCurrentTab here.  See b/17119766.
    SymbolTabWidgetViewPagerAdapter adapter =
        SymbolTabWidgetViewPagerAdapter.class.cast(getCandidateViewPager().getAdapter());
    adapter.setFeedbackEnabled(false);
    getTabHost().setCurrentTab(index);
    adapter.setFeedbackEnabled(true);
  }

  void setEmojiProviderType(EmojiProviderType emojiProviderType) {
    Preconditions.checkNotNull(emojiProviderType);

    Preconditions.checkState(symbolCandidateStorage.isPresent());
    this.emojiProviderType = emojiProviderType;
    this.symbolCandidateStorage.get().setEmojiProviderType(emojiProviderType);
    if (!isInflated()) {
      return;
    }

    resetCandidateViewPager();
  }

  void setEventListener(
      ViewEventListener viewEventListener, OnClickListener closeButtonClickListener,
      OnClickListener microphoneButtonClickListener) {
    this.viewEventListener = Optional.of(viewEventListener);
    this.closeButtonClickListener = Optional.of(closeButtonClickListener);
    this.microphoneButtonClickListener = Optional.of(microphoneButtonClickListener);
  }

  void setMicrophoneButtonEnabled(boolean enabled) {
    isMicrophoneButtonEnabled = enabled;
    if (isInflated()) {
      getMicrophoneButton().setVisibility(enabled ? VISIBLE : GONE);
    }
  }

  void setSkin(Skin skin) {
    Preconditions.checkNotNull(skin);
    if (this.skin.equals(skin)) {
      return;
    }
    this.skin = skin;
    majorCategoryButtonDrawableFactory.setSkin(skin);
    if (!isInflated()) {
      return;
    }
    updateSkinAwareDrawable();
  }

  @SuppressWarnings("deprecation")
  private void updateSkinAwareDrawable() {
    updateTabBackgroundSkin();
    resetTabImageForMinorCategory();

    SymbolTabWidgetViewPagerAdapter adapter =
        SymbolTabWidgetViewPagerAdapter.class.cast(getCandidateViewPager().getAdapter());
    if (adapter != null) {
      adapter.setSkin(skin);
    }
    updateMajorCategoryBackgroundSkin();
    updateMajorCategoryButtonsSkin();
    updateMinorCategoryBackgroundSkin();
    updateNumberKeyboardSkin();
    updateSeparatorsSkin();
    getMicrophoneButton().setSkin(skin);

    TabWidget tabWidget = TabWidget.class.cast(findViewById(android.R.id.tabs));
    for (int i = 0; i < tabWidget.getChildCount(); ++i) {
      MozcImageView.class.cast(tabWidget.getChildTabViewAt(i)).setSkin(skin);
    }

    // Note delete button shouldn't be applied createMajorButtonBackgroundDrawable as background
    // as it should show different background (same as minor categories).
    for (int id : new int[] {R.id.symbol_view_close_button,
                             R.id.symbol_view_enter_button}) {
      MozcImageView view = MozcImageView.class.cast(findViewById(id));
      view.setSkin(skin);
      view.setBackgroundDrawable(createMajorButtonBackgroundDrawable(skin));
    }
    MozcImageView deleteKeyView =
        MozcImageView.class.cast(findViewById(R.id.symbol_view_delete_button));
    deleteKeyView.setSkin(skin);
    deleteKeyView.setBackgroundDrawable(createMinorButtonBackgroundDrawable(skin));
  }

  private KeyboardView getNumberKeyboardView() {
    return KeyboardView.class.cast(findViewById(R.id.number_keyboard));
  }

  private FrameLayout getNumberKeyboardFrame() {
    return FrameLayout.class.cast(findViewById(R.id.number_keyboard_frame));
  }

  private LinearLayout getMajorCategoryFrame() {
    return LinearLayout.class.cast(findViewById(R.id.symbol_major_category));
  }

  private LinearLayout getMinorCategoryFrame() {
    return LinearLayout.class.cast(findViewById(R.id.symbol_minor_category));
  }

  private TabWidget getTabWidget() {
    return TabWidget.class.cast(findViewById(android.R.id.tabs));
  }

  private MozcImageView getMicrophoneButton() {
    return MozcImageView.class.cast(findViewById(R.id.microphone_button));
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh) {
    super.onSizeChanged(w, h, oldw, oldh);
    // The boundary of Drawable instance which has been set as background
    // is not updated automatically.
    // Update the boundary below.
    if (isInflated()) {
      updateSkinAwareDrawable();
    }
  }

  @Override
  public void trimMemory() {
    ViewGroup viewGroup = getCandidateViewPager();
    if (viewGroup == null) {
      return;
    }
    for (int i = 0; i < viewGroup.getChildCount(); ++i) {
      View view = viewGroup.getChildAt(i);
      if (view instanceof MemoryManageable) {
        MemoryManageable.class.cast(view).trimMemory();
      }
    }
  }
}
