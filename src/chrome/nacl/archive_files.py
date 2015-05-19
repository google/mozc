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

"""A utility command to make zip file and tgz file.

When the version is 1.2.3.4, the following command creats output.zip and
output.tgz which have these three files.
  hoge-fuga-1.2.3.4/foo1
  hoge-fuga-1.2.3.4/foo2
  hoge-fuga-1.2.3.4/bar/piyo

Example usage:
  python archive_files.py \
    --base_path=/path/to/base_path/ \
    --zip_output=/path/to/output.zip \
    --tgz_output=/path/to/output.tgz \
    --version_file=../../mozc_version.txt \
    --top_dir_base=hoge-fuga
    /path/to/base_path/foo1 \
    /path/to/base_path/foo2 \
    /path/to/base_path/bar/piyo
"""

import optparse
import os
import tarfile
import zipfile

from build_tools import mozc_version


def main():
  parser = optparse.OptionParser(usage='Usage: %prog [options] files...')
  parser.add_option('--base_path', dest='base_path', action='store',
                    type='string', default='')
  parser.add_option('--zip_output', dest='zip_output', action='store',
                    type='string', default='')
  parser.add_option('--tgz_output', dest='tgz_output', action='store',
                    type='string', default='')
  parser.add_option('--version_file', dest='version_file', action='store',
                    type='string', default='')
  parser.add_option('--top_dir_base', dest='top_dir_base', action='store',
                    type='string', default='')
  (options, args) = parser.parse_args()

  assert options.base_path, 'No --base_path was specified.'
  assert options.zip_output, 'No --zip_output was specified.'
  assert options.tgz_output, 'No --tgz_output was specified.'
  assert options.version_file, 'No --version_file was specified.'
  assert options.top_dir_base, 'No --top_dir_base was specified.'

  version = mozc_version.MozcVersion(options.version_file)
  top_dir_name = options.top_dir_base + '-%s' % version.GetVersionString()

  zip_file = zipfile.ZipFile(options.zip_output, 'w', zipfile.ZIP_DEFLATED)
  tgz_file = tarfile.open(options.tgz_output, 'w:gz')

  for file_name in args:
    if not file_name.startswith(options.base_path):
      raise RuntimeError(file_name + ' is not in ' + options.base_path)
    arc_name = os.path.join(top_dir_name,
                            os.path.relpath(file_name, options.base_path))
    zip_file.write(file_name, arc_name)
    tgz_file.add(file_name, arc_name)

  zip_file.close()
  tgz_file.close()


if __name__ == '__main__':
  main()
