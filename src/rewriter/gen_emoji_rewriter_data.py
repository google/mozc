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
  Each line in the input file has 7 columns separated by TAB characters,
  and this generator requires the 2nd-7th columns.
  We expect that each column has
  # 1) unicode code point
  # 2) actual data (in utf-8)
  # 3) space separated readings
  # 4) unicode name
  # 5) Japanese name
  # 6) space separated descriptions (for emoji suggestion)
  # 7) unicode emoji version

  Detailed format is described in the default input file
  "mozc/src/data/emoji/emoji_data.tsv".
"""

import argparse
import codecs
import collections
import logging
import re
import struct
import sys

from build_tools import code_generator_util
from build_tools import serialized_string_array_builder


_FULLWIDTH_RE = re.compile('[！-～]')  # U+FF01 - U+FF5E
# LINT.IfChange
_VERSION_ALL = [
    'E0.6',
    'E0.7',
    'E1.0',
    'E2.0',
    'E3.0',
    'E4.0',
    'E5.0',
    'E11.0',
    'E12.0',
    'E12.1',
    'E13.0',
    'E13.1',
    'E14.0',
    'E15.0',
    'E15.1',
    'E16.0',
    'E17.0',
]
# Dict for converting version string into enum index.
VERSION_TO_INDEX = {v: i for i, v in enumerate(_VERSION_ALL)}
# EmojiVersion enum must be fixed accordingly.
# LINT.ThenChange(
# //data_manager/emoji_data.h,
# //protocol/commands.proto,
# //rewriter/environmental_filter_rewriter.h
# )


def NormalizeString(string):
  """Normalize full width ascii characters to half width characters."""
  offset = ord('Ａ') - ord('A')
  normalized = _FULLWIDTH_RE.sub(
      lambda x: chr(ord(x.group(0)) - offset), string
  )
  return normalized


def ReadEmojiTsv(stream):
  """Parses emoji_data.tsv file and builds the emoji_data_list and reading map.

  Args:
    stream: input stream of emoji_data.tsv

  Returns:
    tuple of emoji_data_list and token_dict.
  """
  emoji_data_list = []
  token_dict = collections.defaultdict(list)

  stream = code_generator_util.SkipLineComment(stream)
  for columns in code_generator_util.ParseColumnStream(stream, delimiter='\t'):
    if len(columns) != 7:
      logging.critical('format error: %s', '\t'.join(columns))
      sys.exit(1)

    # [0]: Emoji code point.
    emoji = columns[1] if columns[1] else ''
    if not emoji:
      logging.info('Skip: %s', '\t'.join(columns))
      continue

    readings = columns[2]

    # [3]: Name defined in Unicode.  It is ignored in current implementation.
    utf8_description = columns[4] if columns[4] else ''
    # [5]: Descriptions. It is ignored in current implementation.
    unicode_version = columns[6] if columns[6] else ''

    index = len(emoji_data_list)
    emoji_data_list.append((emoji, utf8_description, unicode_version))

    # \xe3\x80\x80 is a full-width space
    for reading in re.split(r'(?: |\xe3\x80\x80)+', readings.strip()):
      if reading:
        token_dict[NormalizeString(reading)].append(index)

  return (emoji_data_list, token_dict)


def OutputData(
    emoji_data_list, token_dict, token_array_file, string_array_file
):
  """Output token and string arrays to files."""
  sorted_token_dict = sorted(token_dict.items())

  strings = {}
  strings[''] = 0
  for reading, _ in sorted_token_dict:
    strings[reading] = 0
  for emoji, utf8_description, _ in emoji_data_list:
    strings[emoji] = 0
    strings[utf8_description] = 0

  sorted_strings = sorted(strings.keys())
  for index, s in enumerate(sorted_strings):
    strings[s] = index

  with open(token_array_file, 'wb') as f:
    for reading, value_list in sorted_token_dict:
      reading_index = strings[reading]
      for value_index in value_list:
        (emoji, utf8_description, unicode_version) = emoji_data_list[
            value_index
        ]
        # Here, f.write should be called exactly 7 times, in order to preserve
        # data format.
        f.write(struct.pack('<I', reading_index))
        f.write(struct.pack('<I', strings[emoji]))
        f.write(struct.pack('<I', VERSION_TO_INDEX[unicode_version]))
        f.write(struct.pack('<I', strings[utf8_description]))
        f.write(struct.pack('<I', strings['']))
        f.write(struct.pack('<I', strings['']))
        f.write(struct.pack('<I', strings['']))

  serialized_string_array_builder.SerializeToFile(
      sorted_strings, string_array_file
  )


def ParseOptions() -> argparse.Namespace:
  """Parse given options.

  Returns:
    parsed options.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', dest='input', help='emoji data file')
  parser.add_argument(
      '--output_token_array',
      dest='output_token_array',
      help='output token array file',
  )
  parser.add_argument(
      '--output_string_array',
      dest='output_string_array',
      help='output string array file',
  )
  return parser.parse_args()


def main():
  options = ParseOptions()
  with codecs.open(options.input, 'r', encoding='utf-8') as input_stream:
    (emoji_data_list, token_dict) = ReadEmojiTsv(input_stream)

  OutputData(
      emoji_data_list,
      token_dict,
      options.output_token_array,
      options.output_string_array,
  )


if __name__ == '__main__':
  main()
