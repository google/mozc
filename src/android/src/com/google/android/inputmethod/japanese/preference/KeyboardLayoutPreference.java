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

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumMap;
import java.util.List;

/**
 * Preference class for KeyboardLayout.
 *
 */
public class KeyboardLayoutPreference extends Preference {

  static class Item {
    final KeyboardLayout keyboardLayout;
    final int keyboardResourceId;
    final int titleResId;
    final int descriptionResId;
    Item(KeyboardLayout keyboardLayout, int keyboardResourceId,
         int titleResId, int descriptionResId) {
      this.keyboardLayout = keyboardLayout;
      this.keyboardResourceId = keyboardResourceId;
      this.titleResId = titleResId;
      this.descriptionResId = descriptionResId;
    }
  }

  class ImageAdapter extends BaseAdapter {
    private final EnumMap<KeyboardLayout, KeyboardPreviewDrawable> drawableMap =
        new EnumMap<KeyboardLayout, KeyboardPreviewDrawable>(KeyboardLayout.class);

    ImageAdapter(Resources resources) {
      for (Item item : itemList) {
        drawableMap.put(
            item.keyboardLayout,
            new KeyboardPreviewDrawable(resources, item.keyboardLayout, item.keyboardResourceId));
      }
    }

    @Override
    public int getCount() {
      return itemList.size();
    }

    @Override
    public Object getItem(int item) {
      return itemList.get(item);
    }

    @Override
    public long getItemId(int item) {
      return item;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parentView) {
      if (convertView == null) {
        // No cached view is available. Inflate it.
        LayoutInflater inflater = LayoutInflater.from(getContext());
        convertView = inflater.inflate(R.layout.pref_keyboard_layout_item, parentView, false);
      }

      boolean isEnabled = KeyboardLayoutPreference.this.isEnabled();

      // Needs to set the properties to the view even if a cached view is available,
      // because the cached view might use to be used for another item.
      Item item = itemList.get(position);
      ImageView imageView =
          ImageView.class.cast(convertView.findViewById(R.id.pref_inputstyle_item_image));
      imageView.setImageDrawable(drawableMap.get(item.keyboardLayout));
      imageView.setEnabled(isEnabled);

      TextView titleView =
          TextView.class.cast(convertView.findViewById(R.id.pref_inputstyle_item_title));
      titleView.setText(item.titleResId);
      titleView.setTextColor(getContext().getResources().getColor(isEnabled
          ? R.color.pref_inputstyle_title
          : R.color.pref_inputstyle_title_disabled));
      titleView.setEnabled(parentView.isEnabled());

      updateBackground(convertView, position, getActiveIndex());
      return convertView;
    }

    void setSkinType(SkinType skinType) {
      for (KeyboardPreviewDrawable drawable : drawableMap.values()) {
        drawable.setSkinType(skinType);
      }
    }
  }

  private class GalleryEventListener implements OnItemSelectedListener, OnItemClickListener {
    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
      // Update the value if necessary.
      KeyboardLayout newValue = itemList.get(position).keyboardLayout;
      if (callChangeListener(newValue)) {
        setValue(newValue);
        updateAllItemBackground(parent, getActiveIndex());
      }
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
      // Update the description.
      TextView descriptionView = TextView.class.cast(
          View.class.cast(parent.getParent()).findViewById(R.id.pref_inputstyle_description));
      if (descriptionView != null) {
        Item item = Item.class.cast(parent.getItemAtPosition(position));
        descriptionView.setText(Html.fromHtml(
            descriptionView.getContext().getResources().getString(item.descriptionResId)));
      }
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
      // Do nothing.
    }
  }

  static final List<Item> itemList = Collections.unmodifiableList(Arrays.asList(
      new Item(
          KeyboardLayout.TWELVE_KEYS,
          R.xml.kbd_12keys_flick_kana,
          R.string.pref_keyboard_layout_title_12keys,
          R.string.pref_keyboard_layout_description_12keys),
      new Item(
          KeyboardLayout.QWERTY,
          R.xml.kbd_qwerty_kana,
          R.string.pref_keyboard_layout_title_qwerty,
          R.string.pref_keyboard_layout_description_qwerty),
      new Item(
          KeyboardLayout.GODAN,
          R.xml.kbd_godan_kana,
          R.string.pref_keyboard_layout_title_godan,
          R.string.pref_keyboard_layout_description_godan)));

  private final ImageAdapter imageAdapter = new ImageAdapter(getContext().getResources());
  private final GalleryEventListener galleryEventListener = new GalleryEventListener();
  private final OnSharedPreferenceChangeListener sharedPreferenceChangeListener =
      new OnSharedPreferenceChangeListener() {
        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
          if (key.equals(PreferenceUtil.PREF_SKIN_TYPE)) {
            updateSkinType();
          }
        }
      };

  private KeyboardLayout value = KeyboardLayout.TWELVE_KEYS;

  public KeyboardLayoutPreference(Context context) {
    super(context);
  }

  public KeyboardLayoutPreference(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public KeyboardLayoutPreference(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
  }

  public void setValue(KeyboardLayout value) {
    if (value == null) {
      throw new NullPointerException("value must not be null.");
    }

    if (this.value != value) {
      this.value = value;
      persistString(value.name());
      notifyChanged();
    }
  }

  public KeyboardLayout getValue() {
    return value;
  }

  private int getActiveIndex() {
    for (int i = 0; i < itemList.size(); ++i) {
      if (itemList.get(i).keyboardLayout == value) {
        return i;
      }
    }

    MozcLog.e("Current value is not found in the itemList: " + value);
    return 0;
  }

  @Override
  protected Object onGetDefaultValue(TypedArray a, int index) {
    return toKeyboardLayoutInternal(a.getString(index));
  }

  @Override
  protected void onSetInitialValue(boolean restoreValue, Object defaultValue) {
    setValue(restoreValue
        ? toKeyboardLayoutInternal(getPersistedString(null))
        : KeyboardLayout.class.cast(defaultValue));
  }

  /**
   * Parses the name and returns the {@link KeyboardLayout} instance.
   * If invalid name or {@code null} is given, the default value {@code TWELVE_KEYS} will
   * be returned.
   */
  private KeyboardLayout toKeyboardLayoutInternal(String keyboardLayoutName) {
    if (keyboardLayoutName != null) {
      try {
        return KeyboardLayout.valueOf(keyboardLayoutName);
      } catch (IllegalArgumentException e) {
        MozcLog.e("Invalid keyboard layout name: " + keyboardLayoutName, e);
      }
    }

    // Fallback. Use TWELVE_KEYS by default.
    return KeyboardLayout.TWELVE_KEYS;
  }

  @Override
  protected void onBindView(View view) {
    super.onBindView(view);

    TextView descriptionView =
        TextView.class.cast(view.findViewById(R.id.pref_inputstyle_description));
    descriptionView.setMovementMethod(LinkMovementMethod.getInstance());

    @SuppressWarnings("deprecation")
    android.widget.Gallery gallery =
        android.widget.Gallery.class.cast(view.findViewById(R.id.pref_inputstyle_gallery));
    gallery.setAdapter(imageAdapter);
    gallery.setOnItemSelectedListener(galleryEventListener);
    gallery.setOnItemClickListener(galleryEventListener);
    gallery.setSelection(getActiveIndex());

    updateSkinType();
  }

  private static void updateAllItemBackground(AdapterView<?> gallery, int activeIndex) {
    for (int i = 0; i < gallery.getChildCount(); ++i) {
      View child = gallery.getChildAt(i);
      int position = gallery.getPositionForView(child);
      updateBackground(child, position, activeIndex);
    }
  }

  private static void updateBackground(View view, int position, int activePosition) {
    if (position == activePosition) {
      view.setBackgroundResource(android.R.drawable.dialog_frame);
    } else {
      view.setBackgroundDrawable(null);
    }
  }

  @Override
  protected void onAttachedToHierarchy(PreferenceManager preferenceManager) {
    super.onAttachedToHierarchy(preferenceManager);
    updateSkinType();
    getSharedPreferences().registerOnSharedPreferenceChangeListener(
        sharedPreferenceChangeListener);
  }

  @Override
  protected void onPrepareForRemoval() {
    getSharedPreferences().unregisterOnSharedPreferenceChangeListener(
        sharedPreferenceChangeListener);
    super.onPrepareForRemoval();
  }

  void updateSkinType() {
    SkinType skinType = PreferenceUtil.getEnum(
        getSharedPreferences(),
        PreferenceUtil.PREF_SKIN_TYPE, SkinType.class, SkinType.ORANGE_LIGHTGRAY);
    imageAdapter.setSkinType(skinType);
  }
}
