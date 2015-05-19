# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
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

import itertools
import optparse
import re
import string
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

  def __init__(self, cp932file, jisx0201file, jisx0208file,
               jisx0212file, jisx0213file):
    self._cp932 = CodePointCategorizer._LoadCP932(cp932file)
    self._jisx0201 = CodePointCategorizer._LoadJISX0201(jisx0201file)
    self._jisx0208 = CodePointCategorizer._LoadJISX0208(jisx0208file)
    self._jisx0212 = CodePointCategorizer._LoadJISX0212(jisx0212file)
    self._jisx0213 = CodePointCategorizer._LoadJISX0213(jisx0213file)
    self._exceptions = CodePointCategorizer._LoadExceptions()

    # Make a list of code point tables in the priority order.
    self._table_list = [
        ('JISX0208', self._exceptions),  # Vender specific code.
        ('JISX0201', self._jisx0201),
        ('JISX0208', self._jisx0208),
        ('JISX0212', self._jisx0212),
        ('JISX0213', self._jisx0213),
        ('CP932', self._cp932)]


  @staticmethod
  def _LoadTable(filename, column_index, pattern, validater):
    result = set()
    for line in open(filename):
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
  def _LoadCP932(filename):
    return CodePointCategorizer._LoadTable(
        filename, 1, CodePointCategorizer._UCS2_PATTERN, IsValidUCS2)

  @staticmethod
  def _LoadJISX0201(filename):
    return CodePointCategorizer._LoadTable(
        filename, 1, CodePointCategorizer._UCS2_PATTERN, IsValidUCS2)

  @staticmethod
  def _LoadJISX0208(filename):
    result = CodePointCategorizer._LoadTable(
        filename, 2, CodePointCategorizer._UCS2_PATTERN, IsValidUCS2)
    result.update([
        0xFF3C, # (FULLWIDTH REVERSE SOLIDS) should be in JISX0208
        0xFF0D, # (FULLWIDTH HYPHEN MINUS) should be in JISX0208
        ])
    return result

  @staticmethod
  def _LoadJISX0212(filename):
    return CodePointCategorizer._LoadTable(
        filename, 1, CodePointCategorizer._UCS2_PATTERN, IsValidUCS2)

  @staticmethod
  def _LoadJISX0213(filename):
    return CodePointCategorizer._LoadTable(
        filename, 1, CodePointCategorizer._UCS4_PATTERN, IsValidUCS4)

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
        0x2225,  # PARALEL TO
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


def FindCodePoint(category_list, category_name):
  """Returns a list of code points which belong to the given category_name."""
  return [codepoint for codepoint, category in enumerate(category_list)
          if category == category_name]


def ParseOptions():
  """Parses command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--cp932file', dest='cp932file',
                    help='File path for the unicode\'s CP932.TXT file')
  parser.add_option('--jisx0201file', dest='jisx0201file',
                    help='File path for the unicode\'s JIS0201.TXT file')
  parser.add_option('--jisx0208file', dest='jisx0208file',
                    help='File path for the unicode\'s JIS0208.TXT file')
  parser.add_option('--jisx0212file', dest='jisx0212file',
                    help='File path for the unicode\'s JIS0212.TXT file')
  parser.add_option('--jisx0213file', dest='jisx0213file',
                    help='File path for the unicode\'s jisx0213-2004-std.txt '
                    'file')
  parser.add_option('--output', dest='output',
                    help='output file path. If not specified, '
                    'output to stdout.')

  return parser.parse_args()[0]


def GenerateCategoryBitmap(category_list, name):
  r"""Generats bitmap data code.

  The generated data looks something like:
  namespace {
  const char name[] =
      "\xXX\xXX\xXX\xXX...\xXX"
      "\xXX\xXX\xXX\xXX...\xXX"
      "\xXX\xXX\xXX\xXX...\xXX"
      "\xXX\xXX\xXX\xXX...\xXX"
  ;
  }  // namespace

  Args:
    category_list: a list of categories.
    name: a bitmap name.
  """
  lines = []

  # Create bitmap list.
  # Encode each code point category to 2-bits.
  # The result should be a byte (8-bits), so group each consecutive
  # (at most) four code points.
  bit_list = []
  for _, group in itertools.groupby(enumerate(category_list),
                                    lambda (codepoint, _): codepoint / 4):
    # Fill bits from LSB to MSB for each group.
    bits = 0
    for index, (_, category) in enumerate(group):
      bits |= CATEGORY_BITMAP.get(category, 0) << (index * 2)
    bit_list.append(bits)

  # Header.
  lines.extend(['namespace {\n',
                'const char %s[] =\n' % name])

  # Output the content. Each line would have (at most) 16 bytes.
  for _, group in itertools.groupby(enumerate(bit_list),
                                    lambda (index, _): index / 16):
    line = ['    \"']
    for _, bits in group:
      line.append('\\x%02X' % bits)
    line.append('\"\n')
    lines.append(''.join(line))

  lines.extend([';\n',
                '}  // namespace\n'])

  return lines


def GenerateIfStatement(codepoint_list, var_name, num_indent, return_type):
  """Generates a if-case statement for given arguments.

  This method generates a if-case statement, which checks if the value of
  the given 'var_name' variable is in 'codepoint_list'
  and returns 'return_type' if contained. The condition expression would be
  a simple range-based linear check.

  The generated code would be something like:

  if (var_name == 0xXXXX ||    // for a single element list.
      (0xXXXX <= var_name && var_name <= 0xXXXX) ||   // for range check.
         :
      (0xXXXX <= var_name && var_name <= 0xXXXX)) {
    return RETURN_TYPE;
  }

  Args:
    codepoint_list: a sorted list of code points.
    var_name: a variable name to be checked.
    num_indent: the indent depth.
    return_type: a return category type which should be returned if
      'var_name' is in the 'codepoint_list'
  Returns: a list of lines of the generated switch-case statement.
  """
  conditions = []
  for codepoint_group in GroupConsecutiveCodepoints(codepoint_list):
    if len(codepoint_group) == 1:
      conditions.append('%s == %d' % (var_name, codepoint_group[0]))
    else:
      conditions.append(
          '(%d <= %s && %s <= %d)' % (codepoint_group[0], var_name,
                                      var_name, codepoint_group[-1]))

  condition = (' ||\n' + ' ' * (num_indent + 4)).join(conditions)
  lines = [''.join([' ' * num_indent, 'if (', condition, ') {\n']),
           ' ' * (num_indent + 2) + 'return %s;\n' % return_type,
           ' ' * num_indent + '}\n']
  return lines


def GenerateSwitchStatement(codepoint_list, var_name, num_indent, return_type):
  """Generates a switch-case statement for given arguments.

  This method generates a switch-case statement, which checks if the value of
  the given 'var_name' variable is in 'codepoint_list'
  and returns 'return_type' if contained.

  The generated code would be something like:
  switch (var_name) {
    case 0xXXXX:
    case 0xXXXX:
      :
    case 0xXXXX:
      return RETURN_TYPE;
  }

  Args:
    codepoint_list: a sorted list of code points.
    var_name: a variable name to be checked.
    num_indent: the indent depth.
    return_type: a return category type which should be returned if
      'var_name' is in the 'codepoint_list'
  Returns: a list of lines of the generated switch-case statement.
  """
  lines = [' ' * num_indent + ('switch (%s) {\n' % var_name)]
  for codepoint in codepoint_list:
    lines.append(' ' * (num_indent + 2) + 'case 0x%08X:\n' % codepoint)
  lines.extend([' ' * (num_indent + 4) + ('return %s;\n' % return_type),
                ' ' * num_indent + '}\n'])
  return lines


def GenerateGetCharacterSet(category_list, bitmap_name, bitmap_size):
  """Generates function body of a Util::GetCharacterSet method."""
  lines = []

  # Function header.
  lines.append('Util::CharacterSet Util::GetCharacterSet(char32 ucs4) {\n')

  # First, check if the given code is valid or not. If not, returns
  # UNICODE_ONLY as a fallback.
  # TODO(komatsu): add INVALID instead of UNICODE_ONLY.
  lines.extend(['  if (ucs4 > 0x10FFFF) {\n',
                '    return UNICODE_ONLY;\n',
                '  }\n'])
  lines.append('\n')

  # Check if the argument is ASCII or not.
  lines.extend(['  if (ucs4 <= 0x7F) {\n',
                '    return ASCII;\n',
                '  }\n'])
  lines.append('\n')

  # Check if the argument is JIS0201.
  # We check this by rangebased if statement, because almost JISX0201 code
  # points are consecutive.
  lines.extend(GenerateIfStatement(
      FindCodePoint(category_list, 'JISX0201'), 'ucs4', 2, 'JISX0201'))
  lines.append('\n')

  # Check if the argument is CP932.
  # Check by a switch-case statement as CP932 code points are discrete.
  lines.extend(GenerateSwitchStatement(
      FindCodePoint(category_list, 'CP932'), 'ucs4', 2, 'CP932'))
  lines.append('\n')

  # Bitmap lookup.
  # TODO(hidehiko): the bitmap has two huge 0-bits ranges. Reduce them.
  category_map = [
      (bits, category) for category, bits in CATEGORY_BITMAP.iteritems()]
  category_map.sort()

  lines.extend([
      '  if (ucs4 < %d) {\n' % bitmap_size,
      '    switch ((kCategoryBitmap[ucs4 >> 2] >> ((ucs4 & 3) * 2)) & 3) {\n'])
  lines.extend(('      case %d: return %s;\n' % item) for item in category_map)
  lines.extend(['    }\n',
                '    return UNICODE_ONLY;\n',
                '  }\n',
                '\n'])

  # For codepoint > bitmap_size.
  # Remaining category should be only JISX0213 or UNICODE_ONLY.
  # The number of JISX0213 code points are much small, so we just check it
  # by a switch-case statement.
  lines.extend(GenerateSwitchStatement(
      [codepoint for codepoint in FindCodePoint(category_list, 'JISX0213')
       if codepoint >= bitmap_size ],
      'ucs4', 2, 'JISX0213'))

  # Returns UNICODE_ONLY as a last resort.
  lines.extend([
      '  return UNICODE_ONLY;\n',
      '}\n'])

  return lines


def GenerateCharacterSetHeader(category_list):
  """Generates lines of character_set.h file."""
  bitmap_name = "kCategoryBitmap"
  bitmap_size = 65536

  lines = []

  # File header comments.
  lines.extend(['// This file is generated by base/gen_character_set.py\n',
                '// Do not edit me!\n',
                '\n'])

  # We use 2-bits bitmap to check JISX0208, JISX0212 and JISX0213, for
  # code point 0 to 65535 (inclusive).
  lines.extend(
      GenerateCategoryBitmap(category_list[:bitmap_size], bitmap_name))
  lines.append('\n')

  # Then add Util::GetCharacterSet method.
  lines.extend(
      GenerateGetCharacterSet(category_list, bitmap_name, bitmap_size))

  return lines


def main():
  options = ParseOptions()

  # Generates lines of the header file.
  categorizer = CodePointCategorizer(options.cp932file,
                                     options.jisx0201file,
                                     options.jisx0208file,
                                     options.jisx0212file,
                                     options.jisx0213file)
  category_list = [
      categorizer.GetCategory(codepoint)
      for codepoint in xrange(categorizer.MaxCodePoint() + 1)]
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


if __name__ == "__main__":
  main()
