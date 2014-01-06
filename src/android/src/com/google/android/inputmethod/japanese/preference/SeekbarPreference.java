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

package org.mozc.android.inputmethod.japanese.preference;

import org.mozc.android.inputmethod.japanese.resources.R;

import android.content.Context;
import android.content.res.TypedArray;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewParent;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;

/**
 * Preference to configure the flick sensitivity.
 *
 * This preference has a seekbar and its indicators to manipulate flick sensitivity,
 * in addition to other regular preferences.
 *
 */
public class SeekbarPreference extends Preference {
  private class SeekBarChangeListener implements OnSeekBarChangeListener {
    private final TextView sensitivityTextView;
    SeekBarChangeListener(TextView textView) {
      this.sensitivityTextView = textView;
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
      if (sensitivityTextView != null) {
        sensitivityTextView.setText(String.valueOf(progress + offset));
      }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
      setValue(seekBar.getProgress() + offset);
    }
  }

  private int max;
  private int offset;
  private int value;
  private String unit;
  private String lowText;
  private String middleText;
  private String highText;

  public SeekbarPreference(Context context) {
    super(context);
  }

  public SeekbarPreference(Context context, AttributeSet attrs) {
    super(context, attrs);
    init(context, attrs);
  }

  public SeekbarPreference(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
    init(context, attrs);
  }

  private void init(Context context, AttributeSet attrs) {
    int[] attributeArray = {
        android.R.attr.max,
        android.R.attr.progress,
        R.attr.seekbar_offset,
        R.attr.seekbar_unit,
        R.attr.seekbar_low_text,
        R.attr.seekbar_middle_text,
        R.attr.seekbar_high_text,
    };
    TypedArray typedArray = context.obtainStyledAttributes(attrs, attributeArray);
    try {
      offset = typedArray.getInt(2, 0);
      max = typedArray.getInt(0, 0);
      value = typedArray.getInt(1, 0) + offset;
      unit = typedArray.getString(3);
      lowText = typedArray.getString(4);
      middleText = typedArray.getString(5);
      highText = typedArray.getString(6);
    } finally {
      typedArray.recycle();
    }
  }

  @Override
  protected View onCreateView(ViewGroup parent) {
    LayoutInflater inflater = LayoutInflater.from(getContext());
    View view = inflater.inflate(R.layout.pref_seekbar, parent, false);
    ViewGroup preferenceFrame = ViewGroup.class.cast(view.findViewById(R.id.preference_frame));
    // Note: Invoking getLayoutResource() is a hack to obtain the default resource id.
    inflater.inflate(getLayoutResource(), preferenceFrame);
    initializeOriginalView(view);
    return view;
  }

  /**
   * Initializes the original preferecen's parameters.
   */
  private void initializeOriginalView(View view) {
    View summaryView = view.findViewById(android.R.id.summary);
    if (summaryView == null) {
      // We can do nothing.
      return;
    }
    shrinkBottomMarginAndPadding(summaryView);

    ViewParent parent = summaryView.getParent();
    if (!(parent instanceof View)) {
      return;
    }
    View parentView = View.class.cast(parent);
    shrinkBottomMarginAndPadding(parentView);

    ViewParent rootViewParent = parentView.getParent();
    if (!(rootViewParent instanceof View)) {
      return;
    }

    View rootView = View.class.cast(rootViewParent);
    rootView.setMinimumHeight(0);

    View widgetFrame = view.findViewById(android.R.id.widget_frame);
    if (widgetFrame != null) {
      widgetFrame.setVisibility(View.GONE);
    }
  }

  private void shrinkBottomMarginAndPadding(View view) {
    LayoutParams params = view.getLayoutParams();
    if (params instanceof MarginLayoutParams) {
      MarginLayoutParams.class.cast(params).bottomMargin = 0;
      view.setLayoutParams(params);
    }
    view.setPadding(view.getPaddingLeft(), view.getPaddingTop(), view.getPaddingRight(), 0);
  }

  @Override
  protected Object onGetDefaultValue(TypedArray a, int index) {
    return a.getInt(index, 0);
  }

  @Override
  protected void onSetInitialValue(boolean restoreValue, Object defaultValue) {
    setValue(restoreValue ? getPersistedInt(value) : Integer.class.cast(defaultValue));
  }

  @Override
  protected void onBindView(View view) {
    super.onBindView(view);

    TextView valueView =
        TextView.class.cast(view.findViewById(R.id.pref_seekbar_value));
    SeekBar seekBar = SeekBar.class.cast(view.findViewById(R.id.pref_seekbar_seekbar));
    if (seekBar != null) {
      seekBar.setMax(max - offset);
      seekBar.setProgress(value - offset);
      seekBar.setOnSeekBarChangeListener(new SeekBarChangeListener(valueView));
    }
    if (valueView != null) {
      valueView.setText(String.valueOf(value));
    }
    TextView unitView = TextView.class.cast(view.findViewById(R.id.pref_seekbar_unit));
    if (unitView != null) {
      if (unit == null) {
        unitView.setVisibility(View.GONE);
      } else {
        unitView.setVisibility(View.VISIBLE);
        unitView.setText(unit);
      }
    }

    setTextView(lowText, view, R.id.pref_seekbar_low_text);
    setTextView(middleText, view, R.id.pref_seekbar_middle_text);
    setTextView(highText, view, R.id.pref_seekbar_high_text);
  }

  private static void setTextView(String text, View parentView, int resourceId) {
    if (text == null) {
      return;
    }

    TextView textView = TextView.class.cast(parentView.findViewById(resourceId));
    if (textView == null) {
      return;
    }

    textView.setText(text);
  }

  void setValue(int value) {
    this.value = value;
    persistInt(value);
    notifyDependencyChange(shouldDisableDependents());
    notifyChanged();
  }
}
