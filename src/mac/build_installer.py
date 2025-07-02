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

"""Build an installer for macOS (.pkg file).

This script creates a .pkgproj file with arguments and build an installer.
"""

import argparse
import logging
import os
import shutil
import sys
import tempfile

from build_tools import util


def ParseArguments():
  """Parses command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--input')
  parser.add_argument('--output')
  parser.add_argument('--pkgproj_command')
  parser.add_argument('--pkgproj_input')
  parser.add_argument('--pkgproj_output')
  parser.add_argument('--version_file')
  parser.add_argument('--build_type')
  parser.add_argument('--auto_updater_dir')
  return parser.parse_args()


def TweakPkgproj(args, src_dir):
  """Creates a .pkgproj file by filling the arguments."""
  commands = [args.pkgproj_command,
              '--output', args.pkgproj_output,
              '--input', args.pkgproj_input,
              '--version_file', args.version_file,
              '--gen_out_dir', src_dir,
              '--build_dir', src_dir,
              '--auto_updater_dir', args.auto_updater_dir,
              '--mozc_dir', os.getcwd(),
              '--launch_agent_dir', src_dir,
              '--build_type', args.build_type,
              ]
  util.RunOrDie(commands)


def BuildInstaller(args, out_dir):
  """Creates a .pkg file by running packagesbuild."""
  # Make sure Packages is installed
  if os.path.exists('/usr/local/bin/packagesbuild'):
    packagesbuild_path = '/usr/local/bin/packagesbuild'
  elif os.path.exists('/usr/bin/packagesbuild'):
    packagesbuild_path = '/usr/bin/packagesbuild'
  else:
    logging.critical('error: Cannot find "packagesbuild"')
    sys.exit(1)

  # Build the package
  commands = [
      packagesbuild_path, args.pkgproj_output, '--build-folder', out_dir
  ]
  util.RunOrDie(commands)

  shutil.copyfile(os.path.join(out_dir, 'Mozc.pkg'), args.output)


def main():
  args = ParseArguments()

  with tempfile.TemporaryDirectory() as tmp_dir:
    # Use the unzip command to extract symbolic links properly.
    util.RunOrDie(['unzip', '-q', args.input, '-d', tmp_dir])
    TweakPkgproj(args, os.path.join(tmp_dir, 'installer'))
    BuildInstaller(args, tmp_dir)


if __name__ == '__main__':
  main()
