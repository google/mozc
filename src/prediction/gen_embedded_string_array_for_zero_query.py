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

"""Generate header file of a string array for zero query suggestion.

Usage:
gen_embedded_string_array_for_zero_query.py --input input.def \
 --output /path/to/output/zero_query_hoge.h --var_name ZeroQueryHoge

Input format:
<key> <TAB> <candidate_1>,<candidate_2>,..,<candidate_n>
...
For more details, please refer to definition files under mozc/data/zero_query/

Output format:
const char *Var0[] = {"Key0", "Cand00", "Cand01", .., 0};
const char *Var1[] = {"Key1", "Cand10", "Cand11", .., 0};

const char **Var[] = {Var0, Var1, .., VarN};

Here, (Cand00, Cand10, ...) is sorted so that we can use binary search.
"""

__author__ = "toshiyuki"

import optparse
import os


_MOZC_DIR_FOR_DEFINE_GUARD = 'MOZC'


def EscapeString(string):
  """Escapes string."""
  return '"' + string.encode('string_escape') + '"'


def GetDefineGuardSymbol(file_name):
  """Returns define guard symbol for .h file.

  For example, returns 'SOME_EXAMPLE_H' for '/path/to/some_example.h'

  Args:
    file_name: a string indicating output file path.
  Returns:
    A string for define guard.
  """
  return os.path.basename(file_name).upper().replace('.', '_')


def GetDefineGuardHeaderLines(output_file_name):
  """Returns define guard header for .h file."""
  result = []
  result.append(
      '#ifndef %s_PREDICTION_%s_' %(_MOZC_DIR_FOR_DEFINE_GUARD,
                                    GetDefineGuardSymbol(output_file_name)))
  result.append(
      '#define %s_PREDICTION_%s_' %(_MOZC_DIR_FOR_DEFINE_GUARD,
                                    GetDefineGuardSymbol(output_file_name)))
  return result


def GetDefineGuardFooterLines(output_file_name):
  """Returns define guard footer for .h file."""
  return [
      '#endif  // %s_PREDICTION_%s_' %(_MOZC_DIR_FOR_DEFINE_GUARD,
                                       GetDefineGuardSymbol(output_file_name))]


def GetZeroQueryRules(input_file_name):
  """Returns zero query trigerring rules. The list is sorted by key."""
  rules = []
  with open(input_file_name, 'r') as input_file:
    for line in input_file:
      if line.startswith('#'):
        continue
      line = line.rstrip('\r\n')
      if not line:
        continue

      tokens = line.split('\t')
      key = tokens[0]
      values = tokens[1].split(',')

      rules.append((key, values))
  rules.sort(lambda x, y: cmp(x[0], y[0]))  # For binary search
  return rules


def GetHeaderContents(input_file_name, var_name, output_file_name):
  """Returns contents for header file that contains a string array."""
  zero_query_rules = GetZeroQueryRules(input_file_name)

  result = []
  result.extend(GetDefineGuardHeaderLines(output_file_name))
  result.append('namespace mozc {')
  result.append('namespace {')

  for i, rule in enumerate(zero_query_rules):
    result.append('const char *%s%d[] = {' % (var_name, i))
    result.append('  ' + ', '.join(
        [EscapeString(s) for s in [rule[0]] + rule[1]] + ['0']))
    result.append('};')

  result.append('} // namespace')

  result.append('const char **%s_data[] = {' % var_name)
  result.append('  ' + ', '.join(
      ['%s%d' % (var_name, c) for c in range(len(zero_query_rules))]))
  result.append('};')
  result.append(
      'const size_t %s_size = %d;' % (var_name, len(zero_query_rules)))

  result.append('} // namespace mozc')
  result.extend(GetDefineGuardFooterLines(output_file_name))
  return result


def ParseOption():
  """Parses command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--input', dest='input', help='Input file path')
  parser.add_option('--output', dest='output', help='Output file path')
  parser.add_option(
      '--var_name', dest='var_name', help='Var name for the array')
  return parser.parse_args()[0]


def main():
  options = ParseOption()
  lines = GetHeaderContents(options.input, options.var_name, options.output)
  with open(options.output, 'w') as out_file:
    out_file.write('\n'.join(lines))


if __name__ == '__main__':
  main()
