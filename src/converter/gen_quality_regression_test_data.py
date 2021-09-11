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

from __future__ import absolute_import
from __future__ import print_function

import codecs
import sys
import xml.dom.minidom


_DISABLED = 'false'
_ENABLED = 'true'


def ParseTSV(file):
  for line in codecs.open(file, 'r', encoding='utf-8'):
    if line.startswith('#'):
      continue
    line = line.rstrip('\r\n')
    if not line:
      continue
    yield (_ENABLED, line)


def GetText(node):
  if len(node) >= 1 and node[0].firstChild:
    return node[0].firstChild.nodeValue.strip().replace('\t', ' ')
  else:
    return ''


def ParseXML(file):
  contents = codecs.open(file, 'r', encoding='utf-8').read()
  dom = xml.dom.minidom.parseString(contents)
  for issue in dom.getElementsByTagName('issue'):
    status = GetText(issue.getElementsByTagName('status'))
    enabled = (_DISABLED if status != 'Fixed' and status != 'Verified'
               else _ENABLED)
    id = issue.attributes['id'].value
    target = GetText(issue.getElementsByTagName('target'))
    for detail in issue.getElementsByTagName(u'detail'):
      fields = []
      fields.append('mozcsu_%s' % id)
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


def GenerateHeader(files):
  print('namespace mozc{')
  print('struct TestCase {')
  print('  const bool enabled;')
  print('  const char *tsv;')
  print('} kTestData[] = {')

  for file in files:
    try:
      for enabled, line in ParseFile(file):
        # Escape characters: \ and " to \\ and \"
        line = line.replace('\\', '\\\\').replace('"', '\\"')
        print(' {%s, "%s"},' % (enabled, line))
    except Exception as e:  # pylint: disable=broad-except
      print('cannot open %s: %s' % (file, e))
      sys.exit(1)

  print('  {false, nullptr},')
  print('};')
  print('}  // namespace mozc')


def main():
  GenerateHeader(sys.argv[1:])

if __name__ == '__main__':
  main()
