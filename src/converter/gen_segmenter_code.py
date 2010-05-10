# Copyright 2010, Google Inc.
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
A tool to generate segmenter-code from human-readable rule file
"""

__author__ = "taku"

import sys
import re

HEADER="""
namespace  {
const int kLSize = %d;
const int kRSize = %d;

bool IsBoundaryInternal(uint16 rid, uint16 lid) {
  // BOS * or * EOS true
  if (rid == 0 || lid == 0) { return true; }"""

FOOTER="""  return true;  // default
}
}   // namespace
"""
def ReadPOSID(file):
  pos = {}
  try:
    for line in open(file, "r"):
      fields = line.split()
      pos[fields[1]] = fields[0]
    return pos
  except:
    print "cannot open %s" % (file)
    sys.exit(1)

def PatternToRegexp(pattern):
  return pattern.replace("*", "[^,]+")

def GetRange(pos, pattern, name):
  if pattern == "*":
    return ""

  pat = re.compile(PatternToRegexp(pattern))
  min = -1;
  max = -1;
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
      tmp.append("(%s == %s)" % (name, r[0]))
    else:
      tmp.append("(%s >= %s && %s <= %s)" % (name, r[0], name, r[1]))

  if len(tmp) == 0:
    print "FATAL: No rule fiind %s" % (pattern)
    sys.exit(-1)

  return " || ".join(tmp)

def main():
  pos = ReadPOSID(sys.argv[1])

  print HEADER % (len(pos.keys()), len(pos.keys()))

  for line in open(sys.argv[2], "r"):
    if len(line) <= 1 or line[0] == '#':
      continue
    (l, r, result) = line.split()
    result = result.lower()
    lcond = GetRange(pos, l, "rid");
    rcond = GetRange(pos, r, "lid");
    print "  // %s %s %s" % (l, r, result)
    if lcond == "" and rcond != "":
      print "  if (%s) { return %s; }" % (rcond, result)
    elif lcond != "" and rcond == "":
      print "  if (%s) { return %s; }" % (lcond, result)
    elif lcond != "" and rcond != "":
      print "  if ((%s) && (%s)) { return %s; }" % (lcond, rcond, result)
    else:
      print "  return %s;" % (result)

  print FOOTER

if __name__ == "__main__":
  main()
