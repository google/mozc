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

"""A tool to embedded tsv file into test binary for quality regression test."""

import argparse
import codecs
from collections.abc import Iterable
import logging
import sys
from typing import TextIO
import xml.dom.minidom


_DISABLED = 'false'
_ENABLED = 'true'


def ParseArgs() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      'Embed TSV files into test binary for the quality regressin test.'
  )
  parser.add_argument(
      '--output',
      help='Output file',
      type=argparse.FileType('w', encoding='utf-8'),
      default=sys.stdout,
  )
  parser.add_argument('files', nargs='+')
  return parser.parse_args()


def ParseTSV(file):
  r"""Parses TSV file for quality regression test.

  Args:
    file: TSV file path
    TSV Format: <label> <key> <expected_value> <command> <expected_rank> \
    <accuracy> <platform>

    You can disable test entries by adding the prefix, 'DISABLED_', to the label

  Yields:
    (is_enabled, test_tsv)
    Actual TSV parsing will be done in C++.
  """
  for line in codecs.open(file, 'r', encoding='utf-8'):
    if line.startswith('#'):
      continue
    line = line.rstrip('\r\n')
    if not line:
      continue
    if line.startswith('DISABLED_'):
      yield (_DISABLED, line[len('DISABLED_') :])
    else:
      yield (_ENABLED, line)


def GetText(node):
  if len(node) >= 1 and node[0].firstChild:
    return node[0].firstChild.nodeValue.strip().replace('\t', ' ')  # pytype: disable=attribute-error
  else:
    return ''


def ParseXML(file):
  """Parses mozcsu XML file."""
  contents = codecs.open(file, 'r', encoding='utf-8').read()
  dom = xml.dom.minidom.parseString(contents)
  for issue in dom.getElementsByTagName('issue'):
    status = GetText(issue.getElementsByTagName('status'))
    enabled = (
        _DISABLED if status != 'Fixed' and status != 'Verified' else _ENABLED
    )
    id_ = issue.attributes['id'].value
    target = GetText(issue.getElementsByTagName('target'))
    for detail in issue.getElementsByTagName('detail'):
      fields = []
      fields.append('mozcsu_%s' % id_)
      for key in ('reading', 'output', 'actionStatus', 'rank', 'accuracy'):
        fields.append(GetText(detail.getElementsByTagName(key)))
      if target:
        fields.append(target)
      tsv_line = '\t'.join(fields)
      yield (enabled, tsv_line)


def ParseFile(file):
  if file.endswith('.xml'):
    return ParseXML(file)
  else:
    return ParseTSV(file)


def GenerateHeader(out: TextIO, files: Iterable[str]) -> None:
  """Generates a C++ file from files.

  Args:
    out: output file object
    files: list of input file paths.
  """
  out.write('namespace mozc{\n')
  out.write('struct TestCase {\n')
  out.write('  const bool enabled;\n')
  out.write('  const char *tsv;\n')
  out.write('} kTestData[] = {\n')

  for file in files:
    try:
      for enabled, line in ParseFile(file):
        # Escape characters: \ and " to \\ and \"
        line = line.replace('\\', '\\\\').replace('"', '\\"')
        out.write(' {%s, "%s"},\n' % (enabled, line))
    except Exception as e:  # pylint: disable=broad-except
      logging.exception('cannot open %r: %r', file, e)
      sys.exit(1)

  out.write('  {false, nullptr},\n')
  out.write('};\n')
  out.write('}  // namespace mozc\n')


def main() -> None:
  args = ParseArgs()
  GenerateHeader(args.output, args.files)


if __name__ == '__main__':
  main()
