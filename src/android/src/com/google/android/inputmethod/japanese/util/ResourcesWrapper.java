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

package org.mozc.android.inputmethod.japanese.util;

import android.annotation.TargetApi;
import android.content.res.AssetFileDescriptor;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.graphics.Movie;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.io.InputStream;

/**
 * Simple proxy of the {@link Resources} class to inject methods.
 * The concept of this class is similar to ContextWrapper in android framework.
 *
 */
public class ResourcesWrapper extends Resources {
  private final Resources base;

  public ResourcesWrapper(Resources base) {
    super(base.getAssets(), null, null);
    this.base = base;
  }

  public Resources getBase() {
    return base;
  }

  @Override
  public boolean equals(Object o) {
    return base.equals(o);
  }

  @Override
  public XmlResourceParser getAnimation(int id) throws NotFoundException {
    return base.getAnimation(id);
  }

  @Override
  public boolean getBoolean(int id) throws NotFoundException {
    return base.getBoolean(id);
  }

  @Override
  public int getColor(int id) throws NotFoundException {
    return base.getColor(id);
  }

  @Override
  public ColorStateList getColorStateList(int id) throws NotFoundException {
    return base.getColorStateList(id);
  }

  @Override
  public Configuration getConfiguration() {
    return base.getConfiguration();
  }

  @Override
  public float getDimension(int id) throws NotFoundException {
    return base.getDimension(id);
  }

  @Override
  public int getDimensionPixelOffset(int id) throws NotFoundException {
    return base.getDimensionPixelOffset(id);
  }

  @Override
  public int getDimensionPixelSize(int id) throws NotFoundException {
    return base.getDimensionPixelSize(id);
  }

  @Override
  public DisplayMetrics getDisplayMetrics() {
    return base.getDisplayMetrics();
  }

  @Override
  public Drawable getDrawable(int id) throws NotFoundException {
    return base.getDrawable(id);
  }

  @TargetApi(15)
  @Override
  public Drawable getDrawableForDensity(int id, int density) throws NotFoundException {
    return base.getDrawableForDensity(id, density);
  }

  @Override
  public float getFraction(int id, int base, int pbase) {
    return this.base.getFraction(id, base, pbase);
  }

  @Override
  public int getIdentifier(String name, String defType, String defPackage) {
    return base.getIdentifier(name, defType, defPackage);
  }

  @Override
  public int[] getIntArray(int id) throws NotFoundException {
    return base.getIntArray(id);
  }

  @Override
  public int getInteger(int id) throws NotFoundException {
    return base.getInteger(id);
  }

  @Override
  public XmlResourceParser getLayout(int id) throws NotFoundException {
    return base.getLayout(id);
  }

  @Override
  public Movie getMovie(int id) throws NotFoundException {
    return base.getMovie(id);
  }

  @Override
  public String getQuantityString(int id, int quantity, Object... formatArgs)
      throws NotFoundException {
    return base.getQuantityString(id, quantity, formatArgs);
  }

  @Override
  public String getQuantityString(int id, int quantity) throws NotFoundException {
    return base.getQuantityString(id, quantity);
  }

  @Override
  public CharSequence getQuantityText(int id, int quantity) throws NotFoundException {
    return base.getQuantityText(id, quantity);
  }

  @Override
  public String getResourceEntryName(int resid) throws NotFoundException {
    return base.getResourceEntryName(resid);
  }

  @Override
  public String getResourceName(int resid) throws NotFoundException {
    return base.getResourceName(resid);
  }

  @Override
  public String getResourcePackageName(int resid) throws NotFoundException {
    return base.getResourcePackageName(resid);
  }

  @Override
  public String getResourceTypeName(int resid) throws NotFoundException {
    return base.getResourceTypeName(resid);
  }

  @Override
  public String getString(int id, Object... formatArgs) throws NotFoundException {
    return base.getString(id, formatArgs);
  }

  @Override
  public String getString(int id) throws NotFoundException {
    return base.getString(id);
  }

  @Override
  public String[] getStringArray(int id) throws NotFoundException {
    return base.getStringArray(id);
  }

  @Override
  public CharSequence getText(int id, CharSequence def) {
    return base.getText(id, def);
  }

  @Override
  public CharSequence getText(int id) throws NotFoundException {
    return base.getText(id);
  }

  @Override
  public CharSequence[] getTextArray(int id) throws NotFoundException {
    return base.getTextArray(id);
  }

  @Override
  public void getValue(int id, TypedValue outValue, boolean resolveRefs)
      throws NotFoundException {
    base.getValue(id, outValue, resolveRefs);
  }

  @Override
  public void getValue(String name, TypedValue outValue, boolean resolveRefs)
      throws NotFoundException {
    base.getValue(name, outValue, resolveRefs);
  }

  @TargetApi(15)
  @Override
  public void getValueForDensity(int id, int density, TypedValue outValue, boolean resolveRefs)
      throws NotFoundException {
    base.getValueForDensity(id, density, outValue, resolveRefs);
  }

  @Override
  public XmlResourceParser getXml(int id) throws NotFoundException {
    return base.getXml(id);
  }

  @Override
  public int hashCode() {
    return base.hashCode();
  }

  @Override
  public TypedArray obtainAttributes(AttributeSet set, int[] attrs) {
    return base.obtainAttributes(set, attrs);
  }

  @Override
  public TypedArray obtainTypedArray(int id) throws NotFoundException {
    return base.obtainTypedArray(id);
  }

  @Override
  public InputStream openRawResource(int id, TypedValue value) throws NotFoundException {
    return base.openRawResource(id, value);
  }

  @Override
  public InputStream openRawResource(int id) throws NotFoundException {
    return base.openRawResource(id);
  }

  @Override
  public AssetFileDescriptor openRawResourceFd(int id) throws NotFoundException {
    return base.openRawResourceFd(id);
  }

  @Override
  public void parseBundleExtra(String tagName, AttributeSet attrs, Bundle outBundle)
      throws XmlPullParserException {
    base.parseBundleExtra(tagName, attrs, outBundle);
  }

  @Override
  public void parseBundleExtras(XmlResourceParser parser, Bundle outBundle)
      throws XmlPullParserException, IOException {
    base.parseBundleExtras(parser, outBundle);
  }

  @Override
  public String toString() {
    return base.toString();
  }

  @Override
  public void updateConfiguration(Configuration config, DisplayMetrics metrics) {
    // Note: This method is invoked also by the super class's constructor.
    if (base != null) {
      base.updateConfiguration(config, metrics);
    }
  }
}
