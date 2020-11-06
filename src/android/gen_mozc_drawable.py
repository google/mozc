# -*- coding: utf-8 -*-
# Copyright 2010-2020, Google Inc.
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

import io
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

DRAWABLE_PICTURE = 1
DRAWABLE_STATE_LIST = 2

CMD_PICTURE_EOP = 0
CMD_PICTURE_DRAW_PATH = 1
CMD_PICTURE_DRAW_POLYLINE = 2
CMD_PICTURE_DRAW_POLYGON = 3
CMD_PICTURE_DRAW_LINE = 4
CMD_PICTURE_DRAW_RECT = 5
CMD_PICTURE_DRAW_CIRCLE = 6
CMD_PICTURE_DRAW_ELLIPSE = 7
CMD_PICTURE_DRAW_GROUP_START = 8
CMD_PICTURE_DRAW_GROUP_END = 9
CMD_PICTURE_DRAW_TEXT = 10

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
CMD_PICTURE_PAINT_FONT_SIZE = 8
CMD_PICTURE_PAINT_TEXT_ANCHOR = 9
CMD_PICTURE_PAINT_DOMINANT_BASELINE = 10
CMD_PICTURE_PAINT_FONT_WEIGHT = 11

CMD_PICTURE_PAINT_TEXT_ANCHOR_START = 0
CMD_PICTURE_PAINT_TEXT_ANCHOR_MIDDLE = 1
CMD_PICTURE_PAINT_TEXT_ANCHOR_END = 2

CMD_PICTURE_PAINT_DOMINANTE_BASELINE_AUTO = 0
CMD_PICTURE_PAINT_DOMINANTE_BASELINE_CENTRAL = 1

CMD_PICTURE_PAINT_FONT_WEIGHT_NORMAL = 0
CMD_PICTURE_PAINT_FONT_WEIGHT_BOLD = 1

CMD_PICTURE_SHADER_LINEAR_GRADIENT = 1
CMD_PICTURE_SHADER_RADIAL_GRADIENT = 2

STYLE_CATEGORY_TAG = 128
STYLE_CATEGORY_KEYICON_MAIN = 0
STYLE_CATEGORY_KEYICON_GUIDE = 1
STYLE_CATEGORY_KEYICON_GUIDE_LIGHT = 2
STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT = 3
STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT = 4
STYLE_CATEGORY_KEYICON_BOUND = 5
STYLE_CATEGORY_KEYICON_TWELVEKEYS_FUNCTION = 6
STYLE_CATEGORY_KEYICON_TWELVEKEYS_GLOBE = 7
STYLE_CATEGORY_KEYICON_QWERTY_FUNCTION = 8
STYLE_CATEGORY_KEYICON_FUNCTION_DARK = 9
STYLE_CATEGORY_KEYICON_ENTER = 10
STYLE_CATEGORY_KEYICON_ENTER_CIRCLE = 11
STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW = 12
STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT = 13
# We may be able to reuse same resources for symbol major/minor icons.
STYLE_CATEGORY_SYMBOL_MAJOR = 14
STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED = 15
STYLE_CATEGORY_SYMBOL_MAJOR_EMOJI_DISABLE_CIRCLE = 16
STYLE_CATEGORY_SYMBOL_MINOR = 17
STYLE_CATEGORY_SYMBOL_MINOR_SELECTED = 18
STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND_DEFAULT = 19
STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND_SCROLLED = 20

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
        ('style-keyicon-twelvekeys-function',
         STYLE_CATEGORY_KEYICON_TWELVEKEYS_FUNCTION),
        ('style-keyicon-twelvekeys-globe',
         STYLE_CATEGORY_KEYICON_TWELVEKEYS_GLOBE),
        ('style-keyicon-qwerty-function',
         STYLE_CATEGORY_KEYICON_QWERTY_FUNCTION),
        ('style-keyicon-function-dark', STYLE_CATEGORY_KEYICON_FUNCTION_DARK),
        ('style-keyicon-enter', STYLE_CATEGORY_KEYICON_ENTER),
        ('style-keyicon-enter-circle', STYLE_CATEGORY_KEYICON_ENTER_CIRCLE),
        ('style-keyicon-qwerty-shift-on-arrow',
         STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW),
        ('style-keypopup-highlight', STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT),
        ('style-symbol-major', STYLE_CATEGORY_SYMBOL_MAJOR),
        ('style-symbol-major-selected', STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED),
        ('style-symbol-major-emoji-disable-circle',
         STYLE_CATEGORY_SYMBOL_MAJOR_EMOJI_DISABLE_CIRCLE),
        ('style-symbol-minor', STYLE_CATEGORY_SYMBOL_MINOR),
        ('style-symbol-minor-selected', STYLE_CATEGORY_SYMBOL_MINOR_SELECTED),
        ('style-keyboard-folding-button-background-default',
         STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND_DEFAULT),
        ('style-keyboard-folding-button-background-scrolled',
         STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND_SCROLLED),
    ],
    reverse=True)


# Format of PictureDrawable:
# 1, width, height, {CMD_PICTURE_XXX sequence}.
#
# Format of StateListDrawable:
# 2, [[state_list], drawable]
COLOR_PATTERN = re.compile(r'#([0-9A-Fa-f]{3,8}(?![0-9A-Fa-f]))')
PIXEL_PATTERN = re.compile(r'(\d+)(?:px)?')
FLOAT_PATTERN = re.compile(r'^\s*,?\s*([+-]?\d+(?:\.\d*)?(:?e[+-]\d+)?)')
SHADER_PATTERN = re.compile(r'url\(#(.*)\)')
TRANSFORM_PATTERN = re.compile(r'(matrix|translate|scale)\((.*)\)')
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
      logging.critical('overflow byte')
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

  def WriteString(self, value):
    utf8_string = value.encode('utf-8')
    self.WriteInt16(len(utf8_string))
    self.output.write(utf8_string)

  def __enter__(self):
    self.output.__enter__()

  def __exit__(self, exec_type, exec_value, traceback):
    self.output.__exit__(exec_type, exec_value, traceback)


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

    c_str = m.group(1)
    if 3 <= len(c_str) <= 4:
      expanded_c_str = ''
      for ch in c_str:
        expanded_c_str = expanded_c_str + ch + ch
      c_str = expanded_c_str
    if len(c_str) == 6:
      c_str = 'FF' + c_str
    if len(c_str) != 8:
      logging.critical('Invalid color format.')
      sys.exit(1)
    c = int(c_str, 16)
    if c < 0 or 0x100000000 <= c:
      logging.critical('Out of color range: %s', color)
      sys.exit(1)
    return c

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
    for _ in range(num):
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
      gradient_transform = node.get('gradientTransform')
      if gradient_transform:
        m = MATRIX_PATTERN.match(gradient_transform)
        (m11, m21, m12, m22, m13, m23) = tuple(self._ParseFloatList(m.group(1)))
        (x1, y1) = (m11 * x1 + m12 * y1 + m13, m21 * x1 + m22 * y1 + m23)
        (x2, y2) = (m11 * x2 + m12 * y2 + m13, m21 * x2 + m22 * y2 + m23)

      color_list = self._ParseStopList(node)
      return {element_id: ('linear', x1, y1, x2, y2, color_list)}

    if node.tag == '{http://www.w3.org/2000/svg}radialGradient':
      element_id = node.get('id')
      cx = float(node.get('cx'))
      cy = float(node.get('cy'))
      r = float(node.get('r'))
      gradient_transform = node.get('gradientTransform')
      if gradient_transform:
        m = MATRIX_PATTERN.match(gradient_transform)
        matrix = self._ParseFloatList(m.group(1))
      else:
        matrix = None

      color_list = self._ParseStopList(node)
      return {element_id: ('radial', cx, cy, r, matrix, color_list)}

    return {}

  def _ParseStyle(self, node, has_shadow, shader_map):
    """Parses style attribute of the given node."""
    common_map = {}
    # Default fill color is black (SVG's spec)
    fill_map = {'style': 'fill', 'color': 0xFF000000, 'shadow': has_shadow}
    # Default stroke color is none (SVG's spec)
    stroke_map = {'style': 'stroke', 'shadow': has_shadow}

    # Special warning for font-size.
    # Inkscape often unexpectedly converts from sytle to font-size attribute.
    if node.get('font-size', ''):
      logging.warning('font-size attribute is not supported.')

    for attr in node.get('style', '').split(';'):
      attr = attr.strip()
      if not attr:
        continue
      command, arg = attr.split(':')
      if command == 'fill' or command == 'stroke':
        paint_map = fill_map if command == 'fill' else stroke_map
        if arg == 'none':
          paint_map.pop('color', None)
          paint_map.pop('shader', None)
          continue

        shader = self._ParseShader(arg, shader_map)
        color = self._ParseColor(arg)
        if shader is None and color is None:
          if arg != 'none':
            logging.critical('Unknown pattern: %s', arg)
            sys.exit(1)
          continue

        paint_map['style'] = command
        if shader is not None:
          paint_map['shader'] = shader
        if color is not None:
          paint_map['color'] = color
        continue

      if command == 'stroke-width':
        stroke_map['stroke-width'] = float(arg)
        continue

      if command == 'stroke-linecap':
        stroke_map['stroke-linecap'] = arg
        continue

      if command == 'stroke-linejoin':
        stroke_map['stroke-linejoin'] = arg
        continue

      # font relating attributes are common to all commands.
      if command == 'font-size':
        common_map['font-size'] = self._ParsePixel(arg)
      if command == 'text-anchor':
        common_map['text-anchor'] = arg
      if command == 'dominant-baseline':
        common_map['dominant-baseline'] = arg
      if command == 'font-weight':
        common_map['font-weight'] = arg

    # 'fill' comes first in order to draw 'fill' first (SVG specification).
    result = []
    if 'color' in fill_map or 'shader' in fill_map:
      fill_map.update(common_map)
      result.append(fill_map)
    if 'color' in stroke_map or 'shader' in stroke_map:
      stroke_map.update(common_map)
      result.append(stroke_map)
    return result

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

  def _ConvertStyleCommand(self, style, output):
    """Converts style attribute."""
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

  def _MaybeConvertFontSize(self, font_size, output):
    if font_size is None:
      return
    output.WriteByte(CMD_PICTURE_PAINT_FONT_SIZE)
    output.WriteFloat(font_size)
    return

  def _MaybeConvertTextAnchor(self, text_anchor, output):
    if text_anchor is None:
      return
    output.WriteByte(CMD_PICTURE_PAINT_TEXT_ANCHOR)
    if text_anchor == 'start':
      output.WriteByte(CMD_PICTURE_PAINT_TEXT_ANCHOR_START)
    elif text_anchor == 'middle':
      output.WriteByte(CMD_PICTURE_PAINT_TEXT_ANCHOR_MIDDLE)
    elif text_anchor == 'end':
      output.WriteByte(CMD_PICTURE_PAINT_TEXT_ANCHOR_END)
    else:
      logging.critical('text-anchor is invalid (%s)', text_anchor)
      sys.exit(1)
    return

  def _MaybeConvertDominantBaseline(self, baseline, output):
    if baseline is None:
      return
    output.WriteByte(CMD_PICTURE_PAINT_DOMINANT_BASELINE)
    if baseline == 'auto':
      output.WriteByte(CMD_PICTURE_PAINT_DOMINANTE_BASELINE_AUTO)
    elif baseline == 'central':
      output.WriteByte(CMD_PICTURE_PAINT_DOMINANTE_BASELINE_CENTRAL)
    else:
      logging.critical('dominant-baseline is invalid (%s)', baseline)
      sys.exit(1)
    return

  def _MaybeConvertFontWeight(self, weight, output):
    if weight is None:
      return
    output.WriteByte(CMD_PICTURE_PAINT_FONT_WEIGHT)
    if weight == 'normal':
      output.WriteByte(CMD_PICTURE_PAINT_FONT_WEIGHT_NORMAL)
    elif weight == 'bold':
      output.WriteByte(CMD_PICTURE_PAINT_FONT_WEIGHT_BOLD)
    else:
      logging.critical('font-weight is invalid (%s)', weight)
      sys.exit(1)
    return

  def _MaybeConvertStyleCategory(self, style_category, output):
    if style_category is None:
      return
    for id_prefix, category in STYLE_CATEGORY_MAP:
      if style_category.startswith(id_prefix):
        output.WriteByte(STYLE_CATEGORY_TAG + category)
        return
    logging.critical('unknown style_category: "%s"', style_category)
    sys.exit(1)

  def _ConvertStyle(self, style_category, style_list, output):
    style_len = len(style_list)
    output.WriteByte(style_len)
    for style_map in style_list:
      self._ConvertStyleCommand(style_map['style'], output)
      self._MaybeConvertColor(style_map.get('color'), output)
      self._MaybeConvertShader(style_map.get('shader'), output)
      self._MaybeConvertShadow(style_map['shadow'], output)
      self._MaybeConvertStrokeWidth(style_map.get('stroke-width'), output)
      self._MaybeConvertStrokeLinecap(style_map.get('stroke-linecap'), output)
      self._MaybeConvertStrokeLinejoin(style_map.get('stroke-linejoin'), output)
      self._MaybeConvertFontSize(style_map.get('font-size'), output)
      self._MaybeConvertTextAnchor(style_map.get('text-anchor'), output)
      self._MaybeConvertDominantBaseline(style_map.get('dominant-baseline'),
                                         output)
      self._MaybeConvertFontWeight(style_map.get('font-weight'), output)
      self._MaybeConvertStyleCategory(style_category, output)
      output.WriteByte(CMD_PICTURE_PAINT_EOP)

  def _ConvertPath(self, node, output):
    path = node.get('d')
    if path is None:
      logging.critical('Unknown path')
      sys.exit(1)

    prev_control = None
    prev = None
    command_list = []
    prev_command = None
    while True:
      path = path.strip()
      if not path:
        break

      if path[0] in 'mMcCsShHvVlLzZ':
        # If the head character of path is one of the command prefix
        # characters, use the character as the command and consume it.
        command = path[0]
        path = path[1:]
      else:
        # If not, use prev_command as command
        # in order to support continuous commands.
        command = prev_command

      if command == 'm' or command == 'M':
        # Move command.
        (x, y), path = self._ConsumeFloatList(path, 2)
        if command == 'm' and prev is not None:
          x += prev[0]
          y += prev[1]
        command_list.append((CMD_PICTURE_PATH_MOVE, x, y))
        prev = (x, y)
        prev_control = None
        start = (x, y)
        # Subsequent coordinates are treated as implicit
        # lineto (l or L) commands.
        prev_command = 'l' if command == 'm' else 'L'
        continue

      if command == 'c' or command == 'C':
        # Cubic curve.
        (x1, y1, x2, y2, x, y), path = self._ConsumeFloatList(path, 6)
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
        prev_command = command
        continue

      if command == 's' or command == 'S':
        # Continued cubic curve.
        (x2, y2, x, y), path = self._ConsumeFloatList(path, 4)
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
        prev_command = command
        continue

      if command == 'h' or command == 'H':
        # Horizontal line.
        x, path = self._ConsumeFloat(path)
        if command == 'h':
          x += prev[0]
        y = prev[1]
        command_list.append((CMD_PICTURE_PATH_HORIZONTAL_LINE, x))
        prev = (x, y)
        prev_control = None
        prev_command = command
        continue

      if command == 'v' or command == 'V':
        # Vertical line.
        y, path = self._ConsumeFloat(path)
        if command == 'v':
          y += prev[1]
        x = prev[0]
        command_list.append((CMD_PICTURE_PATH_VERTICAL_LINE, y))
        prev = (x, y)
        prev_control = None
        prev_command = command
        continue

      if command == 'l' or command == 'L':
        # Line.
        (x, y), path = self._ConsumeFloatList(path, 2)
        if command == 'l':
          x += prev[0]
          y += prev[1]
        command_list.append((CMD_PICTURE_PATH_LINE, x, y))
        prev = (x, y)
        prev_control = None
        prev_command = command
        continue

      if command == 'z' or command == 'Z':
        # Close the path.
        command_list.append((CMD_PICTURE_PATH_CLOSE,))
        path = path
        prev = start
        prev_control = None
        prev_command = command
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
    self._ConvertStyle(style_category, style_list, output)

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
    self._ConvertStyle(style_category, style_list, output)

  def _ConvertPolygonElement(
      self, node, style_category, has_shadow, shader_map, output):
    style_list = self._ParseStyle(node, has_shadow, shader_map)
    point_list = self._ParseFloatList(node.get('points'))

    output.WriteByte(CMD_PICTURE_DRAW_POLYGON)
    output.WriteByte(len(point_list))
    for coord in point_list:
      output.WriteFloat(coord)
    self._ConvertStyle(style_category, style_list, output)

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
    self._ConvertStyle(style_category, style_list, output)

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
    self._ConvertStyle(style_category, style_list, output)

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
    self._ConvertStyle(style_category, style_list, output)

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
    self._ConvertStyle(style_category, style_list, output)

  def _ConvertTextElement(
      self, node, style_category, has_shadow, shader_map, output):
    """Converts text element.

    The text element must not have any children.

    Args:
      node: node
      style_category: style_category(optional)
      has_shadow: shadow value
      shader_map: shader map
      output: output stream
    """
    if not node.text:
      # Ignore empty text node
      return

    style_list = self._ParseStyle(node, has_shadow, shader_map)
    x = float(node.get('x', 0))
    y = float(node.get('y', 0))
    output.WriteByte(CMD_PICTURE_DRAW_TEXT)
    output.WriteFloat(x)
    output.WriteFloat(y)
    if node:
      logging.critical('<text> with children is not supported.')
      sys.exit(1)
    output.WriteString(node.text)
    self._ConvertStyle(style_category, style_list, output)

  def _ConvertGroupElement(
      self, node, style_category, has_shadow, shader_map, output):
    transform = node.get('transform')
    if transform:
      # Output order of 3x3 matrix;
      # [ 0 3 6
      #   1 4 7
      #   2 5 8 ]
      # - Combined transformation is not supported yet.
      # - Only 'matrix' and 'translate' transformation is supported.
      transform_match = TRANSFORM_PATTERN.match(transform)
      if transform_match is None:
        logging.critical('Unsupported transform: %s', transform)
        sys.exit(1)
      transformation = transform_match.group(1)
      coords = transform_match.group(2)

      output.WriteByte(CMD_PICTURE_DRAW_GROUP_START)

      if transformation == 'matrix':
        (m11, m21, m12, m22, m13, m23) = tuple(self._ParseFloatList(coords))
        output.WriteFloat(m11)
        output.WriteFloat(m21)
        output.WriteFloat(0)
        output.WriteFloat(m12)
        output.WriteFloat(m22)
        output.WriteFloat(0)
        output.WriteFloat(m13)
        output.WriteFloat(m23)
        output.WriteFloat(1)
      elif transformation == 'translate':
        (tx, ty) = tuple(self._ParseFloatList(coords))
        output.WriteFloat(1)
        output.WriteFloat(0)
        output.WriteFloat(0)
        output.WriteFloat(0)
        output.WriteFloat(1)
        output.WriteFloat(0)
        output.WriteFloat(tx)
        output.WriteFloat(ty)
        output.WriteFloat(1)
      elif transformation == 'scale':
        parsed_coords = tuple(self._ParseFloatList(coords))
        if len(parsed_coords) != 1 and len(parsed_coords) != 2:
          logging.critical('Invalid argument for scale: %s', coords)
          sys.exit(1)
        sx = parsed_coords[0]
        if len(parsed_coords) == 1:
          sy = sx
        else:
          sy = parsed_coords[1]
        output.WriteFloat(sx)
        output.WriteFloat(0)
        output.WriteFloat(0)
        output.WriteFloat(0)
        output.WriteFloat(sy)
        output.WriteFloat(0)
        output.WriteFloat(0)
        output.WriteFloat(0)
        output.WriteFloat(1)
      else:
        # Never reach here. Just in case.
        logging.critical('Unsupported transform: %s', transform)
        sys.exit(1)

    for child in node:
      self._ConvertPictureSequence(
          child, style_category, has_shadow, shader_map, output)

    if transform:
      output.WriteByte(CMD_PICTURE_DRAW_GROUP_END)

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

    if node.tag == '{http://www.w3.org/2000/svg}text':
      self._ConvertTextElement(
          node, style_category, has_shadow, shader_map, output)
      return

    if node.tag in ['{http://www.w3.org/2000/svg}g',
                    '{http://www.w3.org/2000/svg}svg']:
      self._ConvertGroupElement(
          node, style_category, has_shadow, shader_map, output)
      return

    # Ignore following tags.
    if node.tag in ['{http://www.w3.org/2000/svg}linearGradient',
                    '{http://www.w3.org/2000/svg}radialGradient',
                    '{http://www.w3.org/2000/svg}metadata',
                    '{http://www.w3.org/2000/svg}defs',]:
      return

    logging.critical('Unknown element: %s', node.tag)
    sys.exit(1)

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
    output = _OutputStream(io.BytesIO())
    self._ConvertPictureDrawableInternal(ElementTree.parse(path), output)
    return output.output.getvalue()

  def ConvertStateListDrawable(self, drawable_source_list):
    output = _OutputStream(io.BytesIO())
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


def ConvertFiles(svg_paths, output_dir):
  """Converts SVG files into MechaMozc specific *pic* files.

  Args:
    svg_paths: Comma separated paths to a directory/zip which has svg files
               (recursively).
    output_dir: Path of the destination directory.
  """
  logging.debug('Start SVG conversion. From:%s, To:%s', svg_paths, output_dir)
  # Ensure that the output directory exists.
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)

  converter = MozcDrawableConverter()
  number_of_conversion = 0
  for dirpath, _, filenames in util.WalkFileContainers(svg_paths):
    for filename in filenames:
      basename, ext = os.path.splitext(filename)
      if ext != '.svg':
        # Do nothing for files other than svg.
        continue

      # Filename hack to generate stateful .pic files.
      if basename.endswith('_release'):
        # 'XXX_release.svg' files will be processed with corresponding
        # '_center.svg' files. So just skip them.
        continue

      logging.debug('Converting %s...', filename)

      if basename.endswith('_center'):
        # Process '_center.svg' file with '_release.svg' file to make
        # stateful drawable.
        center_svg_file = os.path.join(dirpath, filename)
        release_svg_file = os.path.join(
            dirpath, basename[:-7] + '_release.svg')
        pic_file = os.path.join(output_dir, basename + '.pic')
        pic_data = converter.ConvertStateListDrawable(
            [([STATE_PRESSED], center_svg_file), ([], release_svg_file)])
      else:
        # Normal .svg file.
        svg_file = os.path.join(dirpath, filename)
        pic_file = os.path.join(output_dir, basename + '.pic')
        pic_data = converter.ConvertPictureDrawable(svg_file)

      with open(pic_file, 'wb') as stream:
        stream.write(pic_data)
      number_of_conversion += 1
  logging.debug('%d files are converted.', number_of_conversion)


def ParseOptions():
  """Parses options."""
  parser = optparse.OptionParser()
  parser.add_option('--svg_paths', dest='svg_paths',
                    help='Comma separated paths to a directory or'
                    ' a .zip file containing .svg files.')
  parser.add_option('--output_dir', dest='output_dir',
                    help='Path to the output directory,')
  parser.add_option('--build_log', dest='build_log',
                    help='(Optional) Path to build log to generate.'
                    ' If set, nothing will be sent to stderr.')
  parser.add_option('--verbose', '-v', dest='verbose',
                    action='store_true', default=False,
                    help='Shows verbose message.')
  return parser.parse_args()[0]


def main():
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
  logging.getLogger().addFilter(util.ColoredLoggingFilter())

  options = ParseOptions()
  if options.build_log:
    logging.getLogger().handlers = []
    logging.getLogger().addHandler(logging.FileHandler(options.build_log, 'w'))
  if options.verbose or options.build_log:
    logging.getLogger().setLevel(logging.DEBUG)
  ConvertFiles(options.svg_paths, options.output_dir)


if __name__ == '__main__':
  main()
