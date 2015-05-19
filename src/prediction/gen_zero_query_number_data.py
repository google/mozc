# -*- coding: utf-8 -*-
# Copyright 2010-2013, Google Inc.
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

__author__ = "manabe"

import sys


def EscapeString(result):
  return '"' + result.encode('string_escape') + '"'


def main():
  print "#ifndef MOZC_PREDICTION_ZERO_QUERY_NUM_DATA_H_"
  print "#define MOZC_PREDICTION_ZERO_QUERY_NUM_DATA_H_"
  print "namespace mozc {"
  print "namespace {"

  count = 0
  for line in open(sys.argv[1], "r"):
    if line.startswith("#"):
      continue

    line = line.rstrip("\r\n")
    if line == "":
      continue

    fields = line.split("\t")
    key = fields[0]
    values = [key] + fields[1].split(",")
    print "const char *ZeroQueryNum%d[] = {" % count
    print "  " + ", ".join([EscapeString(s) for s in values] + ["0"])
    print "};"
    count += 1

  print "}  // namespace"
  print "const char **ZeroQueryNum[] = {"
  print "  " + ", ".join(["ZeroQueryNum%d" % c for c in range(count)] + ["0"])
  print "};"
  print "}  // namespace mozc"
  print "#endif  // MOZC_PREDICTION_ZERO_QUERY_NUM_DATA_H_"


if __name__ == "__main__":
  main()
