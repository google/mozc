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

"""A script to generate a C++ header file for the POS conversion map."""
import codecs
import optparse

from build_tools import code_generator_util


HEADER = """// Copyright 2009 Google Inc. All Rights Reserved.
// Author: keni

#ifndef MOZC_DICTIONARY_POS_MAP_INC_
#define MOZC_DICTIONARY_POS_MAP_INC_

// POS conversion rules
constexpr PosMap kPosMap[] = {
"""
FOOTER = """};

#endif  // MOZC_DICTIONARY_POS_MAP_INC_
"""


def ParseUserPos(user_pos_file):
  with codecs.open(user_pos_file, 'r', encoding='utf8') as stream:
    stream = code_generator_util.SkipLineComment(stream)
    stream = code_generator_util.ParseColumnStream(stream, num_column=2)
    return dict((key, enum_value) for key, enum_value in stream)


def GeneratePosMap(third_party_pos_map_file, user_pos_file):
  user_pos_map = ParseUserPos(user_pos_file)

  result = {}
  with codecs.open(third_party_pos_map_file, 'r', encoding='utf8') as stream:
    stream = code_generator_util.SkipLineComment(stream)
    for columns in code_generator_util.ParseColumnStream(stream, num_column=2):
      third_party_pos_name, mozc_pos = (columns + [None])[:2]
      if mozc_pos is not None:
        mozc_pos = user_pos_map[mozc_pos]

      if third_party_pos_name in result:
        assert result[third_party_pos_name] == mozc_pos
        continue

      result[third_party_pos_name] = mozc_pos

  # Create mozc_pos to mozc_pos map.
  for key, value in user_pos_map.items():
    if key in result:
      assert result[key] == value
      continue
    result[key] = value

  return result


def OutputPosMap(pos_map, output):
  output.write(HEADER)
  for key, value in sorted(pos_map.items()):
    key = code_generator_util.ToCppStringLiteral(key)
    if value is None:
      # Invalid PosType.
      value = (
          'static_cast< ::mozc::user_dictionary::UserDictionary::PosType>(-1)'
      )
    else:
      value = '::mozc::user_dictionary::UserDictionary::' + value
    output.write('  { %s, %s },\n' % (key, value))
  output.write(FOOTER)


def ParseOptions():
  parser = optparse.OptionParser()
  # Input: user_pos.def, third_party_pos_map.def
  # Output: pos_map.h
  parser.add_option(
      '--user_pos_file', dest='user_pos_file', help='Path to user_pos.def'
  )
  parser.add_option(
      '--third_party_pos_map_file',
      dest='third_party_pos_map_file',
      help='Path to third_party_pos_map.def',
  )
  parser.add_option('--output', dest='output', help='Path to output pos_map.h')
  return parser.parse_args()[0]


def main():
  options = ParseOptions()

  pos_map = GeneratePosMap(
      options.third_party_pos_map_file, options.user_pos_file
  )

  with open(options.output, 'w') as stream:
    OutputPosMap(pos_map, stream)


if __name__ == '__main__':
  main()
