# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
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

import string
import re
import sys

kUnicodePat = re.compile(r'[0-9A-Fa-f]{2,4}')

def IsValidUnicode(n):
  return kUnicodePat.match(n)

def LoadJISX0201(filename):
  fh = open(filename)
  result = []
  for line in fh.readlines():
    if line[0] is '#':
      continue
    array = string.split(line)
    jis = array[0].replace('0x', '')
    ucs2 = array[1].replace('0x', '')
    if len(jis) == 2:
      jis = "00" + jis

    if IsValidUnicode(ucs2):
      result.append([jis, ucs2])

  return ["JISX0201", result]

def LoadJISX0208(filename):
  fh = open(filename)
  result = []
  for line in fh.readlines():
    if line[0] is '#':
      continue
    array = line.split()
    jis = array[1].replace('0x', '')
    ucs2 = array[2].replace('0x', '')
    if IsValidUnicode(ucs2):
      result.append([jis, ucs2])

  return ["JISX0208", result]

def LoadJISX0212(filename):
  fh = open(filename)
  result = []
  for line in fh.readlines():
    if line[0] is '#':
      continue
    array = line.split()
    jis = array[0].replace('0x', '')
    ucs2 = array[1].replace('0x', '')
    if IsValidUnicode(ucs2):
      result.append([jis, ucs2])

  return ["JISX0212", result]

def LoadCP932(filename):
  fh = open(filename)
  result = []
  for line in fh.readlines():
    if line[0] is '#':
      continue
    array = line.split()
    sjis = array[0].replace('0x', '')
    ucs2 = array[1].replace('0x', '')
    if int(sjis, 16) < 32:
      continue
    if len(sjis) == 2:
      sjis = "00" + sjis

    if IsValidUnicode(ucs2):
      result.append([sjis, ucs2])

  return ["CP932", result]

def Output(arg):
  name = arg[0]
  result = arg[1]
  print "static const size_t k%sMapSize = %d;" % (name, len(result))
  print "static const mozc::gui::CharacterPalette::LocalCharacterMap k%sMap[] = {" % (name)
  for n in result:
    print "  { 0x%s, 0x%s }," % (n[0] ,n[1])
  print "  { 0, 0 }";
  print "};"
  print ""

if __name__ == "__main__":
  Output(LoadJISX0201(sys.argv[1]))
  Output(LoadJISX0208(sys.argv[2]))
  Output(LoadJISX0212(sys.argv[3]))
  Output(LoadCP932(sys.argv[4]))
