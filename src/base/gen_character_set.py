# -*- coding: utf-8 -*-
# Copyright 2010-2021, Google Inc.
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

__author__ = "taku"

import codecs
import itertools
import optparse
import re
import sys


# We use 2-bits bitmap data for JISX0208, JISX0212 and JISX0213 clustering.
# For remaining categories (i.e. ASCII, CP932, JISX0201 and UNICODE_ONLY),
# we introduce heuristics to check them and use '00' bits for all of them.
CATEGORY_BITMAP = {
    'JISX0208': 1,
    'JISX0212': 2,
    'JISX0213': 3,
}


def IsValidUCS2(n):
  """Returns True if the n is valid code in UCS2."""
  return 0 <= n <= 0xFFFF


def IsValidUCS4(n):
  """Returns True if the n is valid code in UCS4."""
  return 0 <= n <= 0x7FFFFFFF


class CodePointCategorizer(object):
  """Categorizer of ucs4 code points."""

  _UCS2_PATTERN = re.compile(r'^0x([0-9A-F]{4})')

  # UCS4 pattern supports only JIS X 0213.
  # Note that Some JIS X 0213 characters are described as 'U+xxxx+xxxx',
  # and this pattern ignores latter +xxxx part intentionally.
  _UCS4_PATTERN = re.compile(r'^U\+([0-9A-F]+)')

  def __init__(self, jisx0201file, jisx0208file):
    self._jisx0201 = CodePointCategorizer._LoadJISX0201(jisx0201file)
    self._jisx0208 = CodePointCategorizer._LoadJISX0208(jisx0208file)
    self._exceptions = CodePointCategorizer._LoadExceptions()

    # Make a list of code point tables in the priority order.
    self._table_list = [
        ('JISX0208', self._exceptions),  # Vender specific code.
        ('JISX0201', self._jisx0201),
        ('JISX0208', self._jisx0208),
    ]

  @staticmethod
  def _LoadTable(filename, column_index, pattern, validater):
    result = set()
    for line in codecs.open(filename, encoding='utf-8'):
      if line.startswith('#'):
        # Skip a comment line.
        continue

      columns = line.split()
      match = pattern.search(columns[column_index])
      if match:
        ucs = int(match.group(1), 16)
        if validater(ucs):
          result.add(ucs)

    return result

  @staticmethod
  def _LoadJISX0201(filename):
    return CodePointCategorizer._LoadTable(
        filename, 1, CodePointCategorizer._UCS2_PATTERN, IsValidUCS2)

  @staticmethod
  def _LoadJISX0208(filename):
    result = CodePointCategorizer._LoadTable(
        filename, 2, CodePointCategorizer._UCS2_PATTERN, IsValidUCS2)
    result.update([
        0xFF3C,  # (FULLWIDTH REVERSE SOLIDS) should be in JISX0208
        0xFF0D,  # (FULLWIDTH HYPHEN MINUS) should be in JISX0208
        ])
    return result

  # The following chars have different mapping in
  # Windows and Mac. Technically, they are platform dependent
  # characters, but Mozc treat them so that they are normal characters
  # defined in JISX0208
  @staticmethod
  def _LoadExceptions():
    # treat Unicode Japanese incompatible characters as JISX0208.
    return set([
        0x00A5,  # YEN SIGN
        0x203E,  # OVERLINE
        0x301C,  # WAVE DASH
        0xFF5E,  # FULL WIDTH TILDE
        0x2016,  # DOUBLE VERTICAL LINE
        0x2225,  # PARALLEL TO
        0x2212,  # MINUS SIGN
        0xFF0D,  # FULL WIDTH HYPHEN MINUS
        0x00A2,  # CENT SIGN
        0xFFE0,  # FULL WIDTH CENT SIGN
        0x00A3,  # POUND SIGN
        0xFFE1,  # FULL WIDTH POUND SIGN
        0x00AC,  # NOT SIGN
        0xFFE2,  # FULL WIDTH NOT SIGN
        ])

  def GetCategory(self, codepoint):
    """Returns category name of the codepoint, or None for invalid input."""
    if not IsValidUCS4(codepoint):
      return None

    # Special handling for ascii code point.
    if codepoint <= 0x007F:
      return 'ASCII'

    # Then look for loaded table list in the order.
    for name, table in self._table_list:
      if codepoint in table:
        return name

    # Not found in any tables, so return "UNICODE_ONLY" as a fallback.
    return 'UNICODE_ONLY'

  def MaxCodePoint(self):
    """Returns the max of code points in the loaded table."""
    return max(max(table) for _, table in self._table_list)


def GroupConsecutiveCodepoints(codepoint_list):
  """Takes sorted codepoint list and groups by consecutive code points."""
  result = []

  prev = None
  current = []
  for codepoint in codepoint_list:
    if prev is not None and codepoint != prev + 1:
      result.append(current)
      current = []
    current.append(codepoint)
    prev = codepoint

  if current:
    result.append(current)

  return result


def ParseOptions():
  """Parses command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--jisx0201file', dest='jisx0201file',
                    help='File path for the unicode\'s JIS0201.TXT file')
  parser.add_option('--jisx0208file', dest='jisx0208file',
                    help='File path for the unicode\'s JIS0208.TXT file')
  parser.add_option('--output', dest='output',
                    help='output file path. If not specified, '
                    'output to stdout.')

  return parser.parse_args()[0]


def GenerateJisX0208Bitmap(category_list, name):
  r"""Generats bitmap data code.

  The generated data looks something like:
  namespace {
  const uint64_t nameIndex = 0xXXXXXXXXXXXX;
  const uint32_t name[NN][32] = {
    {  // 0000 - 03FF
      0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX,
      0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX,
      ...
      0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX,
    },
    {  // 0400 - 07FF
      0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX, 0xXXXXXXXX,
      ...
    },
  };
  }  // namespace

  Args:
    category_list: a list of categories.
    name: a bitmap name.
  """
  lines = []

  # Create bitmap list.
  # Encode each code point category to 1-bits.
  # The result should be uint32_t, so group each consecutive
  # (at most) 32 code points.
  bit_list = []
  for _, group in itertools.groupby(enumerate(category_list),
                                    lambda args: args[0] // 32):  # codepoint/32
    # Fill bits from LSB to MSB for each group.
    bits = 0
    for index, (_, category) in enumerate(group):
      bit = 1 if CATEGORY_BITMAP.get(category, 0) else 0
      bits |= bit << index
    bit_list.append(bits)

  index = 0
  data = []
  data_size = 0
  # Group by 1024 characters (= 128 bit_list units).
  for key, group1 in itertools.groupby(enumerate(bit_list),
                                       lambda args: args[0] // (1024 // 32)):
    block = ['  {  // U+%04X - U+%04X\n' % (key * 1024, (key + 1) * 1024 - 1)]
    has_value = False

    # Output the content. Each line would have (at most) 4 integers
    for _, group2 in itertools.groupby(group1,
                                       lambda args: args[0] // 4):  # index/4
      line = []
      for _, bits in group2:
        line.append('0x%08X' % bits)
        if bits > 0:
          has_value = True
      block.append('    %s,\n' % ', '.join(line))
    block.append('  },\n')

    if has_value:
      index += (1 << key)
      data_size += 1
      data.extend(block)

  lines.extend(['namespace {\n',
                'const uint64_t %sIndex = 0x%016X;\n' % (name, index),
                'const uint32_t %s[%d][32] = {\n' % (name, data_size)])
  lines.extend(data)
  lines.extend(['};\n',
                '}  // namespace\n'])

  return lines


def GenerateCharacterSetHeader(category_list):
  """Generates lines of character_set.h file."""
  bitmap_name = 'kJisX0208Bitmap'
  bitmap_size = 65536

  lines = []

  # File header comments.
  lines.extend(['// This file is generated by base/gen_character_set.py\n',
                '// Do not edit me!\n',
                '\n'])

  # We use bitmap to check JISX0208 for code point 0 to 65535.
  lines.extend(GenerateJisX0208Bitmap(category_list[:bitmap_size], bitmap_name))
  lines.append('\n')

  return lines


def main():
  options = ParseOptions()

  # Generates lines of the header file.
  categorizer = CodePointCategorizer(options.jisx0201file, options.jisx0208file)
  category_list = [
      categorizer.GetCategory(codepoint)
      for codepoint in range(categorizer.MaxCodePoint() + 1)]
  generated_character_set_header = GenerateCharacterSetHeader(category_list)

  # Write the result.
  if options.output:
    output = open(options.output, 'w')
    try:
      output.writelines(generated_character_set_header)
    finally:
      output.close()
  else:
    sys.stdout.writelines(generated_character_set_header)


if __name__ == '__main__':
  main()
