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

"""Generates zero query data file."""

import argparse
import codecs
import collections
import logging
import re
import sys
import unicodedata
from build_tools import code_generator_util
from prediction import gen_zero_query_util as util


# \xe3\x80\x80 is the UTF-8 sequence of full-width space.
# The last whitespace is full-width space. (U+3000).
RE_SPLIT = r'(?: |\xe3\x80\x80|　)+'


def NormalizeString(string):
  normalized = unicodedata.normalize('NFKC', string)
  return normalized.replace('~', '〜')


def RemoveTrailingNumber(string):
  if not string:
    return ''
  return re.sub(r'^([^0-9]+)[0-9]+$', r'\1', string)


def GetReadingsFromDescription(description):
  if not description:
    return []
  normalized = NormalizeString(description)
  # Description format example:
  #  - 電話１（プッシュホン）
  #  - ビル・建物
  # \xE3\x83\xBB : "・"
  return [
      RemoveTrailingNumber(token)
      for token in re.split(r'(?:\(|\)|/|\xE3\x83\xBB|・)+', normalized)
  ]


def ReadEmojiTsv(stream):
  """Reads emoji data from stream and returns zero query data."""
  zero_query_dict = collections.defaultdict(list)
  stream = code_generator_util.SkipLineComment(stream)
  for columns in code_generator_util.ParseColumnStream(stream, delimiter='\t'):
    if len(columns) != 7:
      logging.critical('format error: %s', '\t'.join(columns))
      sys.exit(1)

    # Emoji code point.
    codepoints, emoji, readings, _, japanese_name, descriptions, _ = columns

    if not codepoints:
      continue

    reading_list = []
    for reading in re.split(RE_SPLIT, NormalizeString(readings)):
      if not reading:
        continue
      reading_list.append(reading)

    reading_list.extend(GetReadingsFromDescription(japanese_name))
    for description in re.split(RE_SPLIT, NormalizeString(descriptions)):
      if not description:
        continue
      reading_list.extend(GetReadingsFromDescription(description))

    for description in set(reading_list):
      if not description:
        continue
      zero_query_dict[description].append(
          util.ZeroQueryEntry(util.ZERO_QUERY_TYPE_EMOJI, emoji)
      )

  # Sort emoji for each reading.
  for key in zero_query_dict.keys():
    zero_query_dict[key].sort(key=lambda e: e.value)

  return zero_query_dict


def ReadZeroQueryRuleData(input_stream):
  """Reads zero query rule data from stream and returns zero query data."""
  zero_query_dict = collections.defaultdict(list)

  for line in input_stream:
    if line.startswith('#'):
      continue
    line = line.rstrip('\r\n')
    if not line:
      continue

    tokens = line.split('\t')
    key = tokens[0]
    values = tokens[1].split(',')

    for value in values:
      zero_query_dict[key].append(
          util.ZeroQueryEntry(util.ZERO_QUERY_TYPE_NONE, value)
      )
  return zero_query_dict


def ReadEmoticonTsv(stream):
  """Reads emoticon data from stream and returns zero query data."""
  zero_query_dict = collections.defaultdict(list)
  stream = code_generator_util.SkipLineComment(stream)
  for columns in code_generator_util.ParseColumnStream(stream, delimiter='\t'):
    if len(columns) != 3:
      logging.critical('format error: %s', '\t'.join(columns))
      sys.exit(1)

    emoticon = columns[0]
    readings = columns[2]

    for reading in re.split(RE_SPLIT, readings.strip()):
      if not reading:
        continue
      zero_query_dict[reading].append(
          util.ZeroQueryEntry(util.ZERO_QUERY_TYPE_EMOTICON, emoticon)
      )

  return zero_query_dict


def ReadSymbolTsv(stream):
  """Reads emoji data from stream and returns zero query data."""
  zero_query_dict = collections.defaultdict(list)
  stream = code_generator_util.SkipLineComment(stream)
  for columns in code_generator_util.ParseColumnStream(stream, delimiter='\t'):
    if len(columns) < 3:
      logging.warning('format error: %s', '\t'.join(columns))
      continue

    symbol = columns[1]
    readings = columns[2]

    symbol_unicode = symbol
    if len(symbol_unicode) != 1:
      continue

    symbol_code_point = ord(symbol_unicode)
    # Selects emoji symbols from symbol dictionary.
    # TODO(toshiyuki): Update the range if we need.
    # from "☀"(black sun with rays) to "❧"(rotated floral heart).
    if not (0x2600 <= symbol_code_point and symbol_code_point <= 0x2767):
      continue

    for reading in re.split(RE_SPLIT, readings.strip()):
      if not reading:
        continue
      zero_query_dict[reading].append(
          util.ZeroQueryEntry(util.ZERO_QUERY_TYPE_NONE, symbol)
      )

    if len(columns) >= 4 and columns[3]:
      # description: "天気", etc.
      description = columns[3]
      zero_query_dict[description].append(
          util.ZeroQueryEntry(util.ZERO_QUERY_TYPE_NONE, symbol)
      )
    if len(columns) >= 5 and columns[4]:
      # additional_description: "傘", etc.
      additional_description = columns[4]
      zero_query_dict[additional_description].append(
          util.ZeroQueryEntry(util.ZERO_QUERY_TYPE_NONE, symbol)
      )

  return zero_query_dict


def IsValidKeyForZeroQuery(key):
  """Returns if the key is valid for zero query trigger."""
  is_ascii = all(ord(char) < 128 for char in key)
  return not is_ascii


def MergeZeroQueryData(rule_dict, symbol_dict, emoji_dict, emoticon_dict):
  """Returnes merged zero query data."""
  merged = collections.defaultdict(list)
  for key in rule_dict.keys():
    merged[key].extend(rule_dict[key])

  for key in emoji_dict.keys():
    if not IsValidKeyForZeroQuery(key):
      continue
    # Skips aggressive emoji candidates.
    # Example: "顔", "天気" are category name and have many candidates.
    if len(emoji_dict[key]) > 3:
      continue
    merged[key].extend(emoji_dict[key])

  for key in emoticon_dict.keys():
    if not IsValidKeyForZeroQuery(key):
      continue
    # Merges only up to 3 emoticons.
    # Example: "にこにこ" have many candidates.
    merged[key].extend(emoticon_dict[key][:3])

  for key in symbol_dict.keys():
    if not IsValidKeyForZeroQuery(key):
      continue
    # Skip aggressive emoji candidates.
    # Example: "きごう" is category name and has many candidates.
    if len(symbol_dict[key]) > 3:
      continue
    merged[key].extend(symbol_dict[key])
  return merged


def ParseOptions() -> argparse.Namespace:
  """Parse command line flags."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--input_rule', dest='input_rule', help='rule file')
  parser.add_argument(
      '--input_symbol', dest='input_symbol', help='symbol data file'
  )
  parser.add_argument(
      '--input_emoji', dest='input_emoji', help='emoji data file'
  )
  parser.add_argument(
      '--input_emoticon', dest='input_emoticon', help='emoticon data file'
  )
  parser.add_argument('--output_tsv', dest='output_tsv', help='output tsv file')
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


def OpenFile(filename):
  return codecs.open(filename, 'r', encoding='utf-8')


def main():
  options = ParseOptions()
  with OpenFile(options.input_rule) as input_stream:
    zero_query_rule_dict = ReadZeroQueryRuleData(input_stream)
  with OpenFile(options.input_symbol) as input_stream:
    zero_query_symbol_dict = ReadSymbolTsv(input_stream)
  with OpenFile(options.input_emoji) as input_stream:
    zero_query_emoji_dict = ReadEmojiTsv(input_stream)
  with OpenFile(options.input_emoticon) as input_stream:
    zero_query_emoticon_dict = ReadEmoticonTsv(input_stream)

  merged_zero_query_dict = MergeZeroQueryData(
      zero_query_rule_dict,
      zero_query_symbol_dict,
      zero_query_emoji_dict,
      zero_query_emoticon_dict,
  )

  util.WriteZeroQueryData(
      merged_zero_query_dict,
      options.output_token_array,
      options.output_string_array,
  )

  if options.output_tsv:
    with codecs.open(options.output_tsv, 'w', encoding='utf-8') as f:
      for key, values in merged_zero_query_dict.items():
        for value in values:
          f.write(key + '\t' + value.value + '\n')


if __name__ == '__main__':
  main()
