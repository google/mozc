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

"""A tool to generate segmenter-code from human-readable rule file."""

import codecs
import re
import sys


HEADER = """
namespace  {
constexpr int kLSize = %d;
constexpr int kRSize = %d;

bool IsBoundaryInternal(uint16_t rid, uint16_t lid) {
  // BOS * or * EOS true
  if (rid == 0 || lid == 0) { return true; }"""

FOOTER = """  return true;  // default
}
}   // namespace
"""


def ReadPOSID(id_file, special_pos_file):
  pos = {}
  max_id = 0

  for line in codecs.open(id_file, "r", encoding="utf8"):
    fields = line.split()
    pos[fields[1]] = fields[0]
    max_id = max(int(fields[0]), max_id)

  max_id = max_id + 1
  for line in codecs.open(special_pos_file, "r", encoding="utf8"):
    if len(line) <= 1 or line[0] == "#":
      continue
    fields = line.split()
    pos[fields[0]] = "%d" % max_id
    max_id = max_id + 1

  return pos


def PatternToRegexp(pattern):
  return pattern.replace("*", "[^,]+")


def GetRange(pos, pattern, name):
  if pattern == "*":
    return ""

  pat = re.compile(PatternToRegexp(pattern))
  min_id = -1
  max_id = -1
  keys = list(pos.keys())
  keys.sort()

  id_range = []

  for p in keys:
    id_val = pos[p]
    if pat.match(p):
      if min_id == -1:
        min_id = id_val
        max_id = id_val
      else:
        max_id = id_val
    else:
      if min_id != -1:
        id_range.append([min_id, max_id])
        min_id = -1
  if min_id != -1:
    id_range.append([min_id, max_id])

  tmp = []
  for r in id_range:
    if r[0] == r[1]:
      tmp.append("(%s == %s)" % (name, r[0]))
    else:
      tmp.append("(%s >= %s && %s <= %s)" % (name, r[0], name, r[1]))

  if not tmp:
    print("FATAL: No rule fiind %s" % (pattern))
    sys.exit(-1)

  return " || ".join(tmp)


def main():
  pos = ReadPOSID(sys.argv[1], sys.argv[2])

  out = codecs.getwriter("utf8")(sys.stdout.buffer)
  print(HEADER % (len(list(pos.keys())), len(list(pos.keys()))), file=out)

  for line in codecs.open(sys.argv[3], "r", encoding="utf8"):
    if len(line) <= 1 or line[0] == "#":
      continue
    (l, r, result) = line.split()
    result = result.lower()
    lcond = GetRange(pos, l, "rid") or "true"
    rcond = GetRange(pos, r, "lid") or "true"
    print("  // %s %s %s" % (l, r, result), file=out)
    print(
        "  if ((%s) && (%s)) { return %s; }" % (lcond, rcond, result), file=out
    )

  print(FOOTER, file=out)


if __name__ == "__main__":
  main()
