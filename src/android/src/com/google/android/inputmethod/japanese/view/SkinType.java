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

package org.mozc.android.inputmethod.japanese.view;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.SkinParser.SkinParserException;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.Resources;

/**
 * Type of skins.
 */
public enum SkinType {

  ORANGE_LIGHTGRAY(R.xml.skin_orange_lightgray),
  BLUE_LIGHTGRAY(R.xml.skin_blue_lightgray),
  BLUE_DARKGRAY(R.xml.skin_blue_darkgray),
  MATERIAL_DESIGN_LIGHT(R.xml.skin_material_design_light),
  MATERIAL_DESIGN_DARK(R.xml.skin_material_design_dark),
  // This is an instance for testing of skin support in some classes.
  // TODO(matsuzakit): No more required. Remove.
  TEST(R.xml.skin_orange_lightgray)
  ;

  private Optional<Skin> skin = Optional.absent();
  private final int resourceId;

  private SkinType(int resourceId) {
    this.resourceId = resourceId;
  }

  public Skin getSkin(Resources resources) {
    Preconditions.checkNotNull(resources);
    if (skin.isPresent()) {
      return skin.get();
    }
    SkinParser parser = new SkinParser(resources, resources.getXml(resourceId));
    try {
      skin = Optional.of(parser.parseSkin());
    } catch (SkinParserException e) {
      MozcLog.e(e.getLocalizedMessage());
      skin = Optional.of(new Skin());  // Fall-back skin.
    }
    return skin.get();
  }
}
