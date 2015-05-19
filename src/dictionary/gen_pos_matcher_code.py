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

"""A tool to generate POS matcher."""

__author__ = "taku"

import optparse
import re
import sys

from dictionary import pos_util


def OutputPosMatcherHeader(pos_matcher, output):
  # Output header.
  output.write(
      '#ifndef MOZC_DICTIONARY_POS_MATCHER_H_\n'
      '#define MOZC_DICTIONARY_POS_MATCHER_H_\n'
      '#include "./base/base.h"\n'
      'namespace mozc {\n'
      'namespace {\n'
      'class POSMatcher {\n'
      ' public:\n')

  # Output in the original file's rule order.
  for rule_name in pos_matcher.GetRuleNameList():
    # Construct an expression to accept the range.
    def _RangeToCondition(name, min_id, max_id):
      if min_id == max_id:
        return '(%s == %d)' % (name, min_id)
      return '(%s >= %d && %s <= %d)' % (name, min_id, name, max_id)
    condition = ' || '.join(
        _RangeToCondition('id', *id_range)
        for id_range in pos_matcher.GetRange(rule_name))

    output.write(
        '  // %(rule_name)s "%(original_pattern)s"\n'
        '  static uint16 Get%(rule_name)sId() {\n'
        '    return %(id)d;\n'
        '  }\n'
        '  static bool Is%(rule_name)s(uint16 id) {\n'
        '    return (%(condition)s);\n'
        '  }\n' % {
            'rule_name': rule_name,
            'original_pattern': pos_matcher.GetOriginalPattern(rule_name),
            'condition': condition,
            'id': pos_matcher.GetId(rule_name)
        })

  # Output footer.
  output.write(
      ' private:\n'
      '  POSMatcher() {}\n'
      '  ~POSMatcher() {}\n'
      '};\n'
      '}  // namespace\n'
      '}  // mozc\n'
      '#endif  // MOZC_DICTIONARY_POS_MATCHER_H_\n')


def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('--id_file', dest='id_file', help='Path to id.def')
  parser.add_option('--special_pos_file', dest='special_pos_file',
                    help='Path to special_pos.def')
  parser.add_option('--pos_matcher_rule_file', dest='pos_matcher_rule_file',
                    help='Path to pos_matcher_rule.def')
  parser.add_option('--output', dest='output',
                    help='Path to the output header file.')
  return parser.parse_args()[0]


def main():
  options = ParseOptions()
  pos_database = pos_util.PosDataBase()
  pos_database.Parse(options.id_file, options.special_pos_file)
  pos_matcher = pos_util.PosMatcher(pos_database)
  pos_matcher.Parse(options.pos_matcher_rule_file)

  with open(options.output, 'w') as stream:
    OutputPosMatcherHeader(pos_matcher, stream)


if __name__ == "__main__":
  main()
