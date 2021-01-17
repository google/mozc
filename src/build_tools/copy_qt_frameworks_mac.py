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

r"""Copy Qt frameworks to the target application's frameworks directory.

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


def GetFrameworkPath(name):
  """Return path to the library in the framework."""
  return '%s.framework/Versions/5/%s' % (name, name)


def Symlink(src, dst):
  if os.path.exists(dst):
    return
  os.symlink(src, dst)


def CopyQt(qtdir, qtlib, target):
  """Copy a Qt framework from qtdir to target."""
  srcdir = '%s/lib/%s.framework/Versions/5/' % (qtdir, qtlib)
  dstdir = '%s/%s.framework/' % (target, qtlib)

  # This function creates the following file, directory and symbolic links.
  #
  # QtCore.framework/Versions/5/QtCore
  # QtCore.framework/Versions/5/Resources/
  # QtCore.framwwork/QtCore@ -> Versions/5/QtCore
  # QtCore.framwwork/Resources@ -> Versions/5/Resources
  # QtCore.framework/Versions/Current -> 5

  # cp {qtdir}/lib/QtCore.framework/Versions/5/QtCore
  #    {target}/QtCore.framework/Versions/5/QtCore
  CopyFiles([srcdir + qtlib], dstdir + 'Versions/5/' + qtlib)

  # Copies Resources of QtGui
  # cp -r {qtdir}/lib/QtCore.framework/Resources
  #       {target}/QtCore.framework/Versions/5/Resources
  CopyFiles([srcdir + 'Resources'],
            dstdir + 'Versions/5/Resources',
            recursive=True)

  # ln -s 5 {target}/QtCore.framework/Versions/Current
  Symlink('5', dstdir + 'Versions/Current')

  # ln -s Versions/Current/QtCore {target}/QtCore.framework/QtCore
  Symlink('Versions/Current/' + qtlib, dstdir + qtlib)

  # ln -s Versions/Current/Resources {target}/QtCore.framework/Resources
  Symlink('Versions/Current/Resources', dstdir + 'Resources')


def ChangeReferences(path, target, ref_to, references=None):
  """Change the references of frameworks, by using install_name_tool."""
  # Change id
  cmd = ['install_name_tool',
         '-id', '%s/%s' % (ref_to, path),
         '%s/%s' % (target, path)]
  RunOrDie(cmd)

  if not references:
    return

  # Change references
  for ref in references:
    ref_framework_path = GetFrameworkPath(ref)
    change_cmd = ['install_name_tool', '-change',
                  '@rpath/%s' % ref_framework_path,
                  '%s/%s' % (ref_to, ref_framework_path),
                  '%s/%s' % (target, path)]
    RunOrDie(change_cmd)


def main():
  opt = ParseOption()

  if not opt.qtdir:
    PrintErrorAndExit('--qtdir option is mandatory.')

  if not opt.target:
    PrintErrorAndExit('--target option is mandatory.')

  qtdir = os.path.abspath(opt.qtdir)
  target = os.path.abspath(opt.target)

  ref_to = '@executable_path/../../../ConfigDialog.app/Contents/Frameworks'

  CopyQt(qtdir, 'QtCore', target)
  CopyQt(qtdir, 'QtGui', target)
  CopyQt(qtdir, 'QtWidgets', target)
  CopyQt(qtdir, 'QtPrintSupport', target)

  qtcore_fpath = GetFrameworkPath('QtCore')
  qtgui_fpath = GetFrameworkPath('QtGui')
  qtwidgets_fpath = GetFrameworkPath('QtWidgets')
  qtprint_fpath = GetFrameworkPath('QtPrintSupport')

  ChangeReferences(qtcore_fpath, target, ref_to)
  ChangeReferences(qtgui_fpath, target, ref_to, references=['QtCore'])
  ChangeReferences(
      qtwidgets_fpath, target, ref_to, references=['QtCore', 'QtGui'])
  ChangeReferences(
      qtprint_fpath,
      target,
      ref_to,
      references=['QtCore', 'QtGui', 'QtWidgets'])

  libqcocoa = 'QtCore.framework/Resources/plugins/platforms/libqcocoa.dylib'
  CopyFiles(['%s/plugins/platforms/libqcocoa.dylib' % qtdir],
            '%s/%s' % (target, libqcocoa))
  ChangeReferences(libqcocoa, target, ref_to,
                   references=['QtCore', 'QtGui',
                               'QtWidgets', 'QtPrintSupport'])


if __name__ == '__main__':
  main()
