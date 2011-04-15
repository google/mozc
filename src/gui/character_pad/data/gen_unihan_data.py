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
import string
import sys
rs = {}

def Escape(n):
  if n is not "NULL":
    return "\"%s\"" % (n)
  else:
    return "NULL"

def GetCode(n):
  if n is not "NULL":
    n = string.replace(n, '0-', 'JIS X 0208: 0x')
    n = string.replace(n, '1-', 'JIS X 0212: 0x')
    n = string.replace(n, '3-', 'JIS X 0213: 0x')
    n = string.replace(n, '4-', 'JIS X 0213: 0x')
    n = string.replace(n, 'A-', 'Vendors Ideographs: 0x')
    n = string.replace(n, '3A', 'JIS X 0213 2000: 0x')
    return "\"%s\"" % n
  else:
    return "NULL"

def GetRadical(n):
  pat = re.compile(r'^(\d+)\.')
  if n is not "NULL":
    m = pat.match(n)
    if m:
      result = rs[m.group(1)]
      return  "\"%s\"" % (result.encode('string_escape'))
    else:
      return "NULL"
  else:
    return "NULL"

def main():
  fh = open(sys.argv[1])
  for line in fh.readlines():
    array = line.split()
    n = array[0]
    id = array[1]
    radical = array[2]
    rs[id] = radical

  dic = {}
  pat = re.compile(r'^U\+(\S+)\s+(kTotalStrokes|kJapaneseKun|kJapaneseOn|kRSUnicode|kIRG_JSource)\t(.+)')
  fh = open(sys.argv[2])
  for line in fh.readlines():
    m = pat.match(line)
    if m:
      field = m.group(2)
      value = m.group(3)
      key = m.group(1)
      n = int(m.group(1), 16)
      if n <= 65536:
        dic.setdefault(key, {}).setdefault(field, value)

  keys = sorted(dic.keys())

  print "struct UnihanData {";
  print "  unsigned short int ucs2;";
# Since the total strokes defined in Unihan data is Chinese-based
# number, we can't use it.
#  print "  unsigned char total_strokes;";
  print "  const char *japanese_kun;";
  print "  const char *japanese_on;";
# Since the radical information defined in Unihan data is Chinese-based
# number, we can't use it.
#  print "  const char *radical;";
  print "  const char *IRG_jsource;";
  print "};"
  print "static const size_t kUnihanDataSize = %d;" % (len(keys))
  print "static const UnihanData kUnihanData[] = {"

  for key in keys:
    total_strokes = dic[key].get("kTotalStrokes", "0")
    kun = Escape(dic[key].get("kJapaneseKun", "NULL"))
    on =  Escape(dic[key].get("kJapaneseOn", "NULL"))
    rad = GetRadical(dic[key].get("kRSUnicode", "NULL"))
    code = GetCode(dic[key].get("kIRG_JSource", "NULL"))
#    print " { 0x%s, %s, %s, %s, %s, %s }," % (key, total_strokes, kun, on, rad, code)
    print " { 0x%s, %s, %s, %s }," % (key, kun, on, code)

  print "};"

if __name__ == "__main__":
  main()
