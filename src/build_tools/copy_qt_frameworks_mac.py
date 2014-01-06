# -*- coding: utf-8 -*-
# Copyright 2010-2014, Google Inc.
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

"""Copy Qt frameworks to the target application's frameworks directory.

Typical usage:

  % python copy_qt_frameworks.py --qtdir=/path/to/qtdir/ \
      --target=/path/to/target.app/Contents/Frameworks/
"""

__author__ = "horo"

import optparse
import os

from copy_file import CopyFiles
from util import PrintErrorAndExit
from util import RunOrDie


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--qtdir', dest='qtdir')
  parser.add_option('--target', dest='target')

  (opts, _) = parser.parse_args()

  return opts


def main():
  opt = ParseOption()

  if not opt.qtdir:
    PrintErrorAndExit('--qtdir option is mandatory.')

  if not opt.target:
    PrintErrorAndExit('--target option is mandatory.')

  qtdir = os.path.abspath(opt.qtdir)
  target = os.path.abspath(opt.target)

  # Copies QtCore
  CopyFiles(['%s/lib/QtCore.framework/Versions/4/QtCore' % qtdir],
           '%s/QtCore.framework/Versions/4/QtCore' % target)
  # Copies QtGui
  CopyFiles(['%s/lib/QtGui.framework/Versions/4/QtGui' % qtdir],
           '%s/QtGui.framework/Versions/4/QtGui' % target)

  # Copies Resources of QtGui
  CopyFiles(['%s/lib/QtGui.framework/Versions/4/Resources' % qtdir],
           '%s/QtGui.framework/Resources' % target,
           recursive=True)

  # Changes QtGui id
  cmd = ["install_name_tool", "-id",
         "@executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui",
         "%s/QtGui.framework/Versions/4/QtGui" % target]
  RunOrDie(cmd)

  # Changes QtCore id
  cmd = ["install_name_tool", "-id",
         "@executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore",
         '%s/QtCore.framework/Versions/4/QtCore' % target]
  RunOrDie(cmd)

  # Changes the reference to QtCore framework from QtGui
  cmd = ["install_name_tool", "-change",
         "%s/lib/QtCore.framework/Versions/4/QtCore" % qtdir,
         "@executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore",
         "%s/QtGui.framework/Versions/4/QtGui" % target]
  RunOrDie(cmd)


if __name__ == '__main__':
  main()
