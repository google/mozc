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

package org.mozc.android.inputmethod.japanese.emoji;

import org.mozc.android.inputmethod.japanese.MozcLog;
import com.google.common.base.Preconditions;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.Build;
import android.text.Layout.Alignment;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.text.TextUtils;

import java.util.Arrays;

/**
 * Checker of whether the given string is renderable as a single glyph on the current device or not.
 * <p/>
 * This class emulates {@link android.graphics.Paint#hasGlyph(String)} by comparing the measured
 * width and rendered bitmap of the given string with those of Unicode Noncharacter code point
 * U+FFFE (which will be drawn as 'Tofu' by a fallback glyph).
 * <p/>
 * Note: this class uses an internal buffer, so is not thread-safe.
 */
// TODO(hsumita): Fix coding style.
// TODO(hsumita): Use {@link android.graphics.Paint#hasGlyph(String)} on API 23 and above.
public class EmojiRenderableChecker {

    private static final int TEST_FONT_SIZE = 10;
    private static final int FOREGROUND_COLOR = Color.WHITE;
    private static final int BACKGROUND_COLOR = Color.BLACK;
    private static final String EM_STRING = "m";
    private static final String FALLBACK_CHARACTER_STRING = "\uFFFE";

    private final Bitmap mBitmap;
    private final Canvas mCanvas;
    private final TextPaint mPaint;
    private final int[] mTargetGlyphPixels;

    /** Measured width of {@code EM_STRING} (width of 'm' glyph). */
    private final float mEmGlyphWidth;
    /** Measured width of {@code FALLBACK_CHARACTER_STRING} (width of 'Tofu' glyph). */
    private final float mFallbackGlyphWidth;
    /** Bitmap pixels of {@code FALLBACK_CHARACTER_STRING} (pixels of 'Tofu' glyph). */
    private final int[] mFallbackGlyphPixels;

    public EmojiRenderableChecker() {
        mPaint = new TextPaint();
        mPaint.setTextSize(TEST_FONT_SIZE);
        mPaint.setColor(FOREGROUND_COLOR);

        mBitmap = Bitmap.createBitmap(TEST_FONT_SIZE, TEST_FONT_SIZE, Config.ARGB_8888);
        mCanvas = new Canvas(mBitmap);
        mTargetGlyphPixels = new int[TEST_FONT_SIZE * TEST_FONT_SIZE];

        mEmGlyphWidth = mPaint.measureText(EM_STRING);
        mFallbackGlyphWidth = mPaint.measureText(FALLBACK_CHARACTER_STRING);

        mFallbackGlyphPixels = new int[TEST_FONT_SIZE * TEST_FONT_SIZE];
        renderGlyphInternal(FALLBACK_CHARACTER_STRING, mFallbackGlyphPixels);
    }

    /**
     * Checks whether the Android system provides a single glyph for the string.
     * <p/>
     * The string can be a single code point or code points that make a ligature.
     * See {@link android.graphics.Paint#hasGlyph(String)} for details.
     * <p/>
     * Note: If SDK version is &lt; 23, this method use heuristics, and there may be wrong results.
     * Especially, this method does not work on multiple proportional glyphs.
     */
    public boolean isRenderable(String string) {
        Preconditions.checkArgument(!TextUtils.isEmpty(string));


        // First, use measureText to determine whether or not {@code string} has a single glyph.
        float width = mPaint.measureText(string);
        if (width == 0.0f) {
            // If width is zero, it is not renderable.
            return false;
        }

        if (string.codePointCount(0, string.length()) > 1) {
            // Heuristic to detect fallback glyphs for ligatures like flags and ZWJ sequences (1).
            // Drop if string is rendererd too widely (> 2 'm').
            if (width > 2 * mEmGlyphWidth) {
                return false;
            }

            float sumWidth = 0;
            for (int i = 0; i < string.length(); ) {
                int charCount = Character.charCount(string.codePointAt(i));
                sumWidth += mPaint.measureText(string, i, i + charCount);
                i += charCount;
            }
            // Heuristic to detect fallback glyphs for ligatures like flags and ZWJ sequences (2).
            // If width is greater than or equal to the sum of width of each code point, it is very
            // likely that the system is using fallback fonts to draw {@code string} in two or more
            // glyphs instead of a single ligature glyph. (hasGlyph returns false in this case.)
            // False detections are possible (the ligature glyph may happen to have the same width
            // as the sum width), but there are no good way to avoid them.
            // NOTE: This heuristic does not work with proportional glyphs.
            // NOTE: This heuristic does not work when a ZWJ sequence is partially combined.
            // E.g. If system has a glyph for "A ZWJ B" and not for "A ZWJ B ZWJ C", this heuristic
            // returns true for "A ZWJ B ZWJ C".
            if (width >= sumWidth) {
                return false;
            }
        }

        if (width != mFallbackGlyphWidth) {
            // If width is not the same as the width of the fallback glyph (Tofu),
            // {@code string} should have its own glyph.
            return true;
        }

        try {
            // Fallback to bitmap rendering if we cannot determine the result by measureText.
            renderGlyphInternal(string, mTargetGlyphPixels);
            return !Arrays.equals(mTargetGlyphPixels, mFallbackGlyphPixels);
        } catch (NullPointerException e) {
            MozcLog.e("Unknown exception happens: " + e);
        }
        return true;
    }

    private void renderGlyphInternal(String string, int[] buffer) {
        mCanvas.drawColor(BACKGROUND_COLOR);

        // We use StaticLayout instead of Canvas#drawText because some devices support Emoji
        // code points, which is the main target of this check, only on TextView and its related
        // widget, but not on Canvas class, for some reason (maybe for animation support).
        // So, even if we can see the Emoji code points in TextView on the device, Canvas#drawText
        // will render a fallback glyph.
        // <p/>
        // Note that, on some devices, StaticLayout may modify the passed TextPaint instance.
        // We have to pass a new TextPaint instance instead of mPaint, in order to avoid affecting
        // following mPaint.measureText() calls.
        StaticLayout layout = new StaticLayout(string, new TextPaint(mPaint), TEST_FONT_SIZE,
                Alignment.ALIGN_NORMAL, 1, 0, false);
        layout.draw(mCanvas);

        mBitmap.getPixels(buffer, 0, TEST_FONT_SIZE, 0, 0, TEST_FONT_SIZE, TEST_FONT_SIZE);
    }
}
