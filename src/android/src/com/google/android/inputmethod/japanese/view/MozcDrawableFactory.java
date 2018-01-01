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
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.vectorgraphic.BufferedDrawable;
import com.google.common.base.Charsets;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.AssetManager;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Paint.Align;
import android.graphics.Paint.Cap;
import android.graphics.Paint.Join;
import android.graphics.Paint.Style;
import android.graphics.Path;
import android.graphics.Picture;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Locale;

/**
 * Factory to create Drawables from raw resources which is in mozc original format.
 *
 * Implementation note: We decided to use vector-rendering to support various devices
 * which are different display resolutions.
 * For that purpose, we needed to have some vector format. {@code PictureDrawable}'s
 * serialization/deserialization seemed what we needed, but it turned out that its binary format
 * seems not compatible among various devices, unfortunately.
 * So, we decided to use our original format, and this class parses it.
 * Also, for performance purpose, this class caches the parsed drawable.
 *
 */
class MozcDrawableFactory {

  private static class MozcStyle {
    Paint paint = new Paint();
    int dominantBaseline = COMMAND_PICTURE_PAINT_DOMINANTE_BASELINE_AUTO;
  }

  /** Locale field for {@link Paint#setTextLocale(Locale)}. */
  private static final Optional<Locale> TEXT_LOCALE = (Build.VERSION.SDK_INT >= 17)
      ? Optional.of(Locale.JAPAN) : Optional.<Locale>absent();

  private static final int DRAWABLE_PICTURE = 1;
  private static final int DRAWABLE_STATE_LIST = 2;

  private static final int COMMAND_PICTURE_EOP = 0;
  private static final int COMMAND_PICTURE_DRAW_PATH = 1;
  private static final int COMMAND_PICTURE_DRAW_POLYLINE = 2;
  private static final int COMMAND_PICTURE_DRAW_POLYGON = 3;
  private static final int COMMAND_PICTURE_DRAW_LINE = 4;
  private static final int COMMAND_PICTURE_DRAW_RECT = 5;
  private static final int COMMAND_PICTURE_DRAW_CIRCLE = 6;
  private static final int COMMAND_PICTURE_DRAW_ELLIPSE = 7;
  private static final int COMMAND_PICTURE_DRAW_GROUP_START = 8;
  private static final int COMMAND_PICTURE_DRAW_GROUP_END = 9;
  private static final int COMMAND_PICTURE_DRAW_TEXT = 10;

  private static final int COMMAND_PICTURE_PATH_EOP = 0;
  private static final int COMMAND_PICTURE_PATH_MOVE = 1;
  private static final int COMMAND_PICTURE_PATH_LINE = 2;
  private static final int COMMAND_PICTURE_PATH_HORIZONTAL_LINE = 3;
  private static final int COMMAND_PICTURE_PATH_VERTICAL_LINE = 4;
  private static final int COMMAND_PICTURE_PATH_CURVE = 5;
  private static final int COMMAND_PICTURE_PATH_CONTINUED_CURVE = 6;
  private static final int COMMAND_PICTURE_PATH_CLOSE = 7;

  private static final int COMMAND_PICTURE_PAINT_EOP = 0;
  private static final int COMMAND_PICTURE_PAINT_STYLE = 1;
  private static final int COMMAND_PICTURE_PAINT_COLOR = 2;
  private static final int COMMAND_PICTURE_PAINT_SHADOW = 3;
  private static final int COMMAND_PICTURE_PAINT_STROKE_WIDTH = 4;
  private static final int COMMAND_PICTURE_PAINT_STROKE_CAP = 5;
  private static final int COMMAND_PICTURE_PAINT_STROKE_JOIN = 6;
  private static final int COMMAND_PICTURE_PAINT_SHADER = 7;
  private static final int COMMAND_PICTURE_PAINT_FONT_SIZE = 8;
  private static final int COMMAND_PICTURE_PAINT_TEXT_ANCHOR = 9;
  private static final int COMMAND_PICTURE_PAINT_DOMINANT_BASELINE = 10;
  private static final int COMMAND_PICTURE_PAINT_FONT_WEIGHT = 11;

  private static final int COMMAND_PICTURE_PAINT_TEXT_ANCHOR_START = 0;
  private static final int COMMAND_PICTURE_PAINT_TEXT_ANCHOR_MIDDLE = 1;
  private static final int COMMAND_PICTURE_PAINT_TEXT_ANCHOR_END = 2;

  private static final int COMMAND_PICTURE_PAINT_DOMINANTE_BASELINE_AUTO = 0;
  @SuppressWarnings("unused")
  private static final int COMMAND_PICTURE_PAINT_DOMINANTE_BASELINE_CENTRAL = 1;

  @SuppressWarnings("unused")
  private static final int COMMAND_PICTURE_PAINT_FONT_WEIGHT_NORMAL = 0;
  private static final int COMMAND_PICTURE_PAINT_FONT_WEIGHT_BOLD = 1;

  private static final int COMMAND_PICTURE_SHADER_LINEAR_GRADIENT = 1;
  private static final int COMMAND_PICTURE_SHADER_RADIAL_GRADIENT = 2;

  private static final int[] EMPTY_STATE_LIST = {};

  private static final String FONT_PATH = "subset_font.otf";

  private final Resources resources;
  private final WeakDrawableCache cacheMap = new WeakDrawableCache();
  private final Skin skin;
  private static volatile Optional<Typeface> typeface = Optional.absent();

  MozcDrawableFactory(Resources resources, Skin skin) {
    this.resources = Preconditions.checkNotNull(resources);
    this.skin = Preconditions.checkNotNull(skin);
    ensureTypeface(resources.getAssets());
  }

  Optional<Drawable> getDrawable(int resourceId) {
    if (!resources.getResourceTypeName(resourceId).equalsIgnoreCase("raw")) {
      // For non-"raw" resources, just delegate loading to Resources.
      return Optional.fromNullable(resources.getDrawable(resourceId));
    }

    Integer key = Integer.valueOf(resourceId);
    Optional<Drawable> drawable = cacheMap.get(key);
    if (!drawable.isPresent()) {
      InputStream stream = resources.openRawResource(resourceId);
      try {
        boolean success = false;
        try {
          drawable = createDrawable(new DataInputStream(stream), skin);
          success = true;
        } finally {
          MozcUtil.close(stream, !success);
        }
      } catch (IOException e) {
        MozcLog.e("Failed to parse file", e);
      }

      if (drawable.isPresent()) {
        cacheMap.put(key, drawable.get());
      }
    }
    return drawable;
  }

  private static Optional<Drawable> createDrawable(DataInputStream stream, Skin skin)
      throws IOException {
    Preconditions.checkNotNull(stream);

    byte tag = stream.readByte();
    switch (tag) {
      case DRAWABLE_PICTURE:
        return Optional.<Drawable>of(createBufferedPictureDrawable(stream, skin));
      case DRAWABLE_STATE_LIST:
        return Optional.<Drawable>of(createStateListDrawable(stream, skin));
      default:
        MozcLog.e("Unknown tag: " + tag);
    }
    return Optional.absent();
  }

  private static Drawable createBufferedPictureDrawable(DataInputStream stream, Skin skin)
      throws IOException {
    Preconditions.checkNotNull(stream);
    Preconditions.checkNotNull(skin);
    // The first eight bytes are width and height (four bytes for each).
    int width = stream.readUnsignedShort();
    int height = stream.readUnsignedShort();

    Picture picture = new Picture();
    Canvas canvas = picture.beginRecording(width, height);
    MozcStyle style = new MozcStyle();
    resetStyle(style);

    LOOP: while (true) {
      byte command = stream.readByte();
      switch (command) {
        case COMMAND_PICTURE_EOP:
          // The end of picture.
          break LOOP;
        case COMMAND_PICTURE_DRAW_PATH: {
          Path path = createPath(stream);
          int size = stream.readByte();
          if (size == 0) {
            resetStyle(style);
            canvas.drawPath(path, style.paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetStyle(style);
              parseStyle(stream, skin, style);
              canvas.drawPath(path, style.paint);
            }
          }
          break;
        }
        case COMMAND_PICTURE_DRAW_POLYLINE: {
          int length = stream.readUnsignedByte();
          if (length < 2 || length % 2 != 0) {
            throw new IllegalArgumentException();
          }
          float[] points = new float[length];
          for (int i = 0; i < length; ++i) {
            points[i] = readCompressedFloat(stream);
          }

          int size = stream.readByte();
          if (size == 0) {
            resetStyle(style);
            for (int i = 0; i < length - 2; i += 2) {
              canvas.drawLine(points[i], points[i + 1], points[i + 2], points[i + 3], style.paint);
            }
          } else {
            for (int i = 0; i < size; ++i) {
              resetStyle(style);
              parseStyle(stream, skin, style);
              for (int j = 0; j < length - 2; j += 2) {
                canvas.drawLine(points[j], points[j + 1], points[j + 2], points[j + 3],
                                style.paint);
              }
            }
          }
          break;
        }
        case COMMAND_PICTURE_DRAW_POLYGON: {
          int length = stream.readUnsignedByte();
          if (length < 2 || length % 2 != 0) {
            throw new IllegalArgumentException();
          }

          Path path = new Path();
          {
            float x = readCompressedFloat(stream);
            float y = readCompressedFloat(stream);
            path.moveTo(x, y);
          }
          for (int i = 2; i < length; i += 2) {
            float x = readCompressedFloat(stream);
            float y = readCompressedFloat(stream);
            path.lineTo(x, y);
          }
          path.close();

          int size = stream.readUnsignedByte();
          if (size == 0) {
            resetStyle(style);
            canvas.drawPath(path, style.paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetStyle(style);
              parseStyle(stream, skin, style);
              canvas.drawPath(path, style.paint);
            }
          }
          break;
        }
        case COMMAND_PICTURE_DRAW_LINE: {
          float x1 = readCompressedFloat(stream);
          float y1 = readCompressedFloat(stream);
          float x2 = readCompressedFloat(stream);
          float y2 = readCompressedFloat(stream);

          int size = stream.readUnsignedByte();
          if (size == 0) {
            resetStyle(style);
            canvas.drawLine(x1, y1, x2, y2, style.paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetStyle(style);
              parseStyle(stream, skin, style);
              canvas.drawLine(x1, y1, x2, y2, style.paint);
            }
          }
          break;
        }
        case COMMAND_PICTURE_DRAW_RECT: {
          float x = readCompressedFloat(stream);
          float y = readCompressedFloat(stream);
          float w = readCompressedFloat(stream);
          float h = readCompressedFloat(stream);

          int size = stream.readUnsignedByte();
          if (size == 0) {
            resetStyle(style);
            canvas.drawRect(x, y, x + w, y + h, style.paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetStyle(style);
              parseStyle(stream, skin, style);
              canvas.drawRect(x, y, x + w, y + h, style.paint);
            }
          }
          break;
        }
        case COMMAND_PICTURE_DRAW_CIRCLE: {
          float cx = readCompressedFloat(stream);
          float cy = readCompressedFloat(stream);
          float r = readCompressedFloat(stream);
          RectF bound = new RectF(cx - r, cy - r, cx + r, cy + r);

          int size = stream.readUnsignedByte();
          if (size == 0) {
            resetStyle(style);
            canvas.drawOval(bound, style.paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetStyle(style);
              parseStyle(stream, skin, style);
              canvas.drawOval(bound, style.paint);
            }
          }
          break;
        }
        case COMMAND_PICTURE_DRAW_ELLIPSE: {
          float cx = readCompressedFloat(stream);
          float cy = readCompressedFloat(stream);
          float rx = readCompressedFloat(stream);
          float ry = readCompressedFloat(stream);
          RectF bound = new RectF(cx - rx, cy - ry, cx + rx, cy + ry);

          int size = stream.readUnsignedByte();
          if (size == 0) {
            resetStyle(style);
            canvas.drawOval(bound, style.paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetStyle(style);
              parseStyle(stream, skin, style);
              canvas.drawOval(bound, style.paint);
            }
          }
          break;
        }
        case COMMAND_PICTURE_DRAW_GROUP_START: {
          float m11 = readCompressedFloat(stream);
          float m21 = readCompressedFloat(stream);
          float m31 = readCompressedFloat(stream);
          float m12 = readCompressedFloat(stream);
          float m22 = readCompressedFloat(stream);
          float m32 = readCompressedFloat(stream);
          float m13 = readCompressedFloat(stream);
          float m23 = readCompressedFloat(stream);
          float m33 = readCompressedFloat(stream);
          Matrix matrix = new Matrix();
          matrix.setValues(new float[] {m11, m12, m13, m21, m22, m23, m31, m32, m33});
          canvas.save();
          canvas.concat(matrix);
          break;
        }
        case COMMAND_PICTURE_DRAW_GROUP_END:
          canvas.restore();
          break;
        case COMMAND_PICTURE_DRAW_TEXT: {
          float x = readCompressedFloat(stream);
          float y = readCompressedFloat(stream);
          short stringSize = stream.readShort();
          byte[] stringBuffer = new byte[stringSize];
          stream.read(stringBuffer);
          String string = new String(stringBuffer, Charsets.UTF_8);
          int size = stream.readByte();
          if (size == 0) {
            resetStyle(style);
            canvas.drawText(string, x, y, style.paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetStyle(style);
              parseStyle(stream, skin, style);
              float drawY = style.dominantBaseline == COMMAND_PICTURE_PAINT_DOMINANTE_BASELINE_AUTO
                  ? y
                  : y - (style.paint.ascent() + style.paint.descent()) / 2;
              canvas.drawText(string, x, drawY, style.paint);
            }
          }
          break;
        }
        default:
          MozcLog.e("unknown command " + command);
      }
    }

    picture.endRecording();
    // H/W accelerated canvas doesn't support Picture so buffering is required.
    return new BufferedDrawable(new MozcPictureDrawable(picture));
  }

  private void ensureTypeface(AssetManager assetManager) {
    if (!typeface.isPresent()) {
      synchronized (typeface) {
        if (!typeface.isPresent()) {
          try {
            typeface = Optional.of(Typeface.createFromAsset(assetManager, FONT_PATH));
          } catch (RuntimeException e) {
            // Typeface cannot be made. Use default typeface as fallback.
            MozcLog.e(FONT_PATH + " is not accessible. Use system font.");
            typeface = Optional.of(Typeface.DEFAULT);
          }
        }
      }
    }
  }

  private static void resetStyle(MozcStyle style) {
    style.paint.reset();
    style.paint.setAntiAlias(true);
    style.paint.setTypeface(typeface.get());
    if (TEXT_LOCALE.isPresent()) {
      style.paint.setTextLocale(TEXT_LOCALE.get());
    }
    style.dominantBaseline = COMMAND_PICTURE_PAINT_DOMINANTE_BASELINE_AUTO;
  }

  private static void parseStyle(DataInputStream stream, Skin skin, MozcStyle style)
      throws IOException {
    Paint paint = style.paint;
    while (true) {
      int tag = stream.readByte() & 0xFF;
      if (tag >= 128) {
        // This is a bit tricky format, but the highest 1-bit means that the style should be
        // based on skin configuration. Delegate the paint to the skin.
        skin.apply(paint, tag & 0x7F);
        continue;
      }

      switch (tag) {
        case COMMAND_PICTURE_PAINT_EOP:
          return;
        case COMMAND_PICTURE_PAINT_STYLE: {
          paint.setStyle(Style.values()[stream.readUnsignedByte()]);
          break;
        }
        case COMMAND_PICTURE_PAINT_COLOR: {
          paint.setColor(stream.readInt());
          break;
        }
        case COMMAND_PICTURE_PAINT_SHADOW: {
          float r = readCompressedFloat(stream);
          float x = readCompressedFloat(stream);
          float y = readCompressedFloat(stream);
          int color = stream.readInt();
          paint.setShadowLayer(r, x, y, color);
          break;
        }
        case COMMAND_PICTURE_PAINT_STROKE_WIDTH: {
          paint.setStrokeWidth(readCompressedFloat(stream));
          break;
        }
        case COMMAND_PICTURE_PAINT_STROKE_CAP: {
          paint.setStrokeCap(Cap.values()[stream.readUnsignedByte()]);
          break;
        }
        case COMMAND_PICTURE_PAINT_STROKE_JOIN: {
          paint.setStrokeJoin(Join.values()[stream.readUnsignedByte()]);
          break;
        }
        case COMMAND_PICTURE_PAINT_SHADER: {
          paint.setShader(createShader(stream).orNull());
          break;
        }
        case COMMAND_PICTURE_PAINT_FONT_SIZE: {
          paint.setTextSize(readCompressedFloat(stream));
          break;
        }
        case COMMAND_PICTURE_PAINT_TEXT_ANCHOR: {
          byte value = stream.readByte();
          switch (value) {
            case COMMAND_PICTURE_PAINT_TEXT_ANCHOR_START:
              paint.setTextAlign(Align.LEFT);
              break;
            case COMMAND_PICTURE_PAINT_TEXT_ANCHOR_MIDDLE:
              paint.setTextAlign(Align.CENTER);
              break;
            case COMMAND_PICTURE_PAINT_TEXT_ANCHOR_END:
              paint.setTextAlign(Align.RIGHT);
              break;
            default:
              MozcLog.e("Unknown text-anchor : " + value, new Exception());
          }
          break;
        }
        case COMMAND_PICTURE_PAINT_DOMINANT_BASELINE: {
          style.dominantBaseline = stream.readByte();
          break;
        }
        case COMMAND_PICTURE_PAINT_FONT_WEIGHT: {
          style.paint.setFakeBoldText(stream.readByte() == COMMAND_PICTURE_PAINT_FONT_WEIGHT_BOLD);
          break;
        }
        default:
          MozcLog.e("Unknown paint tag: " + tag, new Exception());
      }
    }
  }

  private static Optional<Shader> createShader(DataInputStream stream) throws IOException {
    int tag = stream.readByte();
    switch (tag) {
      case COMMAND_PICTURE_SHADER_LINEAR_GRADIENT: {
        float x1 = readCompressedFloat(stream);
        float y1 = readCompressedFloat(stream);
        float x2 = readCompressedFloat(stream);
        float y2 = readCompressedFloat(stream);
        int length = stream.readUnsignedByte();
        int[] colors = new int[length];
        float[] points = new float[length];
        for (int i = 0; i < length; ++i) {
          colors[i] = stream.readInt();
        }
        for (int i = 0; i < length; ++i) {
          points[i] = readCompressedFloat(stream);
        }
        return Optional.<Shader>of(
            new LinearGradient(x1, y1, x2, y2, colors, points, TileMode.CLAMP));
      }
      case COMMAND_PICTURE_SHADER_RADIAL_GRADIENT: {
        float x = readCompressedFloat(stream);
        float y = readCompressedFloat(stream);
        float r = readCompressedFloat(stream);
        Matrix matrix = null;
        if (stream.readByte() != 0) {
          float m11 = readCompressedFloat(stream);
          float m21 = readCompressedFloat(stream);
          @SuppressWarnings("unused")
          float m12 = readCompressedFloat(stream);
          float m22 = readCompressedFloat(stream);
          float m13 = readCompressedFloat(stream);
          float m23 = readCompressedFloat(stream);
          matrix = new Matrix();
          matrix.setValues(new float[] {m11, m21, 0f, m21, m22, 0f, m13, m23, 1f});
        }
        int length = stream.readByte();
        int[] colors = new int[length];
        float[] points = new float[length];
        for (int i = 0; i < length; ++i) {
          colors[i] = stream.readInt();
        }
        for (int i = 0; i < length; ++i) {
          points[i] = readCompressedFloat(stream);
        }
        RadialGradient gradient = new RadialGradient(x, y, r, colors, points, TileMode.CLAMP);
        if (matrix != null) {
          gradient.setLocalMatrix(matrix);
        }
        return Optional.<Shader>of(gradient);
      }
      default:
        MozcLog.e("Unknown shader type: " + tag);
    }
    return Optional.absent();
  }

  private static Path createPath(DataInputStream stream) throws IOException {
    float startX = 0;
    float startY = 0;
    float prevX = 0;
    float prevY = 0;
    float prevControlX = 0;
    float prevControlY = 0;
    boolean hasPrevControl = false;

    Path path = new Path();
    while (true) {
      byte command = stream.readByte();
      switch (command) {
        case COMMAND_PICTURE_PATH_EOP:
          return path;
        case COMMAND_PICTURE_PATH_MOVE: {
          float x = readCompressedFloat(stream);
          float y = readCompressedFloat(stream);
          path.moveTo(x, y);
          startX = x;
          startY = y;
          prevX = x;
          prevY = y;
          hasPrevControl = false;
          break;
        }
        case COMMAND_PICTURE_PATH_LINE: {
          float x = readCompressedFloat(stream);
          float y = readCompressedFloat(stream);
          path.lineTo(x, y);
          prevX = x;
          prevY = y;
          hasPrevControl = false;
          break;
        }
        case COMMAND_PICTURE_PATH_HORIZONTAL_LINE: {
          float x = readCompressedFloat(stream);
          path.lineTo(x, prevY);
          prevX = x;
          hasPrevControl = false;
          break;
        }
        case COMMAND_PICTURE_PATH_VERTICAL_LINE: {
          float y = readCompressedFloat(stream);
          path.lineTo(prevX, y);
          prevY = y;
          hasPrevControl = false;
          break;
        }
        case COMMAND_PICTURE_PATH_CURVE: {
          float x1 = readCompressedFloat(stream);
          float y1 = readCompressedFloat(stream);
          float x2 = readCompressedFloat(stream);
          float y2 = readCompressedFloat(stream);
          float x = readCompressedFloat(stream);
          float y = readCompressedFloat(stream);
          path.cubicTo(x1, y1, x2, y2, x, y);
          prevX = x;
          prevY = y;
          prevControlX = x2;
          prevControlY = y2;
          hasPrevControl = true;
          break;
        }
        case COMMAND_PICTURE_PATH_CONTINUED_CURVE: {
          float x2 = readCompressedFloat(stream);
          float y2 = readCompressedFloat(stream);
          float x = readCompressedFloat(stream);
          float y = readCompressedFloat(stream);
          float x1, y1;
          if (hasPrevControl) {
            x1 = 2 * prevX - prevControlX;
            y1 = 2 * prevY - prevControlY;
          } else {
            x1 = prevX;
            y1 = prevY;
          }
          path.cubicTo(x1, y1, x2, y2, x, y);
          prevX = x;
          prevY = y;
          prevControlX = x2;
          prevControlY = y2;
          hasPrevControl = true;
          break;
        }
        case COMMAND_PICTURE_PATH_CLOSE: {
          path.close();
          path.moveTo(startX, startY);
          prevX = startX;
          prevY = startY;
          hasPrevControl = false;
          break;
        }
        default:
          MozcLog.e("Unknown command: " + command);
      }
    }
  }

  private static StateListDrawable createStateListDrawable(
      DataInputStream stream, Skin skin) throws IOException {
    int length = stream.readUnsignedByte();
    StateListDrawable result = new StateListDrawable();
    for (int i = 0; i < length; ++i) {
      int[] stateList = createStateList(stream);
      Optional<Drawable> drawable = createDrawable(stream, skin);
      result.addState(stateList, drawable.orNull());
    }
    return result;
  }

  private static int[] createStateList(DataInputStream stream) throws IOException {
    int length = stream.readUnsignedByte();
    if (length == 0) {
      return EMPTY_STATE_LIST;
    }
    int[] result = new int[length];
    for (int i = 0; i < length; ++i) {
      result[i] = stream.readInt();
    }
    return result;
  }

  private static float readCompressedFloat(DataInputStream stream) throws IOException {
    // Note: the precision of float is a bit too-much for the mozc purpose.
    // According to the precision in svg, 24-bits fixed-point value should be fine.
    // We can reduce the package size of the picture data to about 3/4 by the hack.
    // TODO(hidehiko): Implement 24-bits fixed-point value.
    return stream.readFloat();
  }
}
