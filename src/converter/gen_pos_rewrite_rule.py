# -*- coding: utf-8 -*-
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

__author__ = "taku"

import sys

def IsPrefix(str, key):
  try:
    n = str.index(key)
  except:
    n = -1

  if n == 0:
    return True
  else:
    return False

def LoadRewriteMapRule(filename):
  fh = open(filename)
  rule = []
  for line in fh.readlines():
    line = line.rstrip("\n")
    if not line or line.startswith('#'):
      continue
    fields = line.split();
    rule.append([fields[0], fields[1]])
  return rule

def main():
  # read rule file
  rules = LoadRewriteMapRule(sys.argv[2])
  current_id = 1

  # read lid file
  lid = 0
  id_map = {}
  ids = []

  for line in open(sys.argv[1], "r"):
    fields = line.split()
    target = fields[1]
    assert(int(fields[0]) == lid)
    lid = lid + 1;
    id = 0
    for rule in rules:
      if IsPrefix(target, rule[0]):
        if id_map.has_key(rule[1]):
          id = id_map[rule[1]]
        else:
          id = current_id
          id_map[rule[1]] = current_id
          current_id = current_id + 1
        pass
    ids.append(str(id))

  # add default id for Zip code
  ids.append("0")

  print "const uint8 kLidGroup[] = {"
  print ',\n'.join(ids)
  print "};"

if __name__ == "__main__":
  main()
