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

"""Utilities to generate source codes."""

import struct

__author__ = "hidehiko"


def ToCppStringLiteral(s):
  """Returns C-style string literal, or NULL if given s is None."""
  if s is None:
    return 'NULL'

  if all(0x20 <= ord(c) <= 0x7E for c in s):
    # All characters are in ascii code.
    return '"%s"' % s.replace('\\', r'\\').replace('"', r'\"')
  else:
    # One or more characters are non-ascii.
    return '"%s"' % ''.join(r'\x%02X' % c for c in s.encode('utf-8'))


def FormatWithCppEscape(format_text, *args):
  """Returns a string filling format with args."""
  literal_list = []
  for arg in args:
    if isinstance(arg, (str, type(None))):
      arg = ToCppStringLiteral(arg)
    literal_list.append(arg)

  return format_text % tuple(literal_list)


def WriteCppDataArray(data, variable_name, target_compiler, stream):
  r"""Format data into C++ style array.

  Visual C++ does not support string literals longer than 65535 characters
  so integer arrays (e.g. arrays of uint64) are used to represent byte arrays
  on Windows.

  The generated code looks like:
    const uint64 kVAR_data_wordtype[] = {
        0x0123456789ABCDEF, ...
    };
    const char * const kVAR_data =
        reinterpret_cast<const char *>(kVAR_data_wordtype);
    const size_t kVAR_size = 123;

  This implementation works well with other toolchains, too, but we use
  string literals for other toolchains.

  The generated code with a string literal looks like:
    const char kVAR_data[] =
        "\\x12\\x34\\x56\\x78...";
    const size_t kVAR_size = 123;

  Args:
    data: original data to be formatted.
    variable_name: the core name of variables.
    target_compiler: the target compiler which will compile the formatted
      code.
    stream: output stream.
  """

  # To accept "target_compiler = None", check target_compiler itself first.
  if target_compiler and target_compiler.startswith('msvs'):
    stream.write('const uint64 k%s_data_wordtype[] = {\n' % variable_name)

    for word_index in range(0, len(data), 8):
      word_chunk = data[word_index:word_index + 8].ljust(8, '\x00')
      stream.write('0x%016X, ' %
                   struct.unpack('<Q', word_chunk.encode('latin-1')))
      if (word_index // 8) % 4 == 3:
        # Line feed for every 4 elements.
        stream.write('\n')

    stream.write('};\n')
    stream.write(
        'const char * const k%s_data = '
        'reinterpret_cast<const char *>(k%s_data_wordtype);\n' % (
            variable_name, variable_name))
  else:
    stream.write('const char k%s_data[] =\n' % variable_name)
    # Output 16bytes per line.
    chunk_size = 16
    for index in range(0, len(data), chunk_size):
      chunk = data[index:index + chunk_size]
      stream.write('"')
      stream.writelines(r'\x%02X' % ord(c) for c in chunk)
      stream.write('"\n')
    stream.write(';\n')

  stream.write('const size_t k%s_size = %d;\n' % (variable_name, len(data)))


def ToJavaStringLiteral(codepoint_list):
  """Returns string literal with surrogate pair and emoji support."""
  if isinstance(codepoint_list, int):
    codepoint_list = (codepoint_list,)
  if not codepoint_list:
    return 'null'
  result = r'"'
  for codepoint in codepoint_list:
    utf16_array = bytearray(chr(codepoint).encode('utf-16be'))
    if len(utf16_array) == 2:
      (u0, l0) = utf16_array
      result += r'\u%02X%02X' % (u0, l0)
    else:
      (u0, l0, u1, l1) = utf16_array
      result += r'\u%02X%02X\u%02X%02X' % (u0, l0, u1, l1)
  result += r'"'
  return result


def SkipLineComment(stream, comment_prefix='#'):
  """Skips line comments from stream."""
  for line in stream:
    stripped_line = line.strip()
    if stripped_line and not stripped_line.startswith(comment_prefix):
      yield line.rstrip('\n')


def ParseColumnStream(stream, num_column=None, delimiter=None):
  """Returns parsed columns read from stream."""
  if num_column is None:
    for line in stream:
      yield line.rstrip('\n').split(delimiter)
  else:
    for line in stream:
      yield line.rstrip('\n').split(delimiter)[:num_column]


def SelectColumn(stream, column_index):
  """Returns the tuple specified by the column_index from the tuple stream."""
  for columns in stream:
    yield tuple(columns[i] for i in column_index)


def SplitChunk(iterable, n):
  """Splits sequence to consecutive n-element chunks.

  Quite similar to grouper in itertools section of python manual,
  but slightly different if len(iterable) is not factor of n.
  grouper extends the last chunk to make it an n-element chunk by adding
  appropriate value, but this returns truncated chunk.
  """
  for index in range(0, len(iterable), n):
    yield iterable[index:index + n]
