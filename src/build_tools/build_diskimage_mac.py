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

"""Create a dmg file.

  % python build_diskimage_mac.py --build_dir=/path/to/build_dir
    --volname=GoogleJapaneseInput GoogleJapaneseInput.mpkg ...

  This file creates the dmg file with the volname, whose contents are
  the remaining args.
"""

import argparse
import os
from os import path
import shutil
import subprocess
import tempfile


def CopyFile(source, target_dir):
  """Copy the source file to the target_dir.

  Args:
    source: the filename which will be copied
    target_dir: the directory which will have the copied file
  """
  source = source.rstrip("/")
  if path.isdir(source):
    shutil.copytree(source, path.join(target_dir, path.basename(source)),
                    symlinks=True)
  else:
    shutil.copy(source, target_dir)


def ParseOptions():
  """Parse command line options.

  Returns:
    A pair of (options data, remaining args)
  """
  parser = argparse.ArgumentParser()
  parser.add_argument("--volname", default="GoogleJapaneseInput")
  parser.add_argument("--output", required=True)
  parser.add_argument("--pkg_file", required=True)
  parser.add_argument("--keystone_install_file")

  return parser.parse_args()


def main():
  """The main function."""
  options = ParseOptions()

  # setup volume directory
  temp_root_dir = tempfile.mkdtemp()
  temp_dir = path.join(temp_root_dir, "dmg")
  os.mkdir(temp_dir)

  # Copy .keystone_install to the temp directory and make it executable.
  if options.keystone_install_file:
    keystone_install_file = path.join(temp_dir, ".keystone_install")
    CopyFile(options.keystone_install_file, keystone_install_file)
    os.chmod(keystone_install_file, 0o755)  # rwxr-xr-x

  # Copy .pkg file to the temp directory.
  CopyFile(options.pkg_file, path.join(temp_dir, options.volname + ".pkg"))

  # build a disk image
  dmg_path = path.join(temp_root_dir, path.basename(options.output))
  # hdiutil create will fail if there is already a target dmg file.
  if path.exists(dmg_path):
    os.remove(dmg_path)
  subprocess.run(
      [
          "/usr/bin/hdiutil", "create",
          "-srcfolder", temp_dir,
          "-volname", options.volname,
          # "SPUD" and "HFS+" are required for KeyStone.
          # "GPTSPUD" and "APFS" are default values.
          "-layout", "SPUD",
          "-fs", "HFS+",
          dmg_path,
      ],
      check=True,
  )
  CopyFile(dmg_path, options.output)

  # clean up the temp directory
  shutil.rmtree(temp_root_dir)


if __name__ == "__main__":
  main()
