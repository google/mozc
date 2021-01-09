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

r"""Change the reference to frameworks.

Typical usage:

  % change_reference_mac.py --qtdir=/path/to/qtdir/ \
      --target=/path/to/target.app/Contents/MacOS/target
"""

__author__ = "horo"

import optparse
import os

from util import PrintErrorAndExit
from util import RunOrDie


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--qtdir', dest='qtdir')
  parser.add_option('--target', dest='target')

  (opts, _) = parser.parse_args()

  return opts


def GetFrameworkPath(name, version):
  return '%s.framework/Versions/%s/%s' % (name, version, name)


def GetReferenceTo(framework):
  return ('@executable_path/../../../ConfigDialog.app/Contents/Frameworks/%s' %
          framework)


def InstallNameTool(target, reference_from, reference_to):
  cmd = ['install_name_tool', '-change', reference_from, reference_to, target]
  RunOrDie(cmd)


def main():
  opt = ParseOption()

  if not opt.qtdir:
    PrintErrorAndExit('--qtdir option is mandatory.')

  if not opt.target:
    PrintErrorAndExit('--target option is mandatory.')

  unused_qtdir = os.path.abspath(opt.qtdir)  # TODO(komatsu): remove this.
  target = os.path.abspath(opt.target)

  # Changes the reference to QtCore framework from the target application
  # From: @rpath/QtCore.framework/Versions/5/QtCore
  #   To: @executable_path/../../../MozcTool.app/Contents/Frameworks/...
  qtcore_framework = GetFrameworkPath('QtCore', '5')
  InstallNameTool(target,
                  '@rpath/%s' % qtcore_framework,
                  GetReferenceTo(qtcore_framework))

  # Changes the reference to QtGui framework from the target application
  qtgui_framework = GetFrameworkPath('QtGui', '5')
  InstallNameTool(target,
                  '@rpath/%s' % qtgui_framework,
                  GetReferenceTo(qtgui_framework))

  # Changes the reference to QtWidgets framework from the target application
  qtwidgets_framework = GetFrameworkPath('QtWidgets', '5')
  InstallNameTool(target,
                  '@rpath/%s' % qtwidgets_framework,
                  GetReferenceTo(qtwidgets_framework))

  # Change the reference to $(branding)Tool_lib from the target application
  # From: @executable_path/../Frameworks/MozcTool_lib.framework/...
  #   To: @executable_path/../../../ConfigDialog.app/Contents/Frameworks/...
  toollib_framework = GetFrameworkPath('GuiTool_lib', 'A')
  InstallNameTool(target,
                  '@executable_path/../Frameworks/%s' % toollib_framework,
                  GetReferenceTo(toollib_framework))


if __name__ == '__main__':
  main()
