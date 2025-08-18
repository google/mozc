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

"""Single kanji dictionary generator.

How to run this script:
gen_single_kanji_rewriter_data.py
  --single_kanji_file=single_kanji.tsv
  --variant_file=variant_rule.txt
  --output=single_kanji_data.h
"""


import codecs
import optparse
import struct

from build_tools import code_generator_util
from build_tools import serialized_string_array_builder


def ReadSingleKanji(stream):
  """Parses single kanji dictionary data from stream."""
  stream = code_generator_util.SkipLineComment(stream)
  stream = code_generator_util.ParseColumnStream(stream, num_column=2)
  outputs = list(stream)
  # For binary search by |key|, sort outputs here.
  outputs.sort(key=lambda x: x[0])

  return outputs


def ReadVariant(stream):
  """Parses variant data from stream."""
  variant_types = []
  variant_items = []

  stream = code_generator_util.SkipLineComment(stream)
  stream = code_generator_util.ParseColumnStream(stream)
  for tokens in stream:
    if len(tokens) == 1:
      variant_types.append(tokens[0])
    elif len(tokens) == 2 and variant_types:
      (target, original) = tokens
      variant_items.append([target, original, len(variant_types) - 1])

  # For binary search by |target|, sort variant items here.
  variant_items.sort(key=lambda x: x[0])

  return (variant_types, variant_items)


def WriteSingleKanji(single_kanji_dic, output_tokens, output_string_array):
  """Writes single kanji list for readings.

  The token output is an array of uint32s, where array[2 * i] and
  array[2 * i + 1] are the indices of key and value in the string array.
  See rewriter/single_kanji_rewriter.cc.
  """
  strings = []
  with open(output_tokens, 'wb') as f:
    for index, (key, value) in enumerate(single_kanji_dic):
      f.write(struct.pack('<I', 2 * index))
      f.write(struct.pack('<I', 2 * index + 1))
      strings.append(key)
      strings.append(value)
  serialized_string_array_builder.SerializeToFile(strings, output_string_array)


def WriteVariantInfo(
    variant_info,
    output_variant_types,
    output_variant_tokens,
    output_variant_strings,
):
  """Writes single kanji variants info.

  The token output is an array of uint32s, where array[3 * i],
  array[3 * i + 1] and array[3 * i + 2] are the index of target, index of
  original,  and variant type ID. See rewriter/single_kanji_rewriter.cc.
  """
  (variant_types, variant_items) = variant_info

  serialized_string_array_builder.SerializeToFile(
      variant_types, output_variant_types
  )

  strings = []
  with open(output_variant_tokens, 'wb') as f:
    for index, (target, original, variant_type) in enumerate(variant_items):
      f.write(struct.pack('<I', 2 * index))
      f.write(struct.pack('<I', 2 * index + 1))
      f.write(struct.pack('<I', variant_type))
      strings.append(target)
      strings.append(original)

  serialized_string_array_builder.SerializeToFile(
      strings, output_variant_strings
  )


def _ParseOptions():
  parser = optparse.OptionParser()

  parser.add_option(
      '--single_kanji_file', dest='single_kanji_file', help='Single kanji file'
  )
  parser.add_option(
      '--variant_file', dest='variant_file', help='Variant rule file'
  )
  parser.add_option(
      '--output_single_kanji_token',
      dest='output_single_kanji_token',
      help='Output Single Kanji token data.',
  )
  parser.add_option(
      '--output_single_kanji_string',
      dest='output_single_kanji_string',
      help='Output Single Kanji string data.',
  )
  parser.add_option(
      '--output_variant_types',
      dest='output_variant_types',
      help='Output variant types.',
  )
  parser.add_option(
      '--output_variant_tokens',
      dest='output_variant_tokens',
      help='Output variant tokens.',
  )
  parser.add_option(
      '--output_variant_strings',
      dest='output_variant_strings',
      help='Output variant strings.',
  )

  return parser.parse_args()[0]


def main():
  options = _ParseOptions()

  with codecs.open(
      options.single_kanji_file, 'r', encoding='utf-8'
  ) as single_kanji_stream:
    single_kanji = ReadSingleKanji(single_kanji_stream)

  with codecs.open(
      options.variant_file, 'r', encoding='utf-8'
  ) as variant_stream:
    variant_info = ReadVariant(variant_stream)

  WriteSingleKanji(
      single_kanji,
      options.output_single_kanji_token,
      options.output_single_kanji_string,
  )
  WriteVariantInfo(
      variant_info,
      options.output_variant_types,
      options.output_variant_tokens,
      options.output_variant_strings,
  )


if __name__ == '__main__':
  main()
