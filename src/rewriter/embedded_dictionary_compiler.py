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

"""Tiny compiler for the mozc::EmbeddedDictionary."""

__author__ = "hidehiko"

from build_tools import code_generator_util

class Token(object):
  def __init__(
      self, key, value, description, additional_description, lid, rid, cost):
    self.key = key
    self.value = value
    self.description = description
    self.additional_description = additional_description
    self.lid = lid
    self.rid = rid
    self.cost = cost


def OutputValue(name, input_data, output_stream):
  """Outputs mozc::EmbeddedDictionary::Value data to given output_stream.

  The generated code should look like:
  static const mozc::EmbeddedDictionary::Value kNAME_value[] = {
    { "value", "description", "additional_description", 10, 10, 300 },
       :
    { NULL, NULL, NULL, 0, 0, 0 }
  };
  """

  output_stream.write(
      'static const mozc::EmbeddedDictionary::Value '
      'k%s_value[] = {\n' % name)

  for _, token_list in sorted(input_data.items()):
    for token in sorted(token_list, key=lambda token: token.cost):
      output_stream.write('  { %s, %s, %s, %d, %d, %d },\n' % (
          code_generator_util.ToCppStringLiteral(token.value),
          code_generator_util.ToCppStringLiteral(token.description),
          code_generator_util.ToCppStringLiteral(token.additional_description),
          token.lid, token.rid, token.cost))

  # Output a sentinel.
  output_stream.write('  { NULL, NULL, NULL, 0, 0, 0 }\n')
  output_stream.write('};\n')


def OutputTokenSize(name, input_data, output_stream):
  """Outputs token_size data to the given output_stream.

  The generated code should look like:
  static const size_t kNAME_token_size = 10000;
  """
  output_stream.write(
      'static const size_t k%s_token_size = %d;\n' % (name, len(input_data)))


def OutputTokenData(name, input_data, output_stream):
  """Output token_data to the given output_stream.

  The generated code should look like:
  static const mozc::EmbeddedDictionary::Token kNAME_token_data[] = {
    { "key1", kNAME_value + 0, 10 },
    { "key2", kNAME_value + 10, 30 },
       :
    { NULL, kNAME_value, 10000},
  };
  """
  output_stream.write(
      'static const mozc::EmbeddedDictionary::Token '
      'k%s_token_data[] = {\n' % name)

  offset = 0
  for key, token_list in sorted(input_data.items()):
    size = len(token_list)
    output_stream.write('  { %s, k%s_value + %d, %d },\n' % (
        code_generator_util.ToCppStringLiteral(key), name, offset, size))
    offset += size

  # Sentinel.
  output_stream.write('  { NULL, k%s_value, %d }\n' % (name, offset))
  output_stream.write('};\n')


def Compile(name, input_data, output_stream):
  """Compiles input_data to EmbeddedDictionary, and output it.

  Args:
    name: a name of the embedded dictionary.
    input_data: a map of key to Token list to be compiled.
    output_stream: a stream to which the result should be written.
  """
  OutputValue(name, input_data, output_stream)
  OutputTokenSize(name, input_data, output_stream)
  OutputTokenData(name, input_data, output_stream)


def main():
  pass


if __name__ == '__main__':
  main()
