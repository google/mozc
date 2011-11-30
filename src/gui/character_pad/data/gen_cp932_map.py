# -*- coding: utf-8 -*-
# Copyright 2010-2011, Google Inc.
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

__author__ = "taku"

import re
import sys
import string

kUnicodePat = re.compile(r'0x[0-9A-Fa-f]{2,4}')
def IsValidUnicode(n):
  return kUnicodePat.match(n)

def main():
  fh = open(sys.argv[1])
  result = {}
  for line in fh.readlines():
    if line[0] is '#':
      continue
    array = string.split(line)
    sjis = array[0]
    ucs2 = array[1]
    if eval(sjis) < 32 or not IsValidUnicode(ucs2):
      continue
    result.setdefault(ucs2, sjis)

  keys = sorted(result.keys())

  print "struct CP932MapData {"
  print "  unsigned int ucs4;"
  print "  unsigned short int sjis;"
  print "};"
  print ""
  print "static const size_t kCP932MapDataSize = %d;" % (len(keys))
  print "static const CP932MapData kCP932MapData[] = {"
  for n in keys:
    print "  { %s, %s }," % (n ,result[n])
  print "  { 0, 0 }";
  print "};"

if __name__ == "__main__":
  main()
