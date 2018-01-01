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

package org.mozc.android.inputmethod.japanese.hardwarekeyboard;

import org.mozc.android.inputmethod.japanese.hardwarekeyboard.KeyEventMapperFactory.KeyEventMapper;
import com.google.common.base.Preconditions;

import android.view.KeyCharacterMap;
import android.view.KeyEvent;

/**
 * Compact and mutable KeyEvent for internal use.
 */
class CompactKeyEvent {

  private int keyCode;
  private int metaState;
  private int unicodeCharacter;
  private int combiningAccent;
  private int scanCode;

  public CompactKeyEvent(KeyEvent keyEvent) {
    Preconditions.checkNotNull(keyEvent);
    keyCode = keyEvent.getKeyCode();
    metaState = keyEvent.getMetaState();
    int flagedCodepoint = keyEvent.getUnicodeChar();
    // TODO(team): Come up with a better definition of the "character" when
    // KeyCharacterMap.COMBINING_ACCENT bit is set.
    unicodeCharacter = flagedCodepoint & KeyCharacterMap.COMBINING_ACCENT_MASK;
    combiningAccent = flagedCodepoint & KeyCharacterMap.COMBINING_ACCENT_MASK;
    scanCode = keyEvent.getScanCode();
  }

  /**
   * Construct an instance and apply overlay mapping.
   */
  public CompactKeyEvent(KeyEvent keyEvent, KeyEventMapper overlayMapper) {
    this(keyEvent);
    Preconditions.checkNotNull(overlayMapper).applyMapping(this);
  }

  public int getKeyCode() {
    return keyCode;
  }

  void setKeyCode(int keyCode) {
    this.keyCode = keyCode;
  }

  public int getMetaState() {
    return metaState;
  }

  void setMetaState(int metaState) {
    this.metaState = metaState;
  }

  public int getCombiningAccent() {
    return combiningAccent;
  }

  public int getDeadChar(int character) {
    return KeyCharacterMap.getDeadChar(combiningAccent, character);
  }

  public int getUnicodeCharacter() {
    return unicodeCharacter;
  }

  void setUnicodeCharacter(int unicodeCharacter) {
    this.unicodeCharacter = unicodeCharacter;
  }

  public int getScanCode() {
    return scanCode;
  }

  void setScanCode(int scanCode) {
    this.scanCode = scanCode;
  }
}
