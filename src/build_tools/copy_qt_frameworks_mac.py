# -*- coding: utf-8 -*-
# Copyright 2010-2018, Google Inc.
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


def GetFrameworkPath(name, version):
  """Return path to the library in the framework."""
  return '%s.framework/Versions/%s/%s' % (name, version, name)


def CopyQt(qtdir, qtlib, version, target, copy_resources=False):
  """Copy a Qt framework from qtdir to target."""
  framework_path = GetFrameworkPath(qtlib, version)
  CopyFiles(['%s/lib/%s' % (qtdir, framework_path)],
            '%s/%s' % (target, framework_path))

  if copy_resources:
    # Copies Resources of QtGui
    CopyFiles(['%s/lib/%s.framework/Versions/%s/Resources' %
               (qtdir, qtlib, version)],
              '%s/%s.framework/Resources' % (target, qtlib),
              recursive=True)

  if version == '4':
    # For codesign, Info.plist should be copied to Resources/.
    CopyFiles(['%s/lib/%s.framework/Contents/Info.plist' % (qtdir, qtlib)],
              '%s/%s.framework/Resources/Info.plist' % (target, qtlib))


def ChangeReferences(qtdir, path, version, target, ref_to, references=None):
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
    ref_framework_path = GetFrameworkPath(ref, version)
    change_cmd = ['install_name_tool', '-change',
                  '%s/lib/%s' % (qtdir, ref_framework_path),
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

  CopyQt(qtdir, 'QtCore', '5', target, copy_resources=True)
  CopyQt(qtdir, 'QtGui', '5', target, copy_resources=True)
  CopyQt(qtdir, 'QtWidgets', '5', target, copy_resources=True)
  CopyQt(qtdir, 'QtPrintSupport', '5', target, copy_resources=True)

  ChangeReferences(qtdir, GetFrameworkPath('QtCore', '5'),
                   '5', target, ref_to)
  ChangeReferences(qtdir, GetFrameworkPath('QtGui', '5'),
                   '5', target, ref_to,
                   references=['QtCore'])
  ChangeReferences(qtdir, GetFrameworkPath('QtWidgets', '5'),
                   '5', target, ref_to,
                   references=['QtCore', 'QtGui'])
  ChangeReferences(qtdir, GetFrameworkPath('QtPrintSupport', '5'),
                   '5', target, ref_to,
                   references=['QtCore', 'QtGui', 'QtWidgets'])

  libqcocoa = 'QtCore.framework/Resources/plugins/platforms/libqcocoa.dylib'
  CopyFiles(['%s/plugins/platforms/libqcocoa.dylib' % qtdir],
            '%s/%s' % (target, libqcocoa))
  ChangeReferences(qtdir, libqcocoa, '5', target, ref_to,
                   references=['QtCore', 'QtGui',
                               'QtWidgets', 'QtPrintSupport'])


if __name__ == '__main__':
  main()
