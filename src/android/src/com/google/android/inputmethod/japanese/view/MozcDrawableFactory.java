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

package org.mozc.android.inputmethod.japanese.view;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Paint.Cap;
import android.graphics.Paint.Join;
import android.graphics.Paint.Style;
import android.graphics.Path;
import android.graphics.Picture;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.PictureDrawable;
import android.graphics.drawable.StateListDrawable;

import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;

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
public class MozcDrawableFactory {
  private static final int DRAWABLE_PICTURE = 1;
  private static final int DRAWABLE_STATE_LIST = 2;
  private static final int DRAWABLE_ANIMATION = 3;

  private static final int COMMAND_PICTURE_EOP = 0;
  private static final int COMMAND_PICTURE_DRAW_PATH = 1;
  private static final int COMMAND_PICTURE_DRAW_POLYLINE = 2;
  private static final int COMMAND_PICTURE_DRAW_POLYGON = 3;
  private static final int COMMAND_PICTURE_DRAW_LINE = 4;
  private static final int COMMAND_PICTURE_DRAW_RECT = 5;
  private static final int COMMAND_PICTURE_DRAW_CIRCLE = 6;
  private static final int COMMAND_PICTURE_DRAW_ELLIPSE = 7;

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

  private static final int COMMAND_PICTURE_SHADER_LINEAR_GRADIENT = 1;
  private static final int COMMAND_PICTURE_SHADER_RADIAL_GRADIENT = 2;

  private static final int[] EMPTY_STATE_LIST = {};

  private final Resources resources;
  private final WeakDrawableCache cacheMap = new WeakDrawableCache();
  private SkinType skinType = SkinType.ORANGE_LIGHTGRAY;

  public MozcDrawableFactory(Resources resources) {
    this.resources = resources;
  }

  // TODO(hidehiko): Add test to make sure getDrawable returns diffenent instances for
  // the same resourceId when the current skinType gets different.
  public void setSkinType(SkinType skinType) {
    if (skinType == null) {
      throw new NullPointerException();
    }
    if (this.skinType != skinType) {
      this.skinType = skinType;
      // Invalidate cache.
      cacheMap.clear();
    }
  }

  public Drawable getDrawable(int resourceId) {
    if (!resources.getResourceTypeName(resourceId).equalsIgnoreCase("raw")) {
      // For non-"raw" resources, just delegate loading to Resources.
      return resources.getDrawable(resourceId);
    }

    Integer key = Integer.valueOf(resourceId);
    Drawable drawable = cacheMap.get(key);
    if (drawable == null) {
      InputStream stream = resources.openRawResource(resourceId);
      try {
        boolean success = false;
        try {
          drawable = createDrawable(new DataInputStream(stream), skinType);
          success = true;
        } finally {
          MozcUtil.close(stream, !success);
        }
      } catch (IOException e) {
        MozcLog.e("Failed to parse file", e);
      }

      if (drawable != null) {
        cacheMap.put(key, drawable);
      }
    }
    return drawable;
  }

  private static Drawable createDrawable(DataInputStream stream, SkinType skinType)
      throws IOException {
    byte tag = stream.readByte();
    switch (tag) {
      case DRAWABLE_PICTURE:
        return createPictureDrawable(stream, skinType);
      case DRAWABLE_STATE_LIST:
        return createStateListDrawable(stream, skinType);
      case DRAWABLE_ANIMATION:
        return createEmojiAnimationDrawable(stream, skinType);
      default:
        MozcLog.e("Unknown tag: " + tag);
    }
    return null;
  }

  // Note, PictureDrawable may cause runtime slowness.
  // Instead, we can prepare pre-rendered bit-map drawable, by modifying the interface to take
  // the drawable, which should be faster theoretically.
  private static PictureDrawable createPictureDrawable(DataInputStream stream, SkinType skinType)
      throws IOException {
    // The first eight bytes are width and height (four bytes for each).
    int width = stream.readUnsignedShort();
    int height = stream.readUnsignedShort();

    Picture picture = new Picture();
    Canvas canvas = picture.beginRecording(width, height);
    Paint paint = new Paint();
    resetPaint(paint);

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
            resetPaint(paint);
            canvas.drawPath(path, paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetPaint(paint);
              parsePaint(stream, skinType, paint);
              canvas.drawPath(path, paint);
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
            resetPaint(paint);
            for (int i = 0; i < length - 2; i += 2) {
              canvas.drawLine(points[i], points[i + 1], points[i + 2], points[i + 3], paint);
            }
          } else {
            for (int i = 0; i < size; ++i) {
              resetPaint(paint);
              parsePaint(stream, skinType, paint);
              for (int j = 0; j < length - 2; j += 2) {
                canvas.drawLine(points[j], points[j + 1], points[j + 2], points[j + 3], paint);
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
            resetPaint(paint);
            canvas.drawPath(path, paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetPaint(paint);
              parsePaint(stream, skinType, paint);
              canvas.drawPath(path, paint);
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
            resetPaint(paint);
            canvas.drawLine(x1, y1, x2, y2, paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetPaint(paint);
              parsePaint(stream, skinType, paint);
              canvas.drawLine(x1, y1, x2, y2, paint);
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
            resetPaint(paint);
            canvas.drawRect(x, y, x + w, y + h, paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetPaint(paint);
              parsePaint(stream, skinType, paint);
              canvas.drawRect(x, y, x + w, y + h, paint);
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
            resetPaint(paint);
            canvas.drawOval(bound, paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetPaint(paint);
              parsePaint(stream, skinType, paint);
              canvas.drawOval(bound, paint);
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
            resetPaint(paint);
            canvas.drawOval(bound, paint);
          } else {
            for (int i = 0; i < size; ++i) {
              resetPaint(paint);
              parsePaint(stream, skinType, paint);
              canvas.drawOval(bound, paint);
            }
          }
          break;
        }
        default:
          MozcLog.e("unknown command " + command);
      }
    }

    picture.endRecording();
    return new MozcPictureDrawable(picture);
  }

  private static void resetPaint(Paint paint) {
    paint.reset();
    paint.setAntiAlias(true);
  }

  private static void parsePaint(DataInputStream stream, SkinType skinType, Paint paint)
      throws IOException {
    while (true) {
      int tag = stream.readByte() & 0xFF;
      if (tag >= 128) {
        // This is a bit tricky format, but the highest 1-bit means that the style should be
        // based on skin configuration. Delegate the paint to the skin.
        skinType.apply(paint, tag & 0x7F);
        return;
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
          paint.setShader(createShader(stream));
          break;
        }
        default:
          MozcLog.e("Unknown paint tag: " + tag, new Exception());
      }
    }
  }

  private static Shader createShader(DataInputStream stream) throws IOException {
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
        return new LinearGradient(x1, y1, x2, y2, colors, points, TileMode.CLAMP);
      }
      case COMMAND_PICTURE_SHADER_RADIAL_GRADIENT: {
        float x = readCompressedFloat(stream);
        float y = readCompressedFloat(stream);
        float r = readCompressedFloat(stream);
        Matrix matrix = null;
        if (stream.readByte() != 0) {
          float m11 = readCompressedFloat(stream);
          float m21 = readCompressedFloat(stream);
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
        return gradient;
      }
      default:
        MozcLog.e("Unknown shader type: " + tag);
    }
    return null;
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
      DataInputStream stream, SkinType skinType) throws IOException {
    int length = stream.readUnsignedByte();
    StateListDrawable result = new StateListDrawable();
    for (int i = 0; i < length; ++i) {
      int[] stateList = createStateList(stream);
      Drawable drawable = createDrawable(stream, skinType);
      result.addState(stateList, drawable);
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

  private static EmojiAnimationDrawable createEmojiAnimationDrawable(
      DataInputStream stream, SkinType skinType) throws IOException {
    int length = stream.readUnsignedByte();
    EmojiAnimationDrawable result = new EmojiAnimationDrawable();
    for (int i = 0; i < length; ++i) {
      int duration = stream.readUnsignedShort();
      Drawable frame = createDrawable(stream, skinType);
      result.addFrame(frame, duration);
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
