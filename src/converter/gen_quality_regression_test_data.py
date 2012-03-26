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

"""
A tool to embedded tsv file into test binary for quality regression test.
"""

__author__ = "taku"

import xml.dom.minidom
import sys

def EscapeString(s):
  """ escape the string with "\\xXX" format.

  We don't use encode('string_escape') because it doesn't escape ascii
  characters.

  Args:
    s: a string to be escaped

  Returns:
    an escaped string.
  """
  result = ''
  for c in s:
    hexstr = hex(ord(c))
    # because hexstr contains '0x', remove the prefix and add our prefix
    result += '\\x' + hexstr[2:]
  return result


def ParseTSV(file):
  result = []
  for line in open(file, "r"):
    if line.startswith('#'):
      continue
    line = line.rstrip('\r\n')
    if not line:
      continue
    result.append(line)
  return result


def GetText(node):
  if len(node) >= 1 and node[0].firstChild:
    return node[0].firstChild.nodeValue.strip().replace('\t', ' ')
  else:
    return ''


def ParseXML(file):
  result = []
  dom = xml.dom.minidom.parse(file)
  for issue in dom.getElementsByTagName('issue'):
    status = GetText(issue.getElementsByTagName('status'))
    if status != 'Fixed' and status != 'Verified':
      continue
    id = issue.attributes['id'].value
    target = GetText(issue.getElementsByTagName('target'))
    for detail in issue.getElementsByTagName(u'detail'):
      fields = []
      fields.append('mozcsu_%s' % id)
      for key in ('reading', 'output', 'actionStatus', 'rank', 'accuracy'):
        fields.append(GetText(detail.getElementsByTagName(key)))
      if target:
        fields.append(target)
      result.append(('\t'.join(fields)).encode('utf-8'))
  return result


def ParseFile(file):
  if file.endswith('.xml'):
    return ParseXML(file)
  else:
    return ParseTSV(file)


def GenerateHeader(files):
  try:
    print "#include <cstddef>"  # for NULL
    print "const char *kTestData[] = {"
    for file in files:
      for line in ParseFile(file):
        print " \"%s\"," % EscapeString(line)
    print "NULL,"
    print "};"
  except:
    print "cannot open %s" % (file)
    sys.exit(1)


def main():
  GenerateHeader(sys.argv[1:])

if __name__ == "__main__":
  main()
