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

"""Change the file structures of the install files.

tweak_installfer_files.py --input installer.zip --output tweaked_installer.zip
"""

import argparse
import os
import shutil
import tempfile

from build_tools import util


def ParseArguments() -> argparse.Namespace:
  """Parses command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--input')
  parser.add_argument('--output')
  parser.add_argument('--work_dir')
  return parser.parse_args()


def RemoveQtFrameworks(app_dir: str, app_name: str) -> None:
  """Change the references to the Qt frameworks."""
  frameworks = ['QtCore', 'QtGui', 'QtPrintSupport', 'QtWidgets']
  host_dir = '@executable_path/../../../ConfigDialog.app/Contents/Frameworks'
  app_file = os.path.join(app_dir, f'Contents/MacOS/{app_name}')

  for framework in frameworks:
    shutil.rmtree(
        os.path.join(app_dir, f'Contents/Frameworks/{framework}.framework/'))
    cmd = [
        'install_name_tool',
        '-change',
        f'@rpath/{framework}.framework/Versions/5/{framework}',
        f'{host_dir}/{framework}.framework/{framework}',
        app_file,
    ]
    util.RunOrDie(cmd)


def SymlinkQtFrameworks(app_dir: str) -> None:
  """Create symbolic links of Qt frameworks."""
  frameworks = ['QtCore', 'QtGui', 'QtPrintSupport', 'QtWidgets']
  for framework in frameworks:
    # Creates the following symbolic links.
    #   QtCore.framwwork/QtCore@ -> Versions/5/QtCore
    #   QtCore.framwwork/Resources@ -> Versions/5/Resources
    #   QtCore.framework/Versions/Current@ -> 5
    framework_dir = os.path.join(app_dir,
                                 f'Contents/Frameworks/{framework}.framework/')

    # ln -s 5 {app_dir}/QtCore.framework/Versions/Current
    os.symlink('5', framework_dir + 'Versions/Current')

    # rm {app_dir}/QtCore.framework/QtCore
    os.remove(framework_dir + framework)

    # ln -s Versions/Current/QtCore {app_dir}/QtCore.framework/QtCore
    os.symlink('Versions/Current/' + framework, framework_dir + framework)

    # ln -s Versions/Current/Resources {app_dir}/QtCore.framework/Resources
    os.symlink('Versions/Current/Resources', framework_dir + 'Resources')


def TweakQtApps(top_dir: str) -> None:
  """Tweak the resource files for the Qt applications."""
  sub_qt_apps = [
      'AboutDialog', 'DictionaryTool', 'ErrorMessageDialog', 'MozcPrelauncher',
      'WordRegisterDialog'
  ]
  for app in sub_qt_apps:
    app_dir = os.path.join(top_dir, f'Mozc.app/Contents/Resources/{app}.app')
    # Remove _CodeSignature to be invalidated.
    shutil.rmtree(os.path.join(app_dir, 'Contents/_CodeSignature'))
    RemoveQtFrameworks(app_dir, app)

    # Remove the Frameworks directory, if it's empty.
    framework_dir = os.path.join(app_dir, 'Contents/Frameworks')
    if not any(os.scandir(framework_dir)):
      os.rmdir(framework_dir)

  main_qt_apps = [
      'ConfigDialog.app',
      'DictionaryTool.app',
      'Mozc.app/Contents/Resources/ConfigDialog.app',
  ]
  for app in main_qt_apps:
    app_dir = os.path.join(top_dir, app)
    SymlinkQtFrameworks(app_dir)


def TweakInstallerFiles(args: argparse.Namespace, work_dir: str) -> None:
  """Tweak the zip file of installer files to optimize the structure."""
  # Remove top_dir if it already exists.
  top_dir = os.path.join(work_dir, 'installer')
  if os.path.exists(top_dir):
    shutil.rmtree(top_dir)

  # The zip file should contain the 'installer' directory.
  util.RunOrDie(['unzip', '-q', args.input, '-d', work_dir])
  TweakQtApps(top_dir)

  # Create a zip file with the zip command.
  # It's possible but not easy to contain symlinks with shutil.make_archive.
  if os.path.exists(args.output):
    os.remove(args.output)
  orig_dir = os.getcwd()
  os.chdir(work_dir)
  cmd = ['zip', '-q', '-ry', os.path.join(orig_dir, args.output), 'installer']
  util.RunOrDie(cmd)
  os.chdir(orig_dir)


def main():
  args = ParseArguments()

  if args.work_dir:
    TweakInstallerFiles(args, args.work_dir)
  else:
    with tempfile.TemporaryDirectory() as work_dir:
      TweakInstallerFiles(args, work_dir)

if __name__ == '__main__':
  main()
