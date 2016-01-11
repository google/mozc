# -*- coding: utf-8 -*-
# Copyright 2010-2016, Google Inc.
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

"""Generate header file for zero query suggestion.

Output format:
const char *kKey0 = "key0";
const ZeroQueryEntry kValues0[] = {
  {zero_query_type00, "Cand00", emoji_type00, codepoint00},
  {zero_query_type01, "Cand01", emoji_type01, codepoint01},
  ..
};
const char *kKey1 = "key1";
const ZeroQueryEntry kValues1[] = {
  {zero_query_type10, "Cand10", emoji_type10, codepoint10},
  {zero_query_type11, "Cand11", emoji_type11, codepoint11},
  ..
};
..

const ZeroQueryList kData_data[] = {
  {kKey0, kValues0, values0_size},
  {kKey1, kValues1, values1_size},
  ..
};
const size_t kData_size = data_size;


Here, (kKey0, kKey1, ...) is sorted so that we can use binary search.
"""

__author__ = "toshiyuki"

import os
from build_tools import code_generator_util as cgu


_MOZC_DIR_FOR_INCLUDE_GUARD = 'GOOGLECLIENT_IME_MOZC'

ZERO_QUERY_TYPE_NONE = 0
ZERO_QUERY_TYPE_NUMBER_SUFFIX = 1
ZERO_QUERY_TYPE_EMOTICON = 2
ZERO_QUERY_TYPE_EMOJI = 3

# bit fields
# These are standing for command::Request::EmojiCarrierType
EMOJI_TYPE_NONE = 0
EMOJI_TYPE_UNICODE = 1
EMOJI_TYPE_DOCOMO = 2
EMOJI_TYPE_SOFTBANK = 4
EMOJI_TYPE_KDDI = 8


class ZeroQueryEntry(object):

  def __init__(self, entry_type, value, emoji_type, emoji_android_pua):
    self.entry_type = entry_type
    self.value = value
    self.emoji_type = emoji_type
    self.emoji_android_pua = emoji_android_pua


def ZeroQueryTypeToString(zero_query_type):
  """Returns a string for C++ code indicating zero query type."""
  if zero_query_type == ZERO_QUERY_TYPE_NONE:
    return 'ZERO_QUERY_NONE'
  elif zero_query_type == ZERO_QUERY_TYPE_NUMBER_SUFFIX:
    return 'ZERO_QUERY_NUMBER_SUFFIX'
  elif zero_query_type == ZERO_QUERY_TYPE_EMOTICON:
    return 'ZERO_QUERY_EMOTICON'
  elif zero_query_type == ZERO_QUERY_TYPE_EMOJI:
    return 'ZERO_QUERY_EMOJI'
  return 0


def EmojiTypeToString(emoji_type):
  """Returns a string for C++ code indicating emoji type."""
  if emoji_type == EMOJI_TYPE_NONE:
    return 'EMOJI_NONE'

  types = []
  if emoji_type & EMOJI_TYPE_UNICODE:
    types.append('EMOJI_UNICODE')
  if emoji_type & EMOJI_TYPE_DOCOMO:
    types.append('EMOJI_DOCOMO')
  if emoji_type & EMOJI_TYPE_SOFTBANK:
    types.append('EMOJI_SOFTBANK')
  if emoji_type & EMOJI_TYPE_KDDI:
    types.append('EMOJI_KDDI')
  return ' | '.join(types)


def GetIncludeGuardSymbol(file_name):
  """Returns include guard symbol for .h file.

  For example, returns 'SOME_EXAMPLE_H' for '/path/to/some_example.h'

  Args:
    file_name: a string indicating output file path.
  Returns:
    A string for include guard.
  """
  return os.path.basename(file_name).upper().replace('.', '_')


def WriteIncludeGuardHeader(output_file_name, output_stream):
  """Returns include guard header for .h file."""
  output_stream.write('#ifndef %s_PREDICTION_%s_\n' %(
      _MOZC_DIR_FOR_INCLUDE_GUARD, GetIncludeGuardSymbol(output_file_name)))
  output_stream.write('#define %s_PREDICTION_%s_\n' %(
      _MOZC_DIR_FOR_INCLUDE_GUARD, GetIncludeGuardSymbol(output_file_name)))


def WriteIncludeGuardFooter(output_file_name, output_stream):
  """Returns include guard footer for .h file."""
  output_stream.write('#endif  // %s_PREDICTION_%s_\n' %(
      _MOZC_DIR_FOR_INCLUDE_GUARD, GetIncludeGuardSymbol(output_file_name)))


def WriteHeaderFileForZeroQuery(
    zero_query_dict, output_file_name, var_name, output_stream):
  """Returns contents for header file that contains a string array."""

  WriteIncludeGuardHeader(output_file_name, output_stream)
  output_stream.write(
      '#include "./prediction/zero_query_list.h"\n')
  output_stream.write('namespace mozc {\n')
  output_stream.write('namespace {\n')

  sorted_keys = sorted(zero_query_dict.keys())
  for i, key in enumerate(sorted_keys):
    if i:
      output_stream.write('\n')
    output_stream.write('const char *%s_key%d = %s;  // "%s"\n' % (
        var_name, i, cgu.ToCppStringLiteral(key), key))
    output_stream.write(
        'const ZeroQueryEntry %s_values%d[] = {\n' % (var_name, i))
    output_stream.write(
        '\n'.join([
            '  {%s, %s, %s, 0x%x},  // "%s"' % (
                ZeroQueryTypeToString(e.entry_type),
                cgu.ToCppStringLiteral(e.value),
                EmojiTypeToString(e.emoji_type),
                e.emoji_android_pua,
                e.value)
            for e in zero_query_dict[key]]) +
        '\n')
    output_stream.write('};\n')

  output_stream.write('} // namespace\n')

  output_stream.write('const ZeroQueryList %s_data[] = {\n' % var_name)
  output_stream.write(',\n'.join(
      ['  {%s_key%d, %s_values%d, %d}' % (
          var_name, c, var_name, c, len(zero_query_dict[key]))
       for c, key in enumerate(sorted_keys)]) + '\n')
  output_stream.write('};\n')
  output_stream.write(
      'const size_t %s_size = %d;' % (var_name, len(sorted_keys)) + '\n')

  output_stream.write('} // namespace mozc\n')
  WriteIncludeGuardFooter(output_file_name, output_stream)
