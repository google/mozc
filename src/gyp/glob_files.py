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

"""Script to glob files for GYP build.

glob_files.py "*.txt" --exclude "exclude.txt"
glob_files.py "*.cc" --base "absl/**" --exclude "*wrapper.cc" --notest
"""

import argparse
import glob
import os


def _ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument('include', nargs='+')
  parser.add_argument('--base', default='')
  parser.add_argument('--exclude', nargs='+', default=[])
  parser.add_argument('--notest', action='store_true')
  return parser.parse_args()


def main():
  args = _ParseArguments()
  includes = []
  excludes = []
  for include in args.include:
    includes.extend(glob.glob(os.path.join(args.base, include), recursive=True))
  for exclude in args.exclude:
    excludes.extend(glob.glob(os.path.join(args.base, exclude), recursive=True))

  for file in sorted(list(set(includes) - set(excludes))):
    if args.notest:
      basename = os.path.splitext(file)[0]
      if basename.endswith('_test') or basename.endswith('_benchmark'):
        continue
    print(file)


if __name__ == '__main__':
  main()
