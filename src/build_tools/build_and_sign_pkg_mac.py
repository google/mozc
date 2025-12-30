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

r"""Builds package file from .pkgproj and sings it.

Typical usage:

  % python build_and_sign_pkg_mac.py --pkgproj=/path/to/prj.pkgproj \
      --signpkg=/path/to/prj.pkg
"""


import logging
import optparse
import os
import shutil
import sys
import codesign_mac
from util import PrintErrorAndExit
from util import RunOrDie


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--pkgproj', dest='pkgproj')
  parser.add_option('--signpkg', dest='signpkg')
  parser.add_option('--product_dir', dest='product_dir')

  (opts, _) = parser.parse_args()

  return opts



def CodesignPackage(product_dir):
  """Codesign the package modules under the product_dir."""
  # Target modules to be signed. To simplify the build rules, we sign built
  # modules here all at once, rather than each build of the module.
  product_name = 'Mozc'
  app_res = product_name + '.app/Contents/Resources/'
  libqcocoa = 'Contents/Frameworks/QtCore.framework/Versions/Current/Resources/plugins/platforms/libqcocoa.dylib'
  targets = [
      'ConfigDialog.app/' + libqcocoa,
      app_res + 'ConfigDialog.app/' + libqcocoa,
  ] + [
      app_res + product_name + 'Converter.app',
      app_res + product_name + 'Prelauncher.app',
      app_res + product_name + 'Renderer.app',
      app_res + 'AboutDialog.app',
      app_res + 'ConfigDialog.app',
      app_res + 'DictionaryTool.app',
      app_res + 'ErrorMessageDialog.app',
      app_res + 'WordRegisterDialog.app',
  ] + [
      product_name + '.app',
      'AboutDialog.app',
      'ActivatePane.bundle',
      'ConfigDialog.app',
      'DevConfirmPane.bundle',
      'DictionaryTool.app',
      'ErrorMessageDialog.app',
      'UninstallGoogleJapaneseInput.app',
      'WordRegisterDialog.app',
  ]

  for target in targets:
    codesign_mac.Codesign(os.path.join(product_dir, target))


def main():
  opt = ParseOption()

  if not opt.pkgproj:
    PrintErrorAndExit('--pkgproj option is mandatory.')
  pkgproj = os.path.abspath(opt.pkgproj)

  if not opt.product_dir:
    PrintErrorAndExit('--product_dir option is mandatory.')
  product_dir = os.path.abspath(opt.product_dir)

  # Make sure Packages is installed
  packagesbuild_path = ''
  if os.path.exists('/usr/local/bin/packagesbuild'):
    packagesbuild_path = '/usr/local/bin/packagesbuild'
  elif os.path.exists('/usr/bin/packagesbuild'):
    packagesbuild_path = '/usr/bin/packagesbuild'
  else:
    logging.critical('error: Cannot find "packagesbuild"')
    sys.exit(1)

  # codesign
  CodesignPackage(product_dir)

  # Build the package
  cmd = [packagesbuild_path, pkgproj]
  RunOrDie(cmd)

if __name__ == '__main__':
  main()
