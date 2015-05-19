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

"""Generate emoji data file (.java)

Generated .java file is used by Android version.
"""

__author__ = "yoichio"

from collections import defaultdict
import optparse

from build_tools import code_generator_util

CATEGORY_LIST = ['FACE', 'FOOD', 'CITY', 'ACTIVITY', 'NATURE']


def ReadData(stream):
  category_map = defaultdict(list)
  stream = code_generator_util.SkipLineComment(stream)
  stream = code_generator_util.ParseColumnStream(stream, delimiter='\t')
  stream = code_generator_util.SelectColumn(stream, [2, 9, 10, 11, 12])
  for (code, docomo_name, softbank_name, kddi_name, category_index) in stream:
    if not code or code[0] == '>':
      continue
    (category, index) = category_index.split('-')
    category_map[category].append(
        (index, int(code, 16), docomo_name, softbank_name, kddi_name))
  return category_map


CHARA_NORMALIZE_MAP = {
    u'Ａ': 'A',
    u'Ｂ': 'B',
    u'Ｃ': 'C',
    u'Ｄ': 'D',
    u'Ｅ': 'E',
    u'Ｆ': 'F',
    u'Ｇ': 'G',
    u'Ｈ': 'H',
    u'Ｉ': 'I',
    u'Ｊ': 'J',
    u'Ｋ': 'K',
    u'Ｌ': 'L',
    u'Ｍ': 'M',
    u'Ｎ': 'N',
    u'Ｏ': 'O',
    u'Ｐ': 'P',
    u'Ｑ': 'Q',
    u'Ｒ': 'R',
    u'Ｓ': 'S',
    u'Ｔ': 'T',
    u'Ｕ': 'U',
    u'Ｖ': 'V',
    u'Ｗ': 'W',
    u'Ｘ': 'X',
    u'Ｙ': 'Y',
    u'Ｚ': 'Z',

    u'ａ': 'a',
    u'ｂ': 'b',
    u'ｃ': 'c',
    u'ｄ': 'd',
    u'ｅ': 'e',
    u'ｆ': 'f',
    u'ｇ': 'g',
    u'ｈ': 'h',
    u'ｉ': 'i',
    u'ｊ': 'j',
    u'ｋ': 'k',
    u'ｌ': 'l',
    u'ｍ': 'm',
    u'ｎ': 'n',
    u'ｏ': 'o',
    u'ｐ': 'p',
    u'ｑ': 'q',
    u'ｒ': 'r',
    u'ｓ': 's',
    u'ｔ': 't',
    u'ｕ': 'u',
    u'ｖ': 'v',
    u'ｗ': 'w',
    u'ｘ': 'x',
    u'ｙ': 'y',
    u'ｚ': 'z',

    u'０': '0',
    u'１': '1',
    u'２': '2',
    u'３': '3',
    u'４': '4',
    u'５': '5',
    u'６': '6',
    u'７': '7',
    u'８': '8',
    u'９': '9',

    u'（': '(',
    u'）': ')',
}

def PreprocessName(name):
  name = unicode(name, 'utf-8')
  name = u''.join(CHARA_NORMALIZE_MAP.get(c, c) for c in name)
  name = name.encode('utf-8')
  name = name.replace('(', '\\n(')
  return name


def OutputData(category_map, stream):
  for data_list in category_map.itervalues():
    data_list.sort()

  stream.write('package org.mozc.android.inputmethod.japanese.emoji;\n'
               'public class EmojiData {\n')

  for category in CATEGORY_LIST:
    data_list = category_map[category]
    stream.write(
        '  public static final String[] %s_VALUES = new String[]{\n' %
        category)
    for _, code, docomo, softbank, kddi in data_list:
      if not docomo and not softbank and not kddi:
        continue
      stream.write(
          '    %s,\n' % (
              code_generator_util.ToJavaSurrogatePairStringLiteral(code)))
    stream.write('  };\n')

    stream.write(
        '  public static final String[] DOCOMO_%s_NAME = {\n' % category)
    for _, code, docomo, softbank, kddi in data_list:
      if not docomo and not softbank and not kddi:
        continue
      if docomo:
        stream.write('    "%s", \n' % PreprocessName(docomo))
      else:
        stream.write('    null, \n')
    stream.write('  };\n')

    stream.write(
        '  public static final String[] SOFTBANK_%s_NAME = {\n' % category)
    for _, code, docomo, softbank, kddi in data_list:
      if not docomo and not softbank and not kddi:
        continue
      if softbank:
        stream.write('    "%s", \n' % PreprocessName(softbank))
      else:
        stream.write('    null, \n')
    stream.write('  };\n')

    stream.write(
        '  public static final String[] KDDI_%s_NAME = {\n' % category)
    for _, code, docomo, softbank, kddi in data_list:
      if not docomo and not softbank and not kddi:
        continue
      if kddi:
        stream.write('    "%s", \n' % PreprocessName(kddi))
      else:
        stream.write('    null, \n')
    stream.write('  };\n')

  stream.write('}\n')


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--emoji_data', dest='emoji_data',
                    help='Path to emoji_data.tsv')
  parser.add_option('--output', dest='output', help='Output file name')
  return parser.parse_args()[0]


def main():
  options = ParseOptions()
  with open(options.emoji_data) as stream:
    emoji_data = ReadData(stream)

  with open(options.output, 'w') as stream:
    OutputData(emoji_data, stream)


if __name__ == '__main__':
  main()
