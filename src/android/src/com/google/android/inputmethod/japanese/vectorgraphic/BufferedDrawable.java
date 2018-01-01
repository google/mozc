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

package org.mozc.android.inputmethod.japanese.vectorgraphic;

import com.google.common.base.Objects;
import com.google.common.base.Preconditions;
import com.google.common.collect.Lists;
import com.google.common.collect.Maps;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Matrix;
import android.graphics.Outline;
import android.graphics.PorterDuff.Mode;
import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.SparseIntArray;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * A Drawable buffering a decorated Drawable.
 * <p>
 * Buffering here means that the decorated Drawable is rendered to on-memory bitmap
 * and draw method call uses the bitmap.
 * <p>
 * There are two motivations for this class.
 * <ol>
 * <li>To support H/W accelerated canvas. H/W canvas has limited API so some Drawable (e.g.,
 * PictureDrawable) cannot be drawn onto it. Through buffering such Drawables become available.
 * <li>Performance. Complex Drawable (e.g., PictureDrawable) takes time to be drawn.
 * </ol>
 * However dumb buffering takes much memory.
 * Therefore this class holds only the decomposed bitmap blocks which has non-transparent pixels.
 * <p>
 * CAUTION: Skew and rotation is not supported. The {@code Matrix} of given {@code Canvas} should
 * have only transform and scale.
 */
public class BufferedDrawable extends Drawable {

  /**
   * The length of decomposed bitmap in pixels.
   * The dimension will be: (COMPOSITION_LENGTH x COMPOSITION_LENGTH)
   * <p>
   * Longer length causes faster performance and bigger memory footprint.
   * <p>
   * For performance, the value should be 2^n.
   */
  private static final int COMPOSITION_LENGTH = 16;

  /**
   * Metadata of decomposed bitmap collection.
   */
  private static class DecomposedBitmapMetadata {

    /**
     * The width of bounds in screen pixels (non-scaled).
     */
    private final int width;

    /**
     * The height of bounds in screen pixels (non-scaled).
     */
    private final int height;

    /**
     * The scales (X and Y) of the canvas.
     * The decomposed bitmaps are rendered with the scales in order to avoid scaling noise.
     */
    private final float scaleX;
    private final float scaleY;

    DecomposedBitmapMetadata(int width, int height, float scaleX, float scaleY) {
      this.width = width;
      this.height = height;
      this.scaleX = scaleX;
      this.scaleY = scaleY;
    }

    @Override
    public boolean equals(Object o) {
      if (o == null) {
        return false;
      }
      if (o == this) {
        return true;
      }
      if (!(o instanceof DecomposedBitmapMetadata)) {
        return false;
      }
      DecomposedBitmapMetadata rhs = DecomposedBitmapMetadata.class.cast(o);
      return width == rhs.width && height == rhs.height
          && scaleX == rhs.scaleX && scaleY == rhs.scaleY;
    }

    @Override
    public int hashCode() {
      return Objects.hashCode(width, height, scaleX, scaleY);
    }
  }

  /**
   * A piece of decomposed bitmap.
   * Rendered image on-memory is decomposed into {@code DecomposedBitmap} instances.
   * <p>
   * Monolithic bitmap consumes much memory: (4 * width * height) bytes; however typical
   * bitmap mainly consists of transparent pixels. Storing such pixels in memory is basically
   * useless because they affect nothing.
   * By decomposing bitmap into small part and filtering-out transparent-pixel-only-part from
   * memory, we can reduce memory footprint.
   */
  private static class DecomposedBitmap {
    private final int left;
    private final int top;
    private final Bitmap bitmap;
    /**
     * @param left left position in the original bitmap in pixels
     * @param top top position in the original bitmap in pixels
     * @param bitmap decomposed bitmap
     */
    DecomposedBitmap(int left, int top, Bitmap bitmap) {
      this.left = left;
      this.top = top;
      this.bitmap = bitmap;
    }
  }

  public BufferedDrawable(Drawable baseDrawable) {
    this.baseDrawable = Preconditions.checkNotNull(baseDrawable);
  }

  /**
   * Temporary {@code Canvas} for on-memory rendering.
   * <p>
   * Using {@code ThreadLocal} for thread safety. The temporary canvas is used only in
   * {@code Drawable#draw(Canvas)} so by using {@code ThreadLocal} synchronization can be omitted.
   * <p>
   * Basically having a Canvas as member field is good way but {@code Canvas} is too big to do so.
   */
  private static final ThreadLocal<Canvas> TEMPORARY_CANVAS = new ThreadLocal<Canvas>() {
    @Override
    protected Canvas initialValue() { return new Canvas(); }
  };

  private static final ThreadLocal<Matrix> TEMPORARY_MATRIX = new ThreadLocal<Matrix>() {
    @Override
    protected Matrix initialValue() { return new Matrix(); }
  };

  /**
   * Base Drawable to be drawn onto on-memory canvas.
   */
  private final Drawable baseDrawable;

  /**
   * Backing decomposed {@code Bitmap} for on-memory rendering.
   * <p>
   * This is a kind of cache. Therefore if depending conditions (e.g., scale) are changed
   * this should be invalidated.
   */
  private final Map<DecomposedBitmapMetadata, Collection<DecomposedBitmap>>
      metadataToBitmapCollection = Maps.newHashMap();

  public Drawable getBaseDrawable() {
    return baseDrawable;
  }

  /**
   * Clears the buffer and its internal resources.
   * <p>
   * The buffer is never freed automatically so you have to call this method if you want to clear
   * the buffer.
   * For example when you call setter method which affects rendering result,
   * you have to call this.
   */
  public void clearBuffer() {
    for (Collection<DecomposedBitmap> bitmaps : metadataToBitmapCollection.values()) {
      for (DecomposedBitmap bitmap : bitmaps) {
        bitmap.bitmap.recycle();
      }
    }
    metadataToBitmapCollection.clear();
  }

  private DecomposedBitmapMetadata createDecomposedBitmapMetadata(int width, int height,
                                                                  float[] matrixValues) {
    Preconditions.checkArgument(width >= 0);
    Preconditions.checkArgument(height >= 0);
    Preconditions.checkArgument(matrixValues.length >= 9);
    Preconditions.checkArgument(matrixValues[Matrix.MSCALE_X] > 0
        && matrixValues[Matrix.MSKEW_X] == 0
        && matrixValues[Matrix.MSKEW_Y] == 0
        && matrixValues[Matrix.MSCALE_Y] > 0
        && matrixValues[Matrix.MPERSP_0] == 0
        && matrixValues[Matrix.MPERSP_1] == 0
        && matrixValues[Matrix.MPERSP_2] == 1f,
        "Only simple matrix (transformation and scale) is supported.");
    return new DecomposedBitmapMetadata(
        width, height, matrixValues[Matrix.MSCALE_X], matrixValues[Matrix.MSCALE_Y]);
  }

  @SuppressWarnings("deprecation")
  @Override
  public void draw(Canvas canvas) {
    Rect bounds = getBounds();
    int width = bounds.width();
    int height = bounds.height();
    if (width <= 0 || height <= 0) {
      return;
    }
    // Get matrix values through temporary matrix.
    Matrix matrix = TEMPORARY_MATRIX.get();
    canvas.getMatrix(matrix);
    float[] matrixValues = new float[9];
    matrix.getValues(matrixValues);
    // Get metadata based on bounds (== width and height) and matrixValues.
    DecomposedBitmapMetadata metadata = createDecomposedBitmapMetadata(width, height, matrixValues);
    // Get rendering result as DecomposedBitmap collection and draw it onto the canvas.
    Collection<DecomposedBitmap> decomposedBitmaps =
        maybeCreateDecomposedBitmap(metadata, matrixValues);
    drawDecomposedBitmap(canvas, matrixValues, decomposedBitmaps);
  }

  /**
   * Gets rendering result as DecomposedBitmap collection.
   * <p>
   * If there is a DecomposedBitmap collection corresponding to given metadata, reuses it.
   * Otherwise new one is created and returned.
   */
  private Collection<DecomposedBitmap> maybeCreateDecomposedBitmap(
      DecomposedBitmapMetadata metadata, float[] matrixValues) {
    Collection<DecomposedBitmap> result = metadataToBitmapCollection.get(metadata);
    if (result != null) {
      return result;  // We have cached data.
    }
    result = createDecomposedBitmap(metadata, matrixValues);
    metadataToBitmapCollection.put(metadata, result);
    return result;
  }

  /**
   * Creates a DecomposedBitmap collection.
   */
  private Collection<DecomposedBitmap> createDecomposedBitmap(DecomposedBitmapMetadata metadata,
                                                              float[] matrixValues) {
    // Create bitmap with original (on screen) width and size with alignment.
    Bitmap bitmap = Bitmap.createBitmap(
        (int) Math.ceil(metadata.width * metadata.scaleX / COMPOSITION_LENGTH)
            * COMPOSITION_LENGTH,
        (int) Math.ceil(metadata.height * metadata.scaleY / COMPOSITION_LENGTH)
            * COMPOSITION_LENGTH,
        Bitmap.Config.ARGB_8888);
    // Draw the base Drawable on the bitmap through on-memory canvas.
    Canvas onMemoryCanvas = TEMPORARY_CANVAS.get();
    // Apply transformation and scale based on the canvas's matrix.
    // NOTE: Skew and/or rotation are not supported.
    onMemoryCanvas.setMatrix(null);  // As the canvas is on-memory, setMatrix works well.
    onMemoryCanvas.translate(Math.min(0, matrixValues[Matrix.MTRANS_X]),
                             Math.min(0, matrixValues[Matrix.MTRANS_Y]));
    onMemoryCanvas.scale(matrixValues[Matrix.MSCALE_X], matrixValues[Matrix.MSCALE_Y]);
    // Render baseDrawable.
    onMemoryCanvas.setBitmap(bitmap);
    baseDrawable.draw(onMemoryCanvas);
    // Release the bitmap from on-memory canvas for smaller memory footprint.
    onMemoryCanvas.setBitmap(null);
    // Decompose the bitmap into small parts.
    // Passing bitmap might be recycled in the method if possible.
    return decomposeBitmap(bitmap);
  }

  /**
   * Draws a DecomposedBitmap collection onto given canvas.
   */
  private void drawDecomposedBitmap(Canvas canvas, float[] matrixValues,
                                    Collection<DecomposedBitmap> decomposedBitmaps) {
    int saveCount = canvas.save();
    try {
      // What are done here:
      // 1. Make the canvas's matrix identity.
      // 2. Translate the canvas based on the (original) canvas's matrix, considering offset.
      // To say,
      //   canvas.setMatrix(null);  // for 1.
      //   canvas.translate(Math.max(0, matrixValues[Matrix.MTRANS_X]),
      //                    Math.max(0, matrixValues[Matrix.MTRANS_Y]));  // for 2.
      // However Canvas#setMatrix() doesn't work well for h/w canvas so do above manually.
      //   canvas.scale(1f / matrixValues[Matrix.MSCALE_X], 1f / matrixValues[Matrix.MSCALE_Y]);
      //   canvas.translate(-matrixValues[Matrix.MTRANS_X], -matrixValues[Matrix.MTRANS_Y]);
      // The translation above can be merged with the translation for 2.
      canvas.scale(1f / matrixValues[Matrix.MSCALE_X], 1f / matrixValues[Matrix.MSCALE_Y]);
      canvas.translate(Math.max(0, -matrixValues[Matrix.MTRANS_X]),
                       Math.max(0, -matrixValues[Matrix.MTRANS_Y]));
      Rect bounds = getBounds();
      int boundsLeft = bounds.left;
      int boundsTop = bounds.top;
      for (DecomposedBitmap decomposedBitmap : decomposedBitmaps) {
        canvas.drawBitmap(decomposedBitmap.bitmap, decomposedBitmap.left + boundsLeft,
                          decomposedBitmap.top + boundsTop, null);
      }
    } finally {
      canvas.restoreToCount(saveCount);
    }
  }

  /**
   * Check if all the pixels in given region are filled with 0 (==Transparent).
   * <p>
   * Don't consider performance improvement:
   * Current implementation is fast enough even on Nexus S according to profiling.
   */
  private boolean isPixelsAllTransparent(int[] pixels, int left, int top, int width) {
    for (int y = top; y < top + COMPOSITION_LENGTH; ++y) {
      int lineOffset = y * width;
      for (int i = left + lineOffset; i < left + lineOffset + COMPOSITION_LENGTH; ++i) {
        if (pixels[i] != Color.TRANSPARENT) {
          return false;
        }
      }
    }
    return true;
  }

  /**
   * Decomposes a {@code bitmap} into a {@code DecomposedBitmap} collection.
   * <p>
   * For rendering performance (reducing drawBitmap operation),
   * horizontally and vertically connected {@code DecomposedBitmap} are merged internally.
   * @param bitmap {@code Bitmap} to be decomposed. Note that this is recycled internally so
   *     the caller side cannot use this after the invocation.
   */
  private Collection<DecomposedBitmap> decomposeBitmap(Bitmap bitmap) {
    int width = bitmap.getWidth();
    int height = bitmap.getHeight();
    Preconditions.checkArgument(width % COMPOSITION_LENGTH == 0);
    Preconditions.checkArgument(height % COMPOSITION_LENGTH == 0);

    // Concatenate blocks to reduce the number of bitblt. Here we employed very simple algorithm to
    // reduce the calculation cost.
    // 1. Divide original bitmap into square blocks.
    // 2. Concatenate non-transparent blocks horizontally.
    // 3. Concatenate horizontally-merged blocks vertically if horizontal position / width of
    //    adjacent blocks are completely same.

    // Fill the int array with the entire bitmap data.
    int[] pixels = new int[width * height];
    bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
    boolean noTransparent = true;  // Will be turned off if transparent block is found.
    // Scan each block (size: COMPOSITION_LENGTH x COMPOSITION_LENGTH).
    List<SparseIntArray> nonTransparentBlocksList =
        Lists.newArrayListWithCapacity((int) Math.ceil((double) height / COMPOSITION_LENGTH));
    int roughBlockNum = 0;
    // Step 1 and 2. Divide original bitmap into blocks and concatenates it horizontally.
    for (int y = 0; y < height; y += COMPOSITION_LENGTH) {
      // Key: Start index of non-transparent block
      // Value: End index of non-transparent block (Exclusive)
      // Blocks in [Key, Value) is non-transparent.
      SparseIntArray nonTransparentBlocks = new SparseIntArray();
      int nonTransparentStartLeftIndex = Integer.MIN_VALUE;
      int xIndex = 0;
      for (int x = 0; x < width; x += COMPOSITION_LENGTH, ++xIndex) {
        // Check if all the pixels in [(x, y) - (x + COMPOSITION_LENGTH, y + COMPOSITION_LENGTH))
        // are transparent.
        if (isPixelsAllTransparent(pixels, x, y, width)) {
          // This block is Transparent so turn off noTransparent.
          noTransparent = false;
          // If nonTransparentStartLeft has already been set,
          // register the following block for decomposing.
          // [(nonTransparentStartLeftIndex * COMPOSITION_LENGTH, y) - (x, y + COMPOSITION_LENGTH)]
          if (nonTransparentStartLeftIndex != Integer.MIN_VALUE) {
            nonTransparentBlocks.append(nonTransparentStartLeftIndex, xIndex);
          }
          nonTransparentStartLeftIndex = Integer.MIN_VALUE;
        } else if (nonTransparentStartLeftIndex == Integer.MIN_VALUE) {
          // Current block is non-transparent and nonTransparentStartLeft has not been set yet.
          // Next DecomposedBitmap starts from this block.
          nonTransparentStartLeftIndex = xIndex;
        }
      }
      // Reached the end of the row. Register pending blocks if exists.
      if (nonTransparentStartLeftIndex != Integer.MIN_VALUE) {
        nonTransparentBlocks.put(nonTransparentStartLeftIndex, xIndex);
      }
      nonTransparentBlocksList.add(nonTransparentBlocks);
      roughBlockNum += nonTransparentBlocks.size();
    }

    // Optimization: If entire bitmap needs to be drawn,
    // return given bitmap instead of decomposed ones for rendering performance.
    if (noTransparent) {
      return Collections.singleton(new DecomposedBitmap(0, 0, bitmap));
    }

    // The capacity will not be increased.
    ArrayList<DecomposedBitmap> result = Lists.newArrayListWithCapacity(roughBlockNum);
    // Step 3. Concatenate horizontally-merged blocks vertically.
    for (int topIndex = 0; topIndex < nonTransparentBlocksList.size(); ++topIndex) {
      SparseIntArray nonTarnsparentBlocks = nonTransparentBlocksList.get(topIndex);
      for (int i = 0; i < nonTarnsparentBlocks.size(); ++i) {
        int leftIndex = nonTarnsparentBlocks.keyAt(i);
        int rightIndex = nonTarnsparentBlocks.valueAt(i);
        int bottomIndex = topIndex;
        if (rightIndex == Integer.MIN_VALUE) {
          // Skip a invalidated block, which was already handled.
          continue;
        }
        while (++bottomIndex < nonTransparentBlocksList.size()) {
          SparseIntArray blocks = nonTransparentBlocksList.get(bottomIndex);
          if (blocks.get(leftIndex) == rightIndex) {
            // Invalidate a merged block. Don't delete for performance.
            blocks.put(leftIndex, Integer.MIN_VALUE);
          } else {
            break;
          }
        }
        int left = leftIndex * COMPOSITION_LENGTH;
        int right = Math.min(rightIndex * COMPOSITION_LENGTH, width);
        int top = topIndex * COMPOSITION_LENGTH;
        int bottom = Math.min(bottomIndex * COMPOSITION_LENGTH, height);
        result.add(new DecomposedBitmap(left, top,
            Bitmap.createBitmap(bitmap, left, top, right - left, bottom - top)));
      }
    }
    bitmap.recycle();
    result.trimToSize();
    return result;
  }

  @Override
  public ConstantState getConstantState() {
    return new ConstantState() {
      @Override
      public int getChangingConfigurations() {
        return 0;
      }

      @Override
      public Drawable newDrawable() {
        ConstantState baseConstantState = baseDrawable.getConstantState();
        return baseConstantState == null
            ? new BufferedDrawable(baseDrawable)
            : new BufferedDrawable(baseConstantState.newDrawable());
      }
    };
  }

  @Override
  public void setBounds(int left, int top, int right, int bottom) {
    // As getBounds() is final method, we cannot delegate getBounds() to baseDrawable.
    // Instead call super.setBounds() to make the result of getBounds() correct.
    super.setBounds(left, top, right, bottom);
    baseDrawable.setBounds(left, top, right, bottom);
  }

  @Override
  public void setBounds(Rect bounds) {
    // c.f., setBound() above.
    super.setBounds(bounds);
    baseDrawable.setBounds(bounds);
  }

  @Override
  public boolean equals(Object o) {
    throw new UnsupportedOperationException();
  }

  @Override
  public Rect getDirtyBounds() {
    return baseDrawable.getDirtyBounds();
  }

  @Override
  public void setChangingConfigurations(int configs) {
    baseDrawable.setChangingConfigurations(configs);
  }

  @Override
  public int getChangingConfigurations() {
    return baseDrawable.getChangingConfigurations();
  }

  @Override
  public void setDither(boolean dither) {
    baseDrawable.setDither(dither);
  }

  @Override
  public void setFilterBitmap(boolean filter) {
    baseDrawable.setFilterBitmap(filter);
  }

  @Override
  public int hashCode() {
    throw new UnsupportedOperationException();
  }

  @Override
  public Callback getCallback() {
    return baseDrawable.getCallback();
  }

  @Override
  public void invalidateSelf() {
    baseDrawable.invalidateSelf();
  }

  @Override
  public void scheduleSelf(Runnable what, long when) {
    baseDrawable.scheduleSelf(what, when);
  }

  @Override
  public String toString() {
    throw new UnsupportedOperationException();
  }

  @Override
  public void unscheduleSelf(Runnable what) {
    baseDrawable.unscheduleSelf(what);
  }

  @Override
  public void setAlpha(int alpha) {
    baseDrawable.setAlpha(alpha);
  }

  @Override
  public int getAlpha() {
    return baseDrawable.getAlpha();
  }

  @Override
  public void setColorFilter(ColorFilter cf) {
    baseDrawable.setColorFilter(cf);
  }

  @Override
  public void setColorFilter(int color, Mode mode) {
    baseDrawable.setColorFilter(color, mode);
  }

  @Override
  public void setTint(int tint) {
    baseDrawable.setTint(tint);
  }

  @Override
  public void setTintList(ColorStateList tint) {
    baseDrawable.setTintList(tint);
  }

  @Override
  public void setTintMode(Mode tintMode) {
    baseDrawable.setTintMode(tintMode);
  }

  @Override
  public ColorFilter getColorFilter() {
    return baseDrawable.getColorFilter();
  }

  @Override
  public void clearColorFilter() {
    baseDrawable.clearColorFilter();
  }

  @Override
  public void setHotspot(float x, float y) {
    baseDrawable.setHotspot(x, y);
  }

  @Override
  public void setHotspotBounds(int left, int top, int right, int bottom) {
    baseDrawable.setHotspotBounds(left, top, right, bottom);
  }

  @Override
  public boolean isStateful() {
    return baseDrawable.isStateful();
  }

  @Override
  public boolean setState(int[] stateSet) {
    return baseDrawable.setState(stateSet);
  }

  @Override
  public int[] getState() {
    return baseDrawable.getState();
  }

  @Override
  public void jumpToCurrentState() {
    baseDrawable.jumpToCurrentState();
  }

  @Override
  public Drawable getCurrent() {
    return baseDrawable.getCurrent();
  }

  @Override
  public boolean setVisible(boolean visible, boolean restart) {
    return baseDrawable.setVisible(visible, restart);
  }

  @Override
  public void setAutoMirrored(boolean mirrored) {
    baseDrawable.setAutoMirrored(mirrored);
  }

  @Override
  public boolean isAutoMirrored() {
    return baseDrawable.isAutoMirrored();
  }

  @Override
  public void applyTheme(Theme t) {
    baseDrawable.applyTheme(t);
  }

  @Override
  public boolean canApplyTheme() {
    return baseDrawable.canApplyTheme();
  }

  @Override
  public int getOpacity() {
    return baseDrawable.getOpacity();
  }

  @Override
  public Region getTransparentRegion() {
    return baseDrawable.getTransparentRegion();
  }

  @Override
  public int getIntrinsicWidth() {
    return baseDrawable.getIntrinsicWidth();
  }

  @Override
  public int getIntrinsicHeight() {
    return baseDrawable.getIntrinsicHeight();
  }

  @Override
  public int getMinimumWidth() {
    return baseDrawable.getMinimumWidth();
  }

  @Override
  public int getMinimumHeight() {
    return baseDrawable.getMinimumHeight();
  }

  @Override
  public boolean getPadding(Rect padding) {
    return baseDrawable.getPadding(padding);
  }

  @Override
  public void getOutline(Outline outline) {
    baseDrawable.getOutline(outline);
  }

  @Override
  public Drawable mutate() {
    // Mutation is not supported.
    return this;
  }

  @Override
  public void inflate(Resources r, XmlPullParser parser, AttributeSet attrs)
      throws XmlPullParserException, IOException {
    throw new UnsupportedOperationException();
  }

  @Override
  public void inflate(Resources r, XmlPullParser parser, AttributeSet attrs, Theme theme)
      throws XmlPullParserException, IOException {
    throw new UnsupportedOperationException();
  }
}
