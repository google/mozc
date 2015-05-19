# -*- coding: utf-8 -*-
# Copyright 2010-2013, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Utilities to create Mozc's .pic file from .svg file.

This module provides some utilities to create a .pic file, which is interpreted
in MechaMozc, from .svg files.
.pic file is a Mozc original format to represent vector image data shared by
all android platforms.
Note: another idea was to use binary data which is saved via PictureDrawable
on Android, but it seems that there are some differencces between devices,
and it'd cause an crash error, unfortunately.
"""

__author__ = "hidehiko"

import cStringIO as StringIO
import logging
import optparse
import os
import re
import struct
import sys
from xml.etree import cElementTree as ElementTree

from build_tools import util

# State const values defined in android.R.attr.
STATE_PRESSED = 0x010100a7
STATE_SELECTED = 0x010100a1
STATE_CHECKED = 0x010100a0

DRAWABLE_PICTURE = 1
DRAWABLE_STATE_LIST = 2
DRAWABLE_ANIMATION = 3

CMD_PICTURE_EOP = 0
CMD_PICTURE_DRAW_PATH = 1
CMD_PICTURE_DRAW_POLYLINE = 2
CMD_PICTURE_DRAW_POLYGON = 3
CMD_PICTURE_DRAW_LINE = 4
CMD_PICTURE_DRAW_RECT = 5
CMD_PICTURE_DRAW_CIRCLE = 6
CMD_PICTURE_DRAW_ELLIPSE = 7

CMD_PICTURE_PATH_EOP = 0
CMD_PICTURE_PATH_MOVE = 1
CMD_PICTURE_PATH_LINE = 2
CMD_PICTURE_PATH_HORIZONTAL_LINE = 3
CMD_PICTURE_PATH_VERTICAL_LINE = 4
CMD_PICTURE_PATH_CURVE = 5
CMD_PICTURE_PATH_CONTINUED_CURVE = 6
CMD_PICTURE_PATH_CLOSE = 7

CMD_PICTURE_PAINT_EOP = 0
CMD_PICTURE_PAINT_STYLE = 1
CMD_PICTURE_PAINT_COLOR = 2
CMD_PICTURE_PAINT_SHADOW = 3
CMD_PICTURE_PAINT_STROKE_WIDTH = 4
CMD_PICTURE_PAINT_STROKE_CAP = 5
CMD_PICTURE_PAINT_STROKE_JOIN = 6
CMD_PICTURE_PAINT_SHADER = 7

CMD_PICTURE_SHADER_LINEAR_GRADIENT = 1
CMD_PICTURE_SHADER_RADIAL_GRADIENT = 2

STYLE_CATEGORY_TAG = 128
STYLE_CATEGORY_KEYICON_MAIN = 0
STYLE_CATEGORY_KEYICON_GUIDE = 1
STYLE_CATEGORY_KEYICON_GUIDE_LIGHT = 2
STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT = 3
STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT = 4
STYLE_CATEGORY_KEYICON_BOUND = 5
STYLE_CATEGORY_KEYICON_FUNCTION = 6
STYLE_CATEGORY_KEYICON_FUNCTION_DARK = 7
STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW = 8
STYLE_CATEGORY_KEYICON_QWERTY_CAPS_ON_ARROW = 9
STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT = 10
STYLE_CATEGORY_KEYICON_POPUP_FUNCTION = 11
STYLE_CATEGORY_KEYICON_POPUP_FUNCTION_DARK = 12
# We may be able to reuse same resources for symbol major/minor icons.
STYLE_CATEGORY_SYMBOL_MAJOR = 13
STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED = 14
STYLE_CATEGORY_SYMBOL_MINOR = 15
STYLE_CATEGORY_SYMBOL_MINOR_SELECTED = 16
STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND = 17

# We'll check the category id by reverse sorted order, to resolve prefix match
# confliction.
STYLE_CATEGORY_MAP = sorted(
    [
        ('style-keyicon-main', STYLE_CATEGORY_KEYICON_MAIN),
        ('style-keyicon-guide', STYLE_CATEGORY_KEYICON_GUIDE),
        ('style-keyicon-guide-light', STYLE_CATEGORY_KEYICON_GUIDE_LIGHT),
        ('style-keyicon-main-highlight', STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT),
        ('style-keyicon-guide-highlight',
         STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT),
        ('style-keyicon-bound', STYLE_CATEGORY_KEYICON_BOUND),
        ('style-keyicon-function', STYLE_CATEGORY_KEYICON_FUNCTION),
        ('style-keyicon-function-dark', STYLE_CATEGORY_KEYICON_FUNCTION_DARK),
        ('style-keyicon-qwerty-shift-on-arrow',
         STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW),
        ('style-keyicon-qwerty-caps-on-arrow',
         STYLE_CATEGORY_KEYICON_QWERTY_CAPS_ON_ARROW),
        ('style-keypopup-highlight', STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT),
        ('style-keyicon-popup-function',
         STYLE_CATEGORY_KEYICON_POPUP_FUNCTION),
        ('style-keyicon-popup-function-dark',
         STYLE_CATEGORY_KEYICON_POPUP_FUNCTION_DARK),
        ('style-symbol-major', STYLE_CATEGORY_SYMBOL_MAJOR),
        ('style-symbol-major-selected', STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED),
        ('style-symbol-minor', STYLE_CATEGORY_SYMBOL_MINOR),
        ('style-symbol-minor-selected', STYLE_CATEGORY_SYMBOL_MINOR_SELECTED),
        ('style-keyboard-folding-button-background',
         STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND),
    ],
    reverse=True)


# Format of PictureDrawable:
# 1, width, height, {CMD_PICTURE_XXX sequence}.
#
# Format of StateListDrawable:
# 2, [[state_list], drawable]
COLOR_PATTERN = re.compile(r'#([0-9A-Fa-f]{6})')
PIXEL_PATTERN = re.compile(r'(\d+)px')
FLOAT_PATTERN = re.compile(r'^\s*,?\s*([+-]?\d+(?:\.\d*)?(:?e[+-]\d+)?)')
SHADER_PATTERN = re.compile(r'url\(#(.*)\)')
MATRIX_PATTERN = re.compile(r'matrix\((.*)\)')
STOP_COLOR_PATTERN = re.compile(r'stop-color:(.*)')

NO_SHADOW = 0
HAS_SHADOW = 1
HAS_BELOW_SHADOW = 2

class _OutputStream(object):
  """Simple wrapper of output stream by by packing value in big endian."""
  def __init__(self, output):
    self.output = output

  def WriteByte(self, value):
    if not (0 <= value <= 255):
      logging.critical('overflow')
      sys.exit(1)
    self.output.write(struct.pack('>B', value & 0xFF))

  def WriteInt16(self, value):
    if not (0 <= value <= 65535):
      logging.critical('overflow')
      sys.exit(1)
    self.output.write(struct.pack('>H', value & 0xFFFF))

  def WriteInt32(self, value):
    self.output.write(struct.pack('>I', value & 0xFFFFFFFF))

  def WriteFloat(self, value):
    # TODO(hidehiko): Because the precision of original values is thousandth,
    # so we can compress the data by using fixed-precision values.
    self.output.write(struct.pack('>f', value))

  def __enter__(self):
    self.output.__enter__()

  def __exit__(self):
    self.output.__exit__()


class MozcDrawableConverter(object):
  """Converter from .svg file to .pic file."""
  def __init__(self):
    pass

  # Definitions of svg parsing utilities.
  def _ParseColor(self, color):
    """Parses color attribute and returns int32 value or None."""
    if color == 'none':
      return None

    m = COLOR_PATTERN.match(color)
    if not m:
      return None

    c = int(m.group(1), 16)
    if c < 0 or 0x1000000 <= c:
      logging.critical('Out of color range: %s', color)
      sys.exit(1)
    # Set alpha.
    return c | 0xFF000000

  def _ParseShader(self, color, shader_map):
    """Parses shader attribute and returns a shader name from the given map."""
    if color == 'none':
      return None

    m = SHADER_PATTERN.match(color)
    if not m:
      return None
    return shader_map[m.group(1)]

  def _ParsePixel(self, s):
    """Parses pixel size from the given attribute value."""
    m = PIXEL_PATTERN.match(s)
    if not m:
      return None
    return int(m.group(1))

  def _ConsumeFloat(self, s):
    """Returns one floating value from string.

    Args:
      s: the target of parsing.
    Return:
      A tuple of the parsed floating value and the remaining string.
    """
    m = FLOAT_PATTERN.search(s)
    if not m:
      logging.critical('failed to consume float: %s', s)
      sys.exit(1)
    return float(m.group(1)), s[m.end():]

  def _ConsumeFloatList(self, s, num):
    """Parses num floating values from s."""
    result = []
    for _ in xrange(num):
      value, s = self._ConsumeFloat(s)
      result.append(value)
    return result, s

  def _ParseFloatList(self, s):
    """Parses floating list from string."""
    s = s.strip()
    result = []
    while s:
      value, s = self._ConsumeFloat(s)
      result.append(value)
    return result

  def _ParseShaderMap(self, node):
    if node.tag in ['{http://www.w3.org/2000/svg}g',
                    '{http://www.w3.org/2000/svg}svg']:
      result = {}
      for child in node:
        result.update(self._ParseShaderMap(child))
      return result

    if node.tag == '{http://www.w3.org/2000/svg}linearGradient':
      element_id = node.get('id')
      x1 = float(node.get('x1'))
      y1 = float(node.get('y1'))
      x2 = float(node.get('x2'))
      y2 = float(node.get('y2'))
      gradientTransform = node.get('gradientTransform')
      if gradientTransform:
        m = MATRIX_PATTERN.match(gradientTransform)
        (m11, m21, m12, m22, m13, m23) = self._ParseFloatList(m.group(1))
        (x1, y1) = (m11 * x1 + m12 * y1 + m13, m21 * x1 + m22 * y1 + m23)
        (x2, y2) = (m11 * x2 + m12 * y2 + m13, m21 * x2 + m22 * y2 + m23)

      color_list = self._ParseStopList(node)
      return { element_id: ('linear', x1, y1, x2, y2, color_list) }

    if node.tag == '{http://www.w3.org/2000/svg}radialGradient':
      element_id = node.get('id')
      cx = float(node.get('cx'))
      cy = float(node.get('cy'))
      r = float(node.get('r'))
      gradientTransform = node.get('gradientTransform')
      if gradientTransform:
        m = MATRIX_PATTERN.match(gradientTransform)
        matrix = self._ParseFloatList(m.group(1))
      else:
        matrix = None

      color_list = self._ParseStopList(node)
      return { element_id: ('radial', cx, cy, r, matrix, color_list) }

    return {}

  def _ParseStyle(self, node, has_shadow, shader_map):
    """Parses style attribute of the given node."""
    result = {}
    for attr in node.get('style', '').split(';'):
      attr = attr.strip()
      if not attr:
        continue
      command, arg = attr.split(':')
      if command == 'fill' or command == 'stroke':
        shader = self._ParseShader(arg, shader_map)
        color = self._ParseColor(arg)

        if shader is None and color is None:
          if arg != 'none':
            logging.error('Unknown pattern: %s', arg)
          continue

        paint_map = {}
        paint_map['style'] = command
        if shader is not None:
          paint_map['shader'] = shader
        if color is not None:
          paint_map['color'] = color
        paint_map['shadow'] = has_shadow

        result[command] = paint_map
        continue

      if command == 'stroke-width':
        paint_map = result['stroke']
        paint_map['stroke-width'] = float(arg)
        continue

      if command == 'stroke-linecap':
        paint_map = result['stroke']
        paint_map['stroke-linecap'] = arg
        continue

      if command == 'stroke-linejoin':
        paint_map = result['stroke']
        paint_map['stroke-linejoin'] = arg
        continue

    return sorted(result.values(), key=lambda e: e['style'])

  def _ParseStopList(self, parent_node):
    result = []
    for stop in parent_node:
      if stop.tag != '{http://www.w3.org/2000/svg}stop':
        logging.critical('unknown elem: %s', stop.tag)
        sys.exit(1)
      offset = float(stop.get('offset'))
      color = self._ParseColor(
          STOP_COLOR_PATTERN.match(stop.get('style')).group(1))
      result.append((offset, color))
    return result

  # Definition of conversion utilities.
  def _ConvertSize(self, tree, output):
    node = tree.getroot()
    width = self._ParsePixel(node.get('width'))
    height = self._ParsePixel(node.get('height'))

    if width is None or height is None:
      logging.critical('Unknown size')
      sys.exit(1)
    output.WriteInt16(width)
    output.WriteInt16(height)

  def _MaybeConvertShadow(self, has_shadow, output):
    if has_shadow == HAS_SHADOW:
      output.WriteByte(CMD_PICTURE_PAINT_SHADOW)
      output.WriteFloat(2.)
      output.WriteFloat(0.)
      output.WriteFloat(-1.)
      output.WriteInt32(0xFF404040)
      return

    if has_shadow == HAS_BELOW_SHADOW:
      output.WriteByte(CMD_PICTURE_PAINT_SHADOW)
      output.WriteFloat(4.)
      output.WriteFloat(0.)
      output.WriteFloat(2.)
      output.WriteInt32(0xFF292929)

  def _ConvertStyle(self, style, output):
    if style == 'fill':
      output.WriteByte(CMD_PICTURE_PAINT_STYLE)
      output.WriteByte(0)
      return

    if style == 'stroke':
      output.WriteByte(CMD_PICTURE_PAINT_STYLE)
      output.WriteByte(1)
      return

    logging.critical('unknown style: %s', style)
    sys.exit(1)

  def _MaybeConvertColor(self, color, output):
    if color is None:
      return
    output.WriteByte(CMD_PICTURE_PAINT_COLOR)
    output.WriteInt32(color)

  def _MaybeConvertShader(self, shader, output):
    if shader is None:
      return

    output.WriteByte(CMD_PICTURE_PAINT_SHADER)
    if shader[0] == 'linear':
      output.WriteByte(CMD_PICTURE_SHADER_LINEAR_GRADIENT)
      for coord in shader[1:5]:
        output.WriteFloat(coord)

      output.WriteByte(len(shader[5]))
      for _, color in shader[5]:
        output.WriteInt32(color)
      for offset, _ in shader[5]:
        output.WriteFloat(offset)
      return

    if shader[0] == 'radial':
      output.WriteByte(CMD_PICTURE_SHADER_RADIAL_GRADIENT)
      for coord in shader[1:4]:
        output.WriteFloat(coord)

      # Output matrix.
      if shader[4] is None:
        output.WriteByte(0)
      else:
        output.WriteByte(1)
        for coord in shader[4]:
          output.WriteFloat(coord)

      output.WriteByte(len(shader[5]))
      for _, color in shader[5]:
        output.WriteInt32(color)
      for offset, _ in shader[5]:
        output.WriteFloat(offset)
      return

    logging.critical('unknown shader: %s', shader[0])
    sys.exit(1)

  def _MaybeConvertStrokeWidth(self, stroke_width, output):
    if stroke_width is None:
      return

    output.WriteByte(CMD_PICTURE_PAINT_STROKE_WIDTH)
    output.WriteFloat(stroke_width)

  def _MaybeConvertStrokeLinecap(self, stroke_linecap, output):
    if stroke_linecap is None:
      return

    if stroke_linecap == 'round':
      output.WriteByte(CMD_PICTURE_PAINT_STROKE_CAP)
      output.WriteByte(1)
      return
    if stroke_linecap == 'square':
      output.WriteByte(CMD_PICTURE_PAINT_STROKE_CAP)
      output.WriteByte(2)
      return

    logging.critical('unknown stroke-linecap: %s', stroke_linecap)
    sys.exit(1)

  def _MaybeConvertStrokeLinejoin(self, stroke_linejoin, output):
    if stroke_linejoin is None:
      return

    if stroke_linejoin == 'round':
      output.WriteByte(CMD_PICTURE_PAINT_STROKE_JOIN)
      output.WriteByte(1)
      return

    if stroke_linejoin == 'bevel':
      output.WriteByte(CMD_PICTURE_PAINT_STROKE_JOIN)
      output.WriteByte(2)
      return

    logging.critical('unknown stroke-linejoin: %s', stroke_linejoin)
    sys.exit(1)

  def _ConvertStyleMap(self, style_map, output):
    self._ConvertStyle(style_map['style'], output)
    self._MaybeConvertColor(style_map.get('color'), output)
    self._MaybeConvertShader(style_map.get('shader'), output)
    self._MaybeConvertShadow(style_map['shadow'], output)
    self._MaybeConvertStrokeWidth(style_map.get('stroke-width'), output)
    self._MaybeConvertStrokeLinecap(style_map.get('stroke-linecap'), output)
    self._MaybeConvertStrokeLinejoin(style_map.get('stroke-linejoin'), output)
    output.WriteByte(CMD_PICTURE_PAINT_EOP)

  def _ConvertStyleList(self, style_list, output):
    output.WriteByte(len(style_list))
    for style_map in style_list:
      self._ConvertStyleMap(style_map, output)

  def _ConvertStyleCategory(self, style_category, output):
    output.WriteByte(1)
    for id_prefix, category in STYLE_CATEGORY_MAP:
      if style_category.startswith(id_prefix):
        output.WriteByte(STYLE_CATEGORY_TAG + category)
        return
    logging.critical('unknown style_category: "%s"', style_category)
    sys.exit(1)

  def _ConvertPath(self, node, output):
    path = node.get('d')
    if path is None:
      logging.critical('Unknown path')
      sys.exit(1)

    # TODO support continuous commands.
    prev_control = None
    prev = None
    command_list = []
    while True:
      path = path.strip()
      if not path:
        break
      command = path[0]

      if command == 'm' or command == 'M':
        # Move command.
        (x, y), path = self._ConsumeFloatList(path[1:], 2)
        if command == 'm' and prev is not None:
          x += prev[0]
          y += prev[1]
        command_list.append((CMD_PICTURE_PATH_MOVE, x, y))
        prev = (x, y)
        prev_control = None
        start = (x, y)
        continue

      if command == 'c' or command == 'C':
        # Cubic curve.
        (x1, y1, x2, y2, x, y), path = self._ConsumeFloatList(path[1:], 6)
        if command == 'c':
          x1 += prev[0]
          y1 += prev[1]
          x2 += prev[0]
          y2 += prev[1]
          x += prev[0]
          y += prev[1]
        command_list.append((CMD_PICTURE_PATH_CURVE, x1, y1, x2, y2, x, y))
        prev = (x, y)
        prev_control = (x2, y2)
        continue

      if command == 's' or command == 'S':
        # Continued cubic curve.
        (x2, y2, x, y), path = self._ConsumeFloatList(path[1:], 4)
        if command == 's':
          x2 += prev[0]
          y2 += prev[1]
          x += prev[0]
          y += prev[1]
        # if prev_control is not None:
        #   x1 = 2 * prev[0] - prev_control[0]
        #   y1 = 2 * prev[1] - prev_control[1]
        # else:
        #   x1, y1 = prev
        command_list.append((CMD_PICTURE_PATH_CONTINUED_CURVE, x2, y2, x, y))
        prev = (x, y)
        prev_control = (x2, y2)
        continue

      if command == 'h' or command == 'H':
        # Horizontal line.
        x, path = self._ConsumeFloat(path[1:])
        if command == 'h':
          x += prev[0]
        y = prev[1]
        command_list.append((CMD_PICTURE_PATH_HORIZONTAL_LINE, x))
        prev = (x, y)
        prev_control = None
        continue

      if command == 'v' or command == 'V':
        # Vertical line.
        y, path = self._ConsumeFloat(path[1:])
        if command == 'v':
          y += prev[1]
        x = prev[0]
        command_list.append((CMD_PICTURE_PATH_VERTICAL_LINE, y))
        prev = (x, y)
        prev_control = None
        continue

      if command == 'l' or command == 'L':
        # Line.
        (x, y), path = self._ConsumeFloatList(path[1:], 2)
        if command == 'l':
          x += prev[0]
          y += prev[1]
        command_list.append((CMD_PICTURE_PATH_LINE, x, y))
        prev = (x, y)
        prev_control = None
        continue

      if command == 'z' or command == 'Z':
        # Close the path.
        command_list.append((CMD_PICTURE_PATH_CLOSE,))
        path = path[1:]
        prev = start
        prev_control = None
        continue

      logging.critical('Unknown command: %s', path)
      sys.exit(1)

    command_list.append((CMD_PICTURE_PATH_EOP,))

    # Output.
    output.WriteByte(CMD_PICTURE_DRAW_PATH)
    for command in command_list:
      output.WriteByte(command[0])
      for coord in command[1:]:
        output.WriteFloat(coord)

  def _ConvertPathElement(
      self, node, style_category, has_shadow, shader_map, output):
    style_list = self._ParseStyle(node, has_shadow, shader_map)
    self._ConvertPath(node, output)
    if style_category is not None:
      self._ConvertStyleCategory(style_category, output)
    else:
      self._ConvertStyleList(style_list, output)

  def _ConvertPolylineElement(
      self, node, style_category, has_shadow, shader_map, output):
    point_list = self._ParseFloatList(node.get('points'))
    if len(point_list) < 2:
      logging.critical('Invalid point number.')
      sys.exit(1)
    style_list = self._ParseStyle(node, has_shadow, shader_map)

    output.WriteByte(CMD_PICTURE_DRAW_POLYLINE)
    output.WriteByte(len(point_list))
    for coord in point_list:
      output.WriteFloat(coord)
    if style_category is not None:
      self._ConvertStyleCategory(style_category, output)
    else:
      self._ConvertStyleList(style_list, output)

  def _ConvertPolygonElement(
      self, node, style_category, has_shadow, shader_map, output):
    style_list = self._ParseStyle(node, has_shadow, shader_map)
    point_list = self._ParseFloatList(node.get('points'))

    output.WriteByte(CMD_PICTURE_DRAW_POLYGON)
    output.WriteByte(len(point_list))
    for coord in point_list:
      output.WriteFloat(coord)
    if style_category is not None:
      self._ConvertStyleCategory(style_category, output)
    else:
      self._ConvertStyleList(style_list, output)

  def _ConvertLineElement(
      self, node, style_category, has_shadow, shader_map, output):
    style_list = self._ParseStyle(node, has_shadow, shader_map)
    x1 = float(node.get('x1'))
    y1 = float(node.get('y1'))
    x2 = float(node.get('x2'))
    y2 = float(node.get('y2'))
    output.WriteByte(CMD_PICTURE_DRAW_LINE)
    output.WriteFloat(x1)
    output.WriteFloat(y1)
    output.WriteFloat(x2)
    output.WriteFloat(y2)
    if style_category is not None:
      self._ConvertStyleCategory(style_category, output)
    else:
      self._ConvertStyleList(style_list, output)

  def _ConvertCircleElement(
      self, node, style_category, has_shadow, shader_map, output):
    style_list = self._ParseStyle(node, has_shadow, shader_map)
    cx = float(node.get('cx'))
    cy = float(node.get('cy'))
    r = float(node.get('r'))
    output.WriteByte(CMD_PICTURE_DRAW_CIRCLE)
    output.WriteFloat(cx)
    output.WriteFloat(cy)
    output.WriteFloat(r)
    if style_category is not None:
      self._ConvertStyleCategory(style_category, output)
    else:
      self._ConvertStyleList(style_list, output)

  def _ConvertEllipseElement(
      self, node, style_category, has_shadow, shader_map, output):
    style_list = self._ParseStyle(node, has_shadow, shader_map)
    cx = float(node.get('cx'))
    cy = float(node.get('cy'))
    rx = float(node.get('rx'))
    ry = float(node.get('ry'))
    output.WriteByte(CMD_PICTURE_DRAW_ELLIPSE)
    output.WriteFloat(cx)
    output.WriteFloat(cy)
    output.WriteFloat(rx)
    output.WriteFloat(ry)
    if style_category is not None:
      self._ConvertStyleCategory(style_category, output)
    else:
      self._ConvertStyleList(style_list, output)

  def _ConvertRectElement(
      self, node, style_category, has_shadow, shader_map, output):
    style_list = self._ParseStyle(node, has_shadow, shader_map)
    x = float(node.get('x', 0))
    y = float(node.get('y', 0))
    w = float(node.get('width'))
    h = float(node.get('height'))
    output.WriteByte(CMD_PICTURE_DRAW_RECT)
    output.WriteFloat(x)
    output.WriteFloat(y)
    output.WriteFloat(w)
    output.WriteFloat(h)
    if style_category is not None:
      self._ConvertStyleCategory(style_category, output)
    else:
      self._ConvertStyleList(style_list, output)

  def _ConvertPictureSequence(
      self, node, style_category, has_shadow, shader_map, output):
    # Hack. To support shadow, we use 'id' attribute.
    # If the 'id' starts with 'shadow', it means the element and its children
    # has shadow.
    nodeid = node.get('id', '')
    if nodeid.startswith('shadow'):
      has_shadow = HAS_SHADOW
    elif nodeid.startswith('below_x5F_shadow'):
      has_shadow = HAS_BELOW_SHADOW

    if nodeid.startswith('style-'):
      style_category = nodeid

    if node.tag == '{http://www.w3.org/2000/svg}path':
      self._ConvertPathElement(
          node, style_category, has_shadow, shader_map, output)
      return

    if node.tag == '{http://www.w3.org/2000/svg}polyline':
      self._ConvertPolylineElement(
          node, style_category, has_shadow, shader_map, output)
      return

    if node.tag == '{http://www.w3.org/2000/svg}polygon':
      self._ConvertPolygonElement(
          node, style_category, has_shadow, shader_map, output)
      return

    if node.tag == '{http://www.w3.org/2000/svg}line':
      self._ConvertLineElement(
          node, style_category, has_shadow, shader_map, output)
      return

    if node.tag == '{http://www.w3.org/2000/svg}circle':
      self._ConvertCircleElement(
          node, style_category, has_shadow, shader_map, output)
      return

    if node.tag == '{http://www.w3.org/2000/svg}ellipse':
      self._ConvertEllipseElement(
          node, style_category, has_shadow, shader_map, output)
      return

    if node.tag == '{http://www.w3.org/2000/svg}rect':
      self._ConvertRectElement(
          node, style_category, has_shadow, shader_map, output)
      return

    if node.tag in ['{http://www.w3.org/2000/svg}g',
                    '{http://www.w3.org/2000/svg}svg']:
      # Flatten child nodes.
      for child in node:
        self._ConvertPictureSequence(
            child, style_category, has_shadow, shader_map, output)
      return

    if node.tag in ['{http://www.w3.org/2000/svg}linearGradient',
                    '{http://www.w3.org/2000/svg}radialGradient']:
      return

    logging.warning('Unknown element: %s', node.tag)

  def _OutputEOP(self, output):
    output.WriteByte(CMD_PICTURE_EOP)

  def _ConvertPictureDrawableInternal(self, tree, output):
    output.WriteByte(DRAWABLE_PICTURE)
    shader_map = self._ParseShaderMap(tree.getroot())
    self._ConvertSize(tree, output)
    self._ConvertPictureSequence(
        tree.getroot(), None, NO_SHADOW, shader_map, output)
    self._OutputEOP(output)

  # Interface for drawable conversion.
  def ConvertPictureDrawable(self, path):
    output = _OutputStream(StringIO.StringIO())
    self._ConvertPictureDrawableInternal(ElementTree.parse(path), output)
    return output.output.getvalue()

  def ConvertStateListDrawable(self, drawable_source_list):
    output = _OutputStream(StringIO.StringIO())
    output.WriteByte(DRAWABLE_STATE_LIST)
    output.WriteByte(len(drawable_source_list))
    for (state_list, path) in drawable_source_list:
      # Output state.
      output.WriteByte(len(state_list))
      for state in state_list:
        output.WriteInt32(state)

      # Output drawable.
      self._ConvertPictureDrawableInternal(ElementTree.parse(path), output)
    return output.output.getvalue()

  # This method is actually not used, but we can use it to create animation
  # drawables.
  def ConvertAnimationDrawable(self, drawable_source_list):
    output = _OutputStream(StringIO.StringIO())
    output.WriteByte(DRAWABLE_ANIMATION)
    output.WriteByte(len(drawable_source_list))
    for (duration, path) in drawable_source_list:
      # Output duration and corresponding picture drawable.
      output.WriteInt16(duration)
      self._ConvertPictureDrawableInternal(ElementTree.parse(path), output)
    return output.output.getvalue()


def ConvertFiles(svg_dir, output_dir):
  # Ensure that the output directory exists.
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)

  converter = MozcDrawableConverter()
  for dirpath, dirnames, filenames in os.walk(svg_dir):
    for filename in filenames:
      basename, ext = os.path.splitext(filename)
      if ext != '.svg':
        # Do nothing for files other than svg.
        continue

      # Filename hack to generate stateful .pic files.
      if basename.endswith('_release') or basename.endswith('_selected'):
        # 'XXX_release.svg' files will be processed with corresponding
        # '_center.svg' files. So just skip them.
        # As similar to it, '_selected.svg' files will be processed with
        # corresponding non-selected .svg files.
        continue

      if basename == 'keyboard_fold_tab_up':
        # 'keyboard_fold_tab_up.svg' file will be processed with
        # 'keyboard_fold_tab_down.svg.' Just skip it, too.
        continue

      logging.info('Converting %s...', filename)

      if basename.endswith('_center'):
        # Process '_center.svg' file with '_release.svg' file to make
        # stateful drawable.
        center_svg_file = os.path.join(dirpath, filename)
        release_svg_file = os.path.join(
            dirpath, basename[:-7] + '_release.svg')
        pic_file = os.path.join(output_dir, basename + '.pic')
        pic_data = converter.ConvertStateListDrawable(
            [([STATE_PRESSED], center_svg_file), ([], release_svg_file)])
      elif os.path.exists(os.path.join(dirpath, basename + '_selected.svg')):
        # Process '_selected.svg' file at the same time if necessary.
        unselected_svg_file = os.path.join(dirpath, filename)
        selected_svg_file = os.path.join(dirpath, basename + '_selected.svg')
        pic_file = os.path.join(output_dir, basename + '.pic')
        pic_data = converter.ConvertStateListDrawable(
            [([STATE_SELECTED], selected_svg_file), ([], unselected_svg_file)])
      elif basename == 'keyboard_fold_tab_down':
        # Special hack for keyboard__fold__tab.pic.
        down_svg_file = os.path.join(dirpath, filename)
        up_svg_file = os.path.join(dirpath, 'keyboard_fold_tab_up.svg')
        pic_file = os.path.join(output_dir, 'keyboard__fold__tab.pic')
        pic_data = converter.ConvertStateListDrawable(
            [([STATE_CHECKED], up_svg_file), ([], down_svg_file)])
      else:
        # Normal .svg file.
        svg_file = os.path.join(dirpath, filename)
        pic_file = os.path.join(output_dir, basename + '.pic')
        pic_data = converter.ConvertPictureDrawable(svg_file)

      with open(pic_file, 'wb') as stream:
        stream.write(pic_data)


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--svg_dir', dest='svg_dir',
                    help='Path to a directory containing .svg files.')
  parser.add_option('--output_dir', dest='output_dir',
                    help='Path to the output directory,')
  return parser.parse_args()[0]


def main():
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
  logging.getLogger().addFilter(util.ColoredLoggingFilter())

  options = ParseOptions()
  ConvertFiles(options.svg_dir, options.output_dir)


if __name__ == '__main__':
  main()
