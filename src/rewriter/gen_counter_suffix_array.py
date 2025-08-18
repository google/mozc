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

"""Generate embedded counter suffix data."""

import argparse
import codecs

from build_tools import code_generator_util
from build_tools import serialized_string_array_builder


def ReadCounterSuffixPosIds(id_file):
  pos_ids = set()
  with codecs.open(id_file, 'r', encoding='utf-8') as stream:
    stream = code_generator_util.ParseColumnStream(stream, num_column=2)
    for pos_id, pos_name in stream:
      if pos_name.startswith('名詞,接尾,助数詞'):
        pos_ids.add(pos_id)
  return pos_ids


def ReadCounterSuffixes(dictionary_files, ids):
  suffixes = set()
  for filename in dictionary_files:
    # TODO(noriyukit): reading_correction.tsv is passed in the list of input
    # files due to the restriction of build system; see also the comment in
    # dictionary/gen_system_dictionary_data_main.cc.  We should handle
    # reading_correction.tsv in a cleaner way in the whole build system.
    if 'reading_correction.tsv' in filename:
      continue
    with codecs.open(filename, 'r', encoding='utf-8') as stream:
      stream = code_generator_util.ParseColumnStream(
          stream, num_column=5, delimiter='\t'
      )
      for _, lid, rid, _, value in stream:
        if (lid == rid) and (lid in ids) and (rid in ids):
          suffixes.add(value)
  return sorted(s.encode('utf-8') for s in suffixes)


def ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument('dictionary_files', nargs='+')
  parser.add_argument('--id_file')
  parser.add_argument('--output')
  parser.add_argument('--text', action='store_true', default=False)
  return parser.parse_args()


def main():
  args = ParseArguments()
  ids = ReadCounterSuffixPosIds(args.id_file)
  suffixes = ReadCounterSuffixes(args.dictionary_files, ids)
  if args.text:
    with open(args.output, 'wb') as f:
      f.write(b'\n'.join(suffixes))
  else:
    serialized_string_array_builder.SerializeToFile(suffixes, args.output)


if __name__ == '__main__':
  main()
