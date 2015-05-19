# -*- coding: utf-8 -*-
# Copyright 2010-2012, Google Inc.
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

__author__ = "hidehiko"

import optparse
from build_tools import code_generator_util
from rewriter import embedded_dictionary_compiler

# key, value, rank
NOUN_PREFIX = [
    ['お', 'お', 1],
    ['ご', 'ご', 1],
    # ['ご', '誤'],    # don't register it as 誤 isn't in the ipadic.
    # ['み', 'み'],    # seems to be rare.
    ['もと', 'もと', 1],
    ['だい', '代', 1],
    ['てい', '低', 0],
    ['もと', '元', 1],
    ['ぜん', '全', 0],
    ['さい', '再', 0],
    ['しょ', '初', 1],
    ['はつ', '初', 0],
    ['ぜん', '前', 1],
    ['かく', '各', 1],
    ['どう', '同', 1],
    ['だい', '大', 1],
    ['おお', '大', 1],
    ['とう', '当', 1],
    ['ご', '御', 1],
    ['お', '御', 1],
    ['しん', '新', 1],
    ['さい', '最', 1],
    ['み', '未', 0],
    ['ほん', '本', 1],
    ['む', '無', 0],
    ['だい', '第', 1],
    ['とう', '等', 1],
    ['やく', '約', 1],
    ['ひ', '被', 1],
    ['ちょう', '超', 1],
    ['ちょう', '長', 1],
    ['なが', '長', 1],
    ['ひ', '非', 1],
    ['こう', '高', 1]
]


def ReadSingleKanji(stream):
  """Parses single kanji dictionary data from stream."""
  stream = code_generator_util.SkipLineComment(stream)
  stream = code_generator_util.ParseColumnStream(stream, num_column=2)
  outputs = list(stream)
  # For binary search by |key|, sort outputs here.
  outputs.sort(lambda x, y: cmp(x[0], y[0]))

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
  variant_items.sort(lambda x, y: cmp(x[0], y[0]))

  return (variant_types, variant_items)


def GenNounPrefix():
  """Generates noun prefix embedded dictioanry entries."""
  token_map = {}
  for entry in NOUN_PREFIX:
    key = entry[0]
    value = entry[1]
    rank = entry[2]
    token_map.setdefault(key, []).append(
        embedded_dictionary_compiler.Token(
            key, value, '', '', 0, 0, rank))

  return token_map


def WriteSingleKanji(outputs, stream):
  """Writes single kanji list for readings."""
  stream.write('static const SingleKanjiList kSingleKanjis[] = {\n')
  for output in outputs:
    (key, values) = output
    stream.write('  // %s, %s\n' % (key, values))
    stream.write(code_generator_util.FormatWithCppEscape(
        '  { %s, %s },\n', key, values))
  stream.write('};\n')


def WriteVariantInfo(variant_info, stream):
  """Writes single kanji variants info."""
  (variant_types, variant_items) = variant_info

  stream.write('static const char *kKanjiVariantTypes[] = {\n')
  for variant_type in variant_types:
    stream.write(code_generator_util.FormatWithCppEscape(
        '  %s,', variant_type))
    stream.write('  // %s\n' % variant_type)
  stream.write('};\n')

  stream.write('static const KanjiVariantItem kKanjiVariants[] = {\n')
  for item in variant_items:
    (target, original, variant_type) = item
    stream.write(code_generator_util.FormatWithCppEscape(
        '  { %s, %s, %d },', target, original, variant_type))
    stream.write('  // %s, %s, %d\n' % (target, original, variant_type))
  stream.write('};\n')


def _ParseOptions():
  parser = optparse.OptionParser()

  parser.add_option('--single_kanji_file', dest='single_kanji_file',
                    help='Single kanji file')
  parser.add_option('--variant_file', dest='variant_file',
                    help='Variant rule file')
  parser.add_option('--output', dest='output', help='Output header file.')

  return parser.parse_args()[0]


def main():
  options = _ParseOptions()

  with open(options.single_kanji_file, 'r') as single_kanji_stream:
    single_kanji = ReadSingleKanji(single_kanji_stream)

  with open(options.variant_file, 'r') as variant_stream:
    variant_info = ReadVariant(variant_stream)

  noun_prefix = GenNounPrefix()

  with open(options.output, 'w') as output_stream:
    WriteSingleKanji(single_kanji, output_stream)
    WriteVariantInfo(variant_info, output_stream)
    embedded_dictionary_compiler.Compile(
        'NounPrefixData', noun_prefix, output_stream)


if __name__ == '__main__':
  main()
