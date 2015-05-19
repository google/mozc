// Copyright 2010-2013, Google Inc.
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

import org.mozc.android.inputmethod.japanese.preference.KeyboardLayoutPreference.Item;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.Html;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

/**
 */
public class KeyboardLayoutPreferenceTest extends InstrumentationTestCase {

  /**
   * Common initialization method.
   */
  private View getWidgetView() {
    Context context = getInstrumentation().getTargetContext();
    KeyboardLayoutPreference preference = new KeyboardLayoutPreference(context);
    LayoutInflater inflate =
        LayoutInflater.class.cast(context.getSystemService(Context.LAYOUT_INFLATER_SERVICE));
    View widgetView = inflate.inflate(R.layout.pref_keyboard_layout, null);
    preference.onBindView(widgetView);
    return widgetView;
  }

  /**
   * Checks if only active (persisted) setting's view has background drawable.
   */
  private void checkBackgroundResource(
      KeyboardLayoutPreference.Item item,
      @SuppressWarnings("deprecation") android.widget.Gallery gallery) {
    for (int i = 0; i < gallery.getChildCount(); ++i) {
      View child = gallery.getChildAt(i);
      Item itemAtPosition =
          Item.class.cast(gallery.getItemAtPosition(gallery.getPositionForView(child)));
      if (itemAtPosition.keyboardLayout == item.keyboardLayout) {
        assertNull(child.getBackground());
      } else {
        assertNotNull(child.getBackground());
      }
    }
  }

  @SmallTest
  public void testInitialState() {
    Context context = getInstrumentation().getTargetContext();
    for (final KeyboardLayoutPreference.Item item : KeyboardLayoutPreference.itemList) {
      KeyboardLayoutPreference preference = new KeyboardLayoutPreference(context);
      // In order to avoid dirtying the persistent data.
      preference.setPersistent(false);
      preference.setValue(item.keyboardLayout);

      LayoutInflater inflate =
          LayoutInflater.class.cast(context.getSystemService(Context.LAYOUT_INFLATER_SERVICE));
      View widgetView = inflate.inflate(R.layout.pref_keyboard_layout, null);
      preference.onBindView(widgetView);

      @SuppressWarnings("deprecation")
      android.widget.Gallery gallery =
          android.widget.Gallery.class.cast(widgetView.findViewById(R.id.pref_inputstyle_gallery));
      // Check if appropriate item is selected on the gallery.
      assertEquals(item, gallery.getSelectedItem());
      // Check background resources.
      checkBackgroundResource(item, gallery);
    }
  }

  @SmallTest
  public void testOnItemSelected() {
    Context context = getInstrumentation().getTargetContext();
    View widgetView = getWidgetView();
    @SuppressWarnings("deprecation")
    android.widget.Gallery gallery =
        android.widget.Gallery.class.cast(widgetView.findViewById(R.id.pref_inputstyle_gallery));
    for (int i = 0; i < KeyboardLayoutPreference.itemList.size(); ++i) {
      KeyboardLayoutPreference.Item item = KeyboardLayoutPreference.itemList.get(i);
      gallery.setSelection(i, false);
      TextView descriptionView =
          (TextView) widgetView.findViewById(R.id.pref_inputstyle_description);
      String expectedDescription =
          Html.fromHtml(context.getResources().getString(item.descriptionResId)).toString();
      assertEquals(expectedDescription, descriptionView.getText().toString());
    }
  }

  @SmallTest
  public void testOnItemClick() {
    Context context = getInstrumentation().getTargetContext();
    KeyboardLayoutPreference preference = new KeyboardLayoutPreference(context);
    preference.setPersistent(false);

    LayoutInflater inflate =
        LayoutInflater.class.cast(context.getSystemService(Context.LAYOUT_INFLATER_SERVICE));
    View widgetView = inflate.inflate(R.layout.pref_keyboard_layout, null);
    preference.onBindView(widgetView);
    @SuppressWarnings("deprecation")
    android.widget.Gallery gallery =
        android.widget.Gallery.class.cast(widgetView.findViewById(R.id.pref_inputstyle_gallery));
    for (int i = 0; i < KeyboardLayoutPreference.itemList.size(); ++i) {
      KeyboardLayoutPreference.Item item = KeyboardLayoutPreference.itemList.get(i);
      gallery.getOnItemClickListener().onItemClick(gallery, null, i, i);
      checkBackgroundResource(item, gallery);
      assertEquals(item.keyboardLayout, preference.getValue());
    }
  }

  @SmallTest
  public void testImageAdapter_getView() {
    View widgetView = getWidgetView();
    @SuppressWarnings("deprecation")
    android.widget.Gallery gallery =
        android.widget.Gallery.class.cast(widgetView.findViewById(R.id.pref_inputstyle_gallery));
    Context context = getInstrumentation().getTargetContext();
    for (int i = 0; i < KeyboardLayoutPreference.itemList.size(); ++i) {
      KeyboardLayoutPreference.Item item = KeyboardLayoutPreference.itemList.get(i);
      View view = gallery.getAdapter().getView(i, null, gallery);
      // Checking ImageView's Drawable is omitted because there is no convenient way.
      TextView titleView = TextView.class.cast(view.findViewById(R.id.pref_inputstyle_item_title));
      assertEquals(context.getResources().getText(item.titleResId),
                   titleView.getText().toString());
    }
  }
}
