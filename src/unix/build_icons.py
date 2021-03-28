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

"""Script to rename icons files and pack them into a zip file.

build_icon.py --inputs ui-tool.png hiragana.svg ... --output icons.zip
"""

import argparse
import os
import shutil
import tempfile

RENAME_RULES = {
    'product_icon_32bpp-128.png': 'mozc.png',
    'full_ascii.svg': 'alpha_full.svg',
    'full_katakana.svg': 'katakana_full.svg',
    'half_ascii.svg': 'alpha_half.svg',
    'half_katakana.svg': 'katakana_half.svg',
    'ui-dictionary.png': 'dictionary.png',
    'ui-properties.png': 'properties.png',
    'ui-tool.png': 'tool.png',
}


def ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument('--inputs', nargs='+')
  parser.add_argument('--output')
  return parser.parse_args()


def GetCopyName(src) -> str:
  basename = os.path.basename(src)
  return RENAME_RULES.get(basename, basename)


def main():
  args = ParseArguments()
  with tempfile.TemporaryDirectory() as tmp_dir:
    for src in args.inputs:
      shutil.copyfile(src, os.path.join(tmp_dir, GetCopyName(src)))
    basename = os.path.splitext(args.output)[0]
    shutil.make_archive(basename, format='zip', root_dir=tmp_dir)


if __name__ == '__main__':
  main()
