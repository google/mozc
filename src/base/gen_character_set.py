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

kUnicodePat = re.compile(r'[0-9A-Fa-f]{2,4}')

def IsValidUCS2(n):
  try:
    return 0 <= int(n, 16) <= 0xFFFF
  except ValueError:
    return False

def IsValidUCS4(n):
  try:
    return 0 <= int(n, 16) <= 0x7FFFFFFF
  except ValueError:
    return False

def LoadJISX0201(filename):
  fh = open(filename)
  result = set()
  for line in fh.readlines():
    if line.startswith('#'):
      continue
    array = string.split(line)
    ucs2 = array[1].replace('0x', '')
    if IsValidUCS2(ucs2):
      result.add(ucs2)

  return result

def LoadJISX0208(filename):
  fh = open(filename)
  result = set()
  for line in fh.readlines():
    if line.startswith('#'):
      continue
    array = line.split()
    ucs2 = array[2].replace('0x', '')
    if IsValidUCS2(ucs2):
      result.add(ucs2)

  # FF3C (FULLWIDTH REVERSE SOLIDS) should be in JISX0208
  result.add('FF3C')

  # FF0D (FULLWIDTH HYPHEN MINUS) should be in JISX0208
  result.add('FF0D')

  return result

def LoadJISX0212(filename):
  fh = open(filename)
  result = set()
  for line in fh.readlines():
    if line.startswith('#'):
      continue
    array = line.split()
    ucs2 = array[1].replace('0x', '')
    if IsValidUCS2(ucs2):
      result.add(ucs2)

  return result

def LoadCP932(filename):
  fh = open(filename)
  result = set()
  for line in fh.readlines():
    if line.startswith('#'):
      continue
    array = line.split()
    ucs2 = array[1].replace('0x', '')
    if IsValidUCS2(ucs2):
      result.add(ucs2)

  return result

def LoadJISX0213(filename):
  fh = open(filename)
  result = set()
  for line in fh.readlines():
    if line.startswith('#'):
      continue
    array = line.split()
    # Some JIS X 0213 characters are described as 'U+xxxx+xxxx'.
    ucs4 = array[1].replace('U+', '').split('+')[0]
    if IsValidUCS4(ucs4):
      result.add(ucs4)

  return result

# The following chars have different mapping in
# Windows and Mac. Technically, they are platform dependent
# characters, but Mozc treat them so that they are normal characters
# defined in JISX0208
def LoadExceptions():
  # treat Unicode Japanese incompatible characters as JISX0208.
  result = set()
  result.add('00A5')  # YEN SIGN
  result.add('003E')  # OVERLINE
  result.add('301C')  # WAVE DASH
  result.add('FF5E')  # FULL WIDTH TILDE
  result.add('2016')  # DOUBLE VERTICAL LINE
  result.add('2225')  # PARALEL TO
  result.add('2212')  # MINUS SIGN
  result.add('FF0D')  # FULL WIDTH HYPHEN MINUS
  result.add('00A2')  # CENT SIGN
  result.add('FFE0')  # FULL WIDTH CENT SIGN
  result.add('00A3')  # POUND SIGN
  result.add('FFE1')  # FULL WIDTH POUND SIGN
  result.add('00AC')  # NOT SIGN
  result.add('FFE2')  # FULL WIDTH NOT SIGN
  return result

def Lookup(key, hash):
  if key in hash:
    return "D"
  else:
    return "N"

def Categorize(key, pattern):
  # ASCII
  if int(key, 16) <= 0x007F:
    return "ASCII"

  # "CP932 JISX0201 JISX0208 JISX0212 JISX0213" and "D or N"
  # regexp => result mapping
  kMapping = ( [ "D . . . . .",  "JISX0208" ],   # vender specific
               [ ". N N N N N",  "UNICODE_ONLY"  ],  # not in JIS nor CP932
               [ "N D N N N N",  "CP932"    ],       # only CP932
               [ "N . D . . .",  "JISX0201" ],       # at least in JISX0201
               [ "N . N D . .",  "JISX0208" ],       # at least in JISX0208
               [ "N . N N D .",  "JISX0212" ],       # in JISX0212
               [ "N . N N N D",  "JISX0213" ] )      # in JISX0213

  for m in kMapping:
    if re.search(m[0], pattern):
      return m[1]

  raise 'Cannot find pattern %s ' % (pattern)

def OutputTable():
  charset = { 'exceptions' : LoadExceptions(),
              'cp932'      : LoadCP932(sys.argv[1]),
              'jisx0201'   : LoadJISX0201(sys.argv[2]),
              'jisx0208'   : LoadJISX0208(sys.argv[3]),
              'jisx0212'   : LoadJISX0212(sys.argv[4]),
              'jisx0213'   : LoadJISX0213(sys.argv[5])}

  max_char = 0
  for chars in charset.values():
    max_char = max(max_char, max([int(c, 16) for c in chars]))

  char_range = xrange(0, max_char + 1)

  cat = []
  for i in char_range:
    key = ("%5.4X" % (i)).lstrip(' ')
    pattern = "%s %s %s %s %s %s" % (
        Lookup(key, charset['exceptions']),
        Lookup(key, charset['cp932']),
        Lookup(key, charset['jisx0201']),
        Lookup(key, charset['jisx0208']),
        Lookup(key, charset['jisx0212']),
        Lookup(key, charset['jisx0213']))
    cat.append(Categorize(key, pattern))

  # Grouping
  prev = ""
  ucs4s = []
  group = []
  for i in char_range:
    if prev != cat[i]:
      if len(ucs4s) > 0:
        group.append([prev, ucs4s])
      prev = cat[i]
      ucs4s = []
    ucs4s.append(i)

  if len(ucs4s) > 0:
    group.append([prev, ucs4s])

  print "Util::CharacterSet Util::GetCharacterSet(char32 ucs4) {"
  print "  switch (ucs4) {";
  for g in group:
    cat = g[0]
    chars = g[1]
    if cat == "UNICODE_ONLY":
      continue
    for i in chars:
      print  "    case 0x%8.8X:" % (i)
    print "      return %s;" % (cat)
    print "      break;";

  print "    default:";
  print "      return UNICODE_ONLY;"
  print "      break;";
  print "  }";   # end switch
  print "  return UNICODE_ONLY;"
  print "}";     # end function

def main():
  print "// This file is generated by base/gen_character_set.py"
  print "// Do not edit me!";
  OutputTable()

if __name__ == "__main__":
  main()
