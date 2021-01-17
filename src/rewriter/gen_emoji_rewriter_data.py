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

"""Converter of emoji data from TSV file to C++ header file.

Usage:
  python gen_emoji_rewriter_data.py --input=input.tsv --output=output.h

File format:
  Each line in the input file has 13 columns separated by TAB characters,
  and this generator requires the 2nd, 3rd, 7th, and 9th-12th columns.
  We expect that each column has
  2nd) an emoji character, in binary format
  3rd) Android PUA code in hexadecimal
  7th) readings separated by commans
  9th) description for the Unicode 6.0 emoji
  10-12th) descriptions for each carrier (docomo, softbank, and kddi)

  Detailed format is described in the default input file
  "mozc/data/emoji/emoji_data.tsv".
"""

from __future__ import absolute_import
from __future__ import print_function

import codecs
import collections
import logging
import optparse
import re
import struct
import sys

import six

from build_tools import code_generator_util
from build_tools import serialized_string_array_builder


def ParseCodePoint(s):
  """Parses the pua string representation.

  The format of the input is either:
  - empty string
  - hexadecimal integer.
  - hexadecimal integer leading '>'.

  We're not interested in empty string nor '>'-leading codes, so returns None
  for them.
  Note that '>'-leading code means it is "secondary" code point to represent
  the glyph (in other words, it has alternative (primary) code point, which
  doesn't lead '>' and that's why we'll ignore it).
  """
  if not s or s[0] == '>':
    return None
  return int(s, 16)


_FULLWIDTH_RE = re.compile(u'[！-～]')   # U+FF01 - U+FF5E


def NormalizeString(string):
  """Normalize full width ascii characters to half width characters."""
  offset = ord(u'Ａ') - ord(u'A')
  normalized = _FULLWIDTH_RE.sub(lambda x: six.unichr(ord(x.group(0)) - offset),
                                 six.ensure_text(string))
  return normalized


def ReadEmojiTsv(stream):
  """Parses emoji_data.tsv file and builds the emoji_data_list and reading map.
  """
  emoji_data_list = []
  token_dict = collections.defaultdict(list)

  stream = code_generator_util.SkipLineComment(stream)
  for columns in code_generator_util.ParseColumnStream(stream, delimiter='\t'):
    if len(columns) != 13:
      logging.critical('format error: %s', '\t'.join(columns))
      sys.exit(1)

    # Emoji code point.
    emoji = columns[1] if columns[1] else ''
    android_pua = ParseCodePoint(columns[2])
    docomo_pua = ParseCodePoint(columns[3])
    softbank_pua = ParseCodePoint(columns[4])
    kddi_pua = ParseCodePoint(columns[5])

    readings = columns[6]

    # [7]: Name defined in Unicode.  It is ignored in current implementation.
    utf8_description = columns[8] if columns[8] else ''
    docomo_description = columns[9] if columns[9] else ''
    softbank_description = columns[10] if columns[10] else ''
    kddi_description = columns[11] if columns[11] else ''

    # Check consistency between carrier PUA codes and descriptions for Android
    # just in case.
    if ((bool(docomo_pua) != bool(docomo_description)) or
        (bool(softbank_pua) != bool(softbank_description)) or
        (bool(kddi_pua) != bool(kddi_description))):
      logging.warning('carrier PUA and description conflict: %s',
                      '\t'.join(columns))
      continue

    # Check if the character is usable on Android.
    if not android_pua or not (docomo_pua or softbank_pua or kddi_pua):
      android_pua = 0  # Replace None with 0.

    if not emoji and not android_pua:
      logging.info('Skip: %s', '\t'.join(columns))
      continue

    index = len(emoji_data_list)
    emoji_data_list.append((emoji, android_pua, utf8_description,
                            docomo_description, softbank_description,
                            kddi_description))

    # \xe3\x80\x80 is a full-width space
    for reading in re.split(r'(?: |\xe3\x80\x80)+', readings.strip()):
      if reading:
        token_dict[NormalizeString(reading)].append(index)

  return (emoji_data_list, token_dict)


def OutputData(emoji_data_list, token_dict,
               token_array_file, string_array_file):
  """Output token and string arrays to files."""
  sorted_token_dict = sorted(six.iteritems(token_dict))

  strings = {}
  for reading, _ in sorted_token_dict:
    strings[reading] = 0
  for (emoji, android_pua, utf8_description, docomo_description,
       softbank_description, kddi_description) in emoji_data_list:
    strings[emoji] = 0
    strings[utf8_description] = 0
    strings[docomo_description] = 0
    strings[softbank_description] = 0
    strings[kddi_description] = 0
  sorted_strings = sorted(strings.keys())
  for index, s in enumerate(sorted_strings):
    strings[s] = index

  with open(token_array_file, 'wb') as f:
    for reading, value_list in sorted_token_dict:
      reading_index = strings[reading]
      for value_index in value_list:
        (emoji, android_pua, utf8_description, docomo_description,
         softbank_description, kddi_description) = emoji_data_list[value_index]
        f.write(struct.pack('<I', reading_index))
        f.write(struct.pack('<I', strings[emoji]))
        f.write(struct.pack('<I', android_pua))
        f.write(struct.pack('<I', strings[utf8_description]))
        f.write(struct.pack('<I', strings[docomo_description]))
        f.write(struct.pack('<I', strings[softbank_description]))
        f.write(struct.pack('<I', strings[kddi_description]))

  serialized_string_array_builder.SerializeToFile(sorted_strings,
                                                  string_array_file)


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='emoji data file')
  parser.add_option('--output_token_array', dest='output_token_array',
                    help='output token array file')
  parser.add_option('--output_string_array', dest='output_string_array',
                    help='output string array file')
  return parser.parse_args()[0]


def main():
  options = ParseOptions()
  with codecs.open(options.input, 'r', encoding='utf-8') as input_stream:
    (emoji_data_list, token_dict) = ReadEmojiTsv(input_stream)

  OutputData(emoji_data_list, token_dict,
             options.output_token_array, options.output_string_array)


if __name__ == '__main__':
  main()
