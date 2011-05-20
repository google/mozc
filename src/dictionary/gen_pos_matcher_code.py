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

"""A tool to generate POS matcher."""

__author__ = "taku"

import re
import sys


def ReadPOSID(id_file, special_pos_file):
  pos = {}
  max_id = 0

  for line in open(id_file, "r"):
    fields = line.split()
    pos[fields[1]] = fields[0]
    max_id = max(int(fields[0]), max_id)

  max_id += 1
  for line in open(special_pos_file, "r"):
    if len(line) <= 1 or line[0] == "#":
      continue
    fields = line.split()
    pos[fields[0]] = ("%d" % max_id)
    max_id += 1

  return pos


def PatternToRegexp(pattern):
  return pattern.replace("*", "[^,]+")


def GetRange(pos, pattern):
  if pattern == "*":
    return ""

  pat = re.compile(PatternToRegexp(pattern))
  min = -1
  max = -1
  keys = pos.keys()
  keys.sort()

  range = []

  for p in keys:
    id = pos[p]
    if pat.match(p):
      if min == -1:
        min = id
        max = id
      else:
        max = id
    else:
      if min != -1:
        range.append([min, max])
        min = -1
  if min != -1:
    range.append([min, max])

  tmp = []
  for r in range:
    if r[0] == r[1]:
      tmp.append("(id == %s)" % (r[0]))
    else:
      tmp.append("(id >= %s && id <= %s)" % (r[0], r[1]))

  if not tmp:
    print "FATAL: No rule fiind %s" % (pattern)
    sys.exit(-1)

  return (range[0][0], " || ".join(tmp))


def main():
  pos = ReadPOSID(sys.argv[1], sys.argv[2])
  print "#include \"base/base.h\""
  print "namespace mozc {"
  print "namespace {"
  print "class POSMatcher {"
  print " public:"

  for line in open(sys.argv[3], "r"):
    if len(line) <= 1 or line[0] == "#":
      continue
    (func, pattern) = line.split()
    (init_id, cond) = GetRange(pos, pattern)
    print "  // %s \"%s\"" % (func, pattern)
    print "  static uint16 Get%sId() {" % (func)
    print "    return %s;" % (init_id)
    print "  }"
    print "  static bool Is%s(uint16 id) {" % (func)
    print "    return (%s);" % (cond)
    print "  }"

  print " private:"
  print "  POSMatcher() {}"
  print "  ~POSMatcher() {}"
  print "};"
  print "}  // namespace"
  print "}  // mozc"

if __name__ == "__main__":
  main()
