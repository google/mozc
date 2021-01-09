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

"""A tool to generate test sentences for stress test."""

from __future__ import print_function

import codecs
import sys
import six


def escape_string(s):
  r"""escape the string with "\\xXX" format.

  We don't use encode('string_escape') because it doesn't escape ascii
  characters.

  Args:
    s: a string to be escaped

  Returns:
    an escaped string.
  """
  if six.PY3:
    return ''.join(r'\x%02x' % b for b in s.encode('utf-8'))

  result = ''
  for c in s:
    hexstr = hex(ord(c))
    # because hexstr contains '0x', remove the prefix and add our prefix
    result += '\\x' + hexstr[2:]
  return result


def OpenFile(filename):
  if six.PY2:
    return open(filename, 'r')
  else:
    return codecs.open(filename, 'r', encoding='utf-8')


def GenerateHeader(file):
  try:
    print('const char *kTestSentences[] = {')
    for line in OpenFile(file):
      if line.startswith('#'):
        continue
      line = line.rstrip('\r\n')
      if not line:
        continue
      print(' "%s",' % escape_string(line))
    print('};')
  except Exception as e:
    print('cannot open %s' % file)
    raise e


def main():
  GenerateHeader(sys.argv[1])

if __name__ == '__main__':
  main()
