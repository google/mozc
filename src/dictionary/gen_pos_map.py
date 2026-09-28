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


HEADER = """// POS conversion rules from third-party IME POS names to Mozc POS types.
// This file contains only the key-value entries, and is designed to be
// #included inside the braced entry list of a map definition; see
// user_dictionary_importer.cc.
"""

_PROTO_PACKAGE = 'org.mozc.android.inputmethod.japanese.protobuf'

JAVA_HEADER = f"""package com.google.android.apps.inputmethod.libs.mozc.session;

import {_PROTO_PACKAGE}.ProtoUserDictionaryStorage.UserDictionary.PosType;
import com.google.common.collect.ImmutableMap;

// POS conversion rules
final class PosMap {{
  static final ImmutableMap<String, PosType> POS_MAP =
      ImmutableMap.<String, PosType>builder()
"""
JAVA_FOOTER = """          .buildOrThrow();

  private PosMap() {}
}
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
    output.write('      { %s, %s },\n' % (key, value))


def OutputPosMapJava(pos_map, output):
  output.write(JAVA_HEADER)
  for key, value in sorted(pos_map.items()):
    if value is None:
      continue
    key = code_generator_util.ToCppStringLiteral(key)
    output.write(f'          .put({key}, PosType.{value})\n')
  output.write(JAVA_FOOTER)


def ParseOptions():
  parser = optparse.OptionParser()
  # Input: user_pos.def, third_party_pos_map.def
  # Output: pos_map.h or PosMap.java
  parser.add_option(
      '--user_pos_file', dest='user_pos_file', help='Path to user_pos.def'
  )
  parser.add_option(
      '--third_party_pos_map_file',
      dest='third_party_pos_map_file',
      help='Path to third_party_pos_map.def',
  )
  parser.add_option('--output', dest='output', help='Path to output pos_map.h')
  parser.add_option(
      '--output_java', dest='output_java', help='Path to output PosMap.java'
  )
  return parser.parse_args()[0]


def main():
  options = ParseOptions()

  pos_map = GeneratePosMap(
      options.third_party_pos_map_file, options.user_pos_file
  )

  if options.output:
    with open(options.output, 'w', encoding='utf8') as stream:
      OutputPosMap(pos_map, stream)
  if options.output_java:
    with open(options.output_java, 'w', encoding='utf8') as stream:
      OutputPosMapJava(pos_map, stream)


if __name__ == '__main__':
  main()
