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

"""Generate symbol data file (.java) from .tsv file.

In this file, "symbol" means both "emoticon (e.g. ＼(^o^)／)"
and "symbol character (e.g. ♪)".
Generated .java file is used by Android version.
The first line of .tsv file is ignored because it is used as label.
"""

__author__ = "matsuzakit"

import io
import optparse
import sys
import unicodedata


PACKAGE_NAME = 'org.mozc.android.inputmethod.japanese'

TEMPLATE_CLASS = """package %s;
public class %s {
%s
}
"""


def ParseSymbolFile(file_name, value_column, category_column,
                    expand_variant_tags):
  """Parses symbol file and returns tag->symbols dictionary."""
  tag2symbol = {}
  is_first_line = True
  for line in io.open(file_name, encoding='utf-8'):
    line_parts = line.rstrip().split('\t')
    if is_first_line:
      # Skip the first line, which is used as label.
      is_first_line = False
      continue
    if len(line_parts) <= value_column or len(line_parts) <= category_column:
      continue
    symbol = line_parts[value_column]
    if not symbol:
      continue
    normalized = unicodedata.normalize('NFKC', symbol)

    for tag in line_parts[category_column].split(' '):
      if not tag:
        continue
      tag2symbol.setdefault(tag, []).append(symbol)
      if tag in expand_variant_tags and symbol != normalized:
        tag2symbol[tag].append(normalized)

  return tag2symbol


def GetStringArrayOfSymbols(tag_name, original_symbols, ordering_rule_list):
  """Generate Java string literal from tag_name and symbol list."""

  def Compare(l, r):
    """Comparing method.

    The rules are:
    1. Characters on the ordering rule is prioritized.
    2. If both are on the rule list, comprare based on the rule.
    3. Lower character code is prioritized.
    """
    l_in_ordering_rule = l in ordering_rule_list
    r_in_ordering_rule = r in ordering_rule_list
    if l_in_ordering_rule and r_in_ordering_rule:
      return cmp(ordering_rule_list.index(l), ordering_rule_list.index(r))
    if l_in_ordering_rule:
      return -1
    if r_in_ordering_rule:
      return +1
    return cmp(l, r)

  lines = ['  public static final String[] %s_VALUES = new String[]{'
           % tag_name]

  if ordering_rule_list:
    symbols = sorted(original_symbols, Compare)
  else:
    symbols = original_symbols

  _ESCAPE = (u'"', u'\\')
  for symbol in symbols:
    # Escape characters (defined above) have to be escaped by backslashes.
    # e.g.
    #   input : \(・∀・")
    #   output : \u005C\u005C\u0028\u30FB\u2200\u30FB\u005C\u0022\u0029
    # Note that the results of unescape '\u####' by Java compiler are
    # treated as the same as raw characters.
    # Thus;
    # - '\u005c' will be treated as '\' and the next character
    #   will be escaped by '\u005c'.
    # - '\u0022' will be treated as '"' and the string literal
    #   which include '\u0022' will terminate here.
    # They are not what we want so before such characters we place '\'
    # in order to escape them.
    line = ['%s\\u%04x' % ('' if c not in _ESCAPE else '\u005c', ord(c))
            for c in symbol]
    # The white space is quick fix for the backslash at the tail of symbol.
    lines.append('    "%s",  // %s ' % (''.join(line), symbol))
  lines.append('  };')
  return '\n'.join(lines)


def WriteOut(output, tag2symbol, class_name, ordering_rule_list):
  body = [GetStringArrayOfSymbols(tag, symbols, ordering_rule_list)
          for tag, symbols in tag2symbol.iteritems()]
  with io.open(output, 'w', encoding='utf-8') as out_file:
    out_file.write(TEMPLATE_CLASS % (PACKAGE_NAME, class_name, '\n'.join(body)))


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='Input file (.tsv)')
  parser.add_option('--output', dest='output', help='Output file (.java)')
  parser.add_option('--class_name', dest='class_name',
                    help='Generated class\'s name')
  parser.add_option('--ordering_rule', dest='ordering_rule',
                    help='(optional) Ordering rule file')
  parser.add_option('--value_column', dest='value_column', type='int',
                    help='Column number of the values (0-origin)')
  parser.add_option('--category_column', dest='category_column', type='int',
                    help='Column number of the categories (0-origin)')
  parser.add_option('--expand_variant_tag',
                    action='append', default=[], dest='expand_variant_tags',
                    help='(optional, repeatedly definable) Category name'
                    'of which values\' variants are expanded')
  return parser.parse_args()[0]


def CreateOrderingRuleList(file_name):
  ordering_rule_list = []
  for line in io.open(file_name, encoding='utf-8'):
    # Do not forget such line of which content is ' '.
    # Such line has to be appended into the list.
    if not line.startswith(u'# ') and not line.startswith(u'\n'):
      value = line.rstrip(u'\r\n')
      ordering_rule_list.append(value)
  return ordering_rule_list


def main():
  options = ParseOption()
  if not (options.input and options.output and options.class_name and
          options.value_column is not None and
          options.category_column is not None):
    print 'Some options cannot be omitted. See --help.'
    sys.exit(1)
  tag2symbol = ParseSymbolFile(options.input,
                               options.value_column,
                               options.category_column,
                               options.expand_variant_tags)

  if options.ordering_rule is not None:
    ordering_rule_list = CreateOrderingRuleList(options.ordering_rule)
  else:
    ordering_rule_list = []

  WriteOut(options.output, tag2symbol, options.class_name, ordering_rule_list)


if __name__ == '__main__':
  main()
