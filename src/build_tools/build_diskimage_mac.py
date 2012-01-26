# -*- coding: utf-8 -*-
# Copyright 2010-2012, Google Inc.
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

  % python build_diskimage_mac.py --build_dir=/path/to/build_dir \
    --volname=GoogleJapaneseInput GoogleJapaneseInput.mpkg ... \

  This file creates the dmg file with the volname, whose contents are
  the remaining args.
"""

__author__ = "mukai"

import logging
import optparse
import os
import tempfile
import shutil
import subprocess

from os import path


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
  parser = optparse.OptionParser()
  parser.add_option("--build_dir", dest="build_dir")
  parser.add_option("--volname", dest="volname", default="GoogleJapaneseInput")
  parser.add_option("--pkgfile", dest="pkgfile")

  return parser.parse_args()


def main():
  """The main function"""
  (options, args) = ParseOptions()
  if options.build_dir is None:
    logging.error("--build_dir is not specified.")
    exit(-1)

  build_dir = options.build_dir

  # setup volume directory
  temp_dir = tempfile.mkdtemp()
  CopyFile(path.join(build_dir, ".keystone_install"), temp_dir)
  os.chmod(path.join(temp_dir, ".keystone_install"), 0755) # rwxr-xr-x
  for a in args:
    CopyFile(path.join(build_dir, a), temp_dir)

  # build a disk image
  dmg_path = path.join(build_dir, options.volname + ".dmg")
  # hdiutil create will fail if there is already a target dmg file.
  if path.exists(dmg_path):
    os.remove(dmg_path)
  process = subprocess.Popen(["/usr/bin/hdiutil", "create",
                              "-srcfolder", temp_dir,
                              "-format", "UDBZ", "-volname", options.volname,
                              dmg_path])
  process.wait()

  # clean up the temp directory
  shutil.rmtree(temp_dir)


if __name__ == '__main__':
  main()
