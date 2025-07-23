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

"""Build a package for macOS (.pkg file).

This script creates a .pkg file with pkgbuild and productbuild
"""

import argparse
import os
import shutil
import tempfile

from build_tools import util


def ParseArguments():
  """Parses command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--input')
  parser.add_argument('--output')
  parser.add_argument('--oss', action='store_true')
  return parser.parse_args()


def main():
  args = ParseArguments()

  if args.oss:
    identifier = 'org.mozc.pkg.JapaneseInput'
  else:
    identifier = 'com.google.pkg.GoogleJapaneseInput'

  output_path = os.path.abspath(args.output)

  with tempfile.TemporaryDirectory() as tmp_dir:
    # Use the unzip command to extract symbolic links properly.
    util.RunOrDie(['unzip', '-q', args.input, '-d', tmp_dir])
    os.chdir(os.path.join(tmp_dir, 'installer'))
    pkgbuild_commands = [
        'pkgbuild',
        '--root', 'root',
        '--identifier', identifier,
        '--scripts', 'scripts/',
        'Mozc.pkg',  # the name "Mozc.pkg" is configured in distribution.xml.
    ]
    util.RunOrDie(pkgbuild_commands)
    productbuild_commands = [
        'productbuild',
        '--distribution', 'distribution.xml',
        '--plugins', 'Plugins/',
        '--resources', 'Resources/',
        'package.pkg',  # this name is only used within this script.
    ]
    util.RunOrDie(productbuild_commands)
    shutil.copyfile('package.pkg', output_path)


if __name__ == '__main__':
  main()
