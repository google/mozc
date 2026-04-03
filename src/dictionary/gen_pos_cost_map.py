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

"""A script to generate a C++ header file for kPosTypeStringTable."""

import codecs
import optparse

from build_tools import code_generator_util


def ParseUserPos(user_pos_file):
  with codecs.open(user_pos_file, 'r', encoding='utf8') as stream:
    stream = code_generator_util.SkipLineComment(stream)
    stream = code_generator_util.ParseColumnStream(stream, num_column=5)
    return [
        [pos, enum_value, cost]
        for pos, enum_value, cform, ctype, cost in stream
    ]


def OutputPosCostMap(pos_cost_entries, output):
  """Exports the POS cost map to a specified output.

  Args:
      pos_cost_entries: list of [pos, enum_value, cost]
      output: output stream

  Returns:
      None
  """
  output.write(
      'constexpr std::pair<absl::string_view, uint16_t> kPosTypeStringTable[]'
      ' = {\n'
  )
  for pos, enum_value, cost in pos_cost_entries:
    pos = code_generator_util.ToCppStringLiteral(pos)
    output.write(f'  {{ {pos}, {cost} }},  // {enum_value}\n')

  output.write('};\n\n')

  # Write static_assert to check the enum value and index are strictly aligned.
  for i, (pos, enum_value, cost) in enumerate(pos_cost_entries):
    output.write(
        f'static_assert(static_cast<int>(::mozc::user_dictionary::UserDictionary::{enum_value})'
        f' == {i});\n'
    )


def ParseOptions():
  parser = optparse.OptionParser()
  # Input: user_pos.def
  # Output: pos_cost_map.inc
  parser.add_option(
      '--user_pos_file', dest='user_pos_file', help='Path to user_pos.def'
  )
  parser.add_option(
      '--output', dest='output', help='Path to output pos_cost_map.h'
  )
  return parser.parse_args()[0]


def main():
  options = ParseOptions()

  pos_cost_entries = ParseUserPos(options.user_pos_file)

  with open(options.output, 'w') as stream:
    OutputPosCostMap(pos_cost_entries, stream)


if __name__ == '__main__':
  main()
