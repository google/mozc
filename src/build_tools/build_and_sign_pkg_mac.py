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

"""Builds package file from .pkgproj and sings it.

Typical usage:

  % python build_and_sign_pkg_mac.py --pkgproj=/path/to/prj.pkgproj \
      --signpkg=/path/to/prj.pkg
"""

__author__ = "horo"

import logging
import optparse
import os
import shutil
import sys

from copy_file import CopyFiles
from util import PrintErrorAndExit
from util import RunOrDie


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--pkgproj', dest='pkgproj')
  parser.add_option('--signpkg', dest='signpkg')

  (opts, _) = parser.parse_args()

  return opts


def main():
  opt = ParseOption()

  if not opt.pkgproj:
    PrintErrorAndExit('--pkgproj option is mandatory.')

  if not opt.signpkg:
    PrintErrorAndExit('--signpkg option is mandatory.')

  pkgproj = os.path.abspath(opt.pkgproj)
  signpkg = os.path.abspath(opt.signpkg)

  # Make sure Packages is installed
  packagesbuild_path = ""
  if os.path.exists('/usr/local/bin/packagesbuild'):
    packagesbuild_path = '/usr/local/bin/packagesbuild'
  elif os.path.exists('/usr/bin/packagesbuild'):
    packagesbuild_path = '/usr/bin/packagesbuild'
  else:
    logging.critical('error: Cannot find "packagesbuild"')
    sys.exit(1)

  # Build the package
  cmd = [packagesbuild_path, pkgproj]
  RunOrDie(cmd)


if __name__ == '__main__':
  main()
