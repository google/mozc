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

"""A helper script to extract native libraries from an *.apk file."""

import argparse
import pathlib
import zipfile


def extract_native_libs(apk: pathlib.Path, dest: pathlib.Path) -> None:
  """Extract native libraries from an APK.

  Args:
    apk: APK file from which native libraries will be extracted.
    dest: ZIP file to which native libraries will be extracted.
  """
  with zipfile.ZipFile(apk) as z:
    with zipfile.ZipFile(dest, mode='w') as output:
      for info in z.infolist():
        paths = info.filename.split('/')
        if '..' in paths:
          continue
        if len(paths) < 1:
          continue
        if paths[0] != 'lib':
          continue
        output.writestr(info, z.read(info))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--input', help='A path to APK', type=str)
  parser.add_argument('--output', help='A path to output ZIP file', type=str)
  args = parser.parse_args()

  extract_native_libs(pathlib.Path(args.input), pathlib.Path(args.output))


if __name__ == '__main__':
  main()
