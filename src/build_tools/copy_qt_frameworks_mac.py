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

  % python copy_qt_frameworks.py --qtdir=/path/to/qtdir/ --qtver=6 \
      --target=/path/to/target.app/Contents/Frameworks/
"""

__author__ = "horo"

import argparse
import dataclasses
import os
from copy_file import CopyFiles
from util import RunOrDie


@dataclasses.dataclass
class QtModule:
  """A pair of Qt module name and Qt version.

  Attributes:
    name: the name of the component (e.g. 'QtCore')
    version: the version string in framework (e.g. '5' and 'A')
  """

  name: str
  version: str


def ParseArgs():
  """Parses command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--qtdir', required=True)
  parser.add_argument('--qtver', required=True, type=int)
  parser.add_argument('--target', required=True)
  return parser.parse_args()


def GetFrameworkPath(module: QtModule):
  """Return path to the library in the framework."""
  return f'{module.name}.framework/Versions/{module.version}/{module.name}'


def Symlink(src, dst):
  if os.path.exists(dst):
    return
  os.symlink(src, dst)


def CopyQt(qtdir, module: QtModule, target):
  """Copy a Qt framework from qtdir to target."""
  srcdir = f'{qtdir}/lib/{module.name}.framework/Versions/{module.version}/'
  dstdir = f'{target}/{module.name}.framework/'

  # This function creates the following file, directory and symbolic links.
  #
  # QtCore.framework/Versions/{ver}/QtCore
  # QtCore.framework/Versions/{ver}/Resources/
  # QtCore.framwwork/QtCore@ -> Versions/{ver}/QtCore
  # QtCore.framwwork/Resources@ -> Versions/{ver}/Resources
  # QtCore.framework/Versions/Current -> {ver}

  # cp {qtdir}/lib/QtCore.framework/Versions/{ver}/QtCore
  #    {target}/QtCore.framework/Versions/{ver}/QtCore
  CopyFiles(
      [srcdir + module.name], f'{dstdir}Versions/{module.version}/{module.name}'
  )

  # Copies Resources of QtGui
  # cp -r {qtdir}/lib/QtCore.framework/Resources
  #       {target}/QtCore.framework/Versions/{ver}/Resources
  CopyFiles(
      [srcdir + 'Resources'],
      f'{dstdir}Versions/{module.version}/Resources',
      recursive=True,
  )

  # ln -s {ver} {target}/QtCore.framework/Versions/Current
  Symlink(f'{module.version}', dstdir + 'Versions/Current')

  # ln -s Versions/Current/QtCore {target}/QtCore.framework/QtCore
  Symlink('Versions/Current/' + module.name, dstdir + module.name)

  # ln -s Versions/Current/Resources {target}/QtCore.framework/Resources
  Symlink('Versions/Current/Resources', dstdir + 'Resources')


def ChangeReferences(path, target, ref_to, ref_framework_paths=None):
  """Change the references of frameworks, by using install_name_tool."""
  # Change id
  cmd = [
      'install_name_tool',
      '-id',
      '%s/%s' % (ref_to, path),
      '%s/%s' % (target, path),
  ]
  RunOrDie(cmd)

  if not ref_framework_paths:
    return

  # Change references
  for ref_framework_path in ref_framework_paths:
    change_cmd = [
        'install_name_tool',
        '-change',
        '@rpath/%s' % ref_framework_path,
        '%s/%s' % (ref_to, ref_framework_path),
        '%s/%s' % (target, path),
    ]
    RunOrDie(change_cmd)


def main():
  args = ParseArgs()

  qtdir = os.path.abspath(args.qtdir)
  target = os.path.abspath(args.target)

  ref_to = '@executable_path/../../../ConfigDialog.app/Contents/Frameworks'

  if args.qtver == 5:
    version = '5'  # Qt5 uses '5' as the version name
  elif args.qtver == 6:
    version = 'A'  # Qt5 uses 'A' as the version name
  else:
    raise ValueError(f'Invalid qtver: {args.qtver}')
  qt_core = QtModule(name='QtCore', version=version)
  qt_gui = QtModule(name='QtGui', version=version)
  qt_widgets = QtModule(name='QtWidgets', version=version)
  qt_print_support = QtModule(name='QtPrintSupport', version=version)

  CopyQt(qtdir, qt_core, target)
  CopyQt(qtdir, qt_gui, target)
  CopyQt(qtdir, qt_widgets, target)
  CopyQt(qtdir, qt_print_support, target)

  libqcocoa = 'QtCore.framework/Resources/plugins/platforms/libqcocoa.dylib'
  CopyFiles(
      ['%s/plugins/platforms/libqcocoa.dylib' % qtdir],
      '%s/%s' % (target, libqcocoa),
  )

  changed_refs = []
  for ref in [
      GetFrameworkPath(qt_core),
      GetFrameworkPath(qt_gui),
      GetFrameworkPath(qt_widgets),
      GetFrameworkPath(qt_print_support),
      libqcocoa,
  ]:
    ChangeReferences(ref, target, ref_to, ref_framework_paths=changed_refs)
    changed_refs.append(ref)


if __name__ == '__main__':
  main()
