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

"""Create a zip file from the input files.

If an input file is a zip file, the zip file is extracted.

% python3 zip_file.py --inputs a.zip b.txt --output c.zip

In the above case, c.zip contains extracted files of a.zip, but not a.zip.
"""

import argparse
import os
import shutil
import tempfile
import zipfile


def ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument('--inputs', nargs='+')
  parser.add_argument('--output')
  return parser.parse_args()


def ExtractZip(zip_path, out_dir):
  with zipfile.ZipFile(zip_path) as zip_file:
    zip_file.extractall(path=out_dir)


def CopyFiles(input_files, out_dir):
  for src in input_files:
    if src.endswith('.zip'):
      ExtractZip(src, out_dir)
    else:
      shutil.copyfile(src, os.path.join(out_dir, os.path.basename(src)))


def main():
  args = ParseArguments()

  with tempfile.TemporaryDirectory() as tmp_dir:
    out_path = 'installer'
    out_dir = os.path.join(tmp_dir, out_path)
    os.mkdir(out_dir)

    CopyFiles(args.inputs, out_dir)
    basename = os.path.splitext(args.output)[0]
    shutil.make_archive(basename, format='zip', root_dir=tmp_dir)


if __name__ == '__main__':
  main()
