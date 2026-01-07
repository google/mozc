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
  parser.add_argument('--productbuild', action='store_true')
  parser.add_argument('--noqt', action='store_true')
  parser.add_argument('--oss', action='store_true')
  parser.add_argument('--channel', default='dev')
  parser.add_argument('--work_dir')
  # '-' means pseudo identity.
  # https://github.com/bazelbuild/rules_apple/blob/3.5.1/apple/internal/codesigning_support.bzl#L42
  # https://developer.apple.com/documentation/security/seccodesignatureflags/1397793-adhoc
  parser.add_argument('--codesign_identity', default='-')
  return parser.parse_args()


def RemoveQtFrameworks(app_dir: str, app_name: str) -> None:
  """Change the references to the Qt frameworks."""
  frameworks = ['QtCore', 'QtGui', 'QtPrintSupport', 'QtWidgets']
  host_dir = '@executable_path/../../../ConfigDialog.app/Contents/Frameworks'
  app_file = os.path.join(app_dir, f'Contents/MacOS/{app_name}')

  for framework in frameworks:
    shutil.rmtree(
        os.path.join(app_dir, f'Contents/Frameworks/{framework}.framework/')
    )
    cmd = [
        'install_name_tool',
        '-change',
        f'@rpath/{framework}.framework/Versions/A/{framework}',
        f'{host_dir}/{framework}.framework/{framework}',
        app_file,
    ]
    util.RunOrDie(cmd)


def SymlinkQtFrameworks(app_dir: str) -> None:
  """Create symbolic links of Qt frameworks."""
  frameworks = ['QtCore', 'QtGui', 'QtPrintSupport', 'QtWidgets']
  for framework in frameworks:
    # Creates the following symbolic links.
    #   QtCore.framwwork/QtCore@ -> Versions/A/QtCore
    #   QtCore.framwwork/Resources@ -> Versions/A/Resources
    #   QtCore.framework/Versions/Current@ -> A
    framework_dir = os.path.join(
        app_dir, f'Contents/Frameworks/{framework}.framework/'
    )

    # Restore symlinks. Bazel uses zip without consideration of symlinks.
    # It changes symlink files to normal files. The following logics remove
    # those normal files and make symlinks again.

    # rm {app_dir}/QtCore.framework/Versions/Current
    # ln -s A {app_dir}/QtCore.framework/Versions/Current
    os.remove(framework_dir + 'Versions/Current')
    os.symlink('A', framework_dir + 'Versions/Current')

    # rm {app_dir}/QtCore.framework/QtCore
    # ln -s Versions/Current/QtCore {app_dir}/QtCore.framework/QtCore
    os.remove(framework_dir + framework)
    os.symlink('Versions/Current/' + framework, framework_dir + framework)

    # rm {app_dir}/QtCore.framework/Resources
    # ln -s Versions/Current/Resources {app_dir}/QtCore.framework/Resources
    os.remove(framework_dir + 'Resources')
    os.symlink('Versions/Current/Resources', framework_dir + 'Resources')


def TweakQtApps(top_dir: str, oss: bool) -> None:
  """Tweak the resource files for the Qt applications."""
  name = 'Mozc' if oss else 'GoogleJapaneseInput'
  sub_qt_apps = [
      'AboutDialog',
      'DictionaryTool',
      'ErrorMessageDialog',
      f'{name}Prelauncher',
      'WordRegisterDialog',
  ]
  for app in sub_qt_apps:
    app_dir = os.path.join(top_dir, f'{name}.app/Contents/Resources/{app}.app')
    RemoveQtFrameworks(app_dir, app)

    # Remove the Frameworks directory, if it's empty.
    framework_dir = os.path.join(app_dir, 'Contents/Frameworks')
    if not any(os.scandir(framework_dir)):
      os.rmdir(framework_dir)

  qt_app = f'{top_dir}/{name}.app/Contents/Resources/ConfigDialog.app'
  SymlinkQtFrameworks(qt_app)


def TweakForProductbuild(
    top_dir: str, tweak_qt: bool, oss: bool, channel: str
) -> None:
  """Tweak file paths for the productbuild command."""
  orig_dir = os.getcwd()
  os.chdir(top_dir)

  if oss:
    is_dev_channel = False
    name = 'Mozc'
    folder = 'Mozc'
  else:
    is_dev_channel = channel == 'dev'
    name = 'GoogleJapaneseInput'
    folder = 'GoogleJapaneseInput.localized'

  renames = [
      (f'Uninstall{name}.app', f'root/Applications/{folder}/'),
      (f'{name}.app', 'root/Library/Input Methods/'),
      ('LaunchAgents', 'root/Library/'),
      ('ActivatePane.bundle', 'Plugins/'),
      ('InstallerSections.plist', 'Plugins/'),
      ('postflight.sh', 'scripts/postinstall'),
      ('preflight.sh', 'scripts/preinstall'),
      (
          'Resources/en.lproj/Localizable.strings',
          f'root/Applications/{folder}/.localized/en.strings',
      ),
      (
          'Resources/ja.lproj/Localizable.strings',
          f'root/Applications/{folder}/.localized/ja.strings',
      ),
  ]

  # For the dev channel, add the dev confirm section to the installer.
  if is_dev_channel:
    renames += [('DevConfirmPane.bundle', 'Plugins/')]
  else:
    shutil.rmtree('DevConfirmPane.bundle')
    # Remove the dev confirm section from InstallerSections.plist
    contents = []
    with open('InstallerSections.plist', 'r') as f:
      for line in f:
        if 'DevConfirmPane' in line:
          continue
        contents.append(line)
    with open('InstallerSections.plist', 'w') as f:
      f.write(''.join(contents))

  for src, dst in renames:
    if dst.endswith('/'):
      dst = os.path.join(dst, src)
    os.renames(src, dst)
  os.chmod('scripts/postinstall', 0o755)
  os.chmod('scripts/preinstall', 0o755)

  if tweak_qt:
    resources_dir = f'/Library/Input Methods/{name}.app/Contents/Resources/'
    # /Applications/Mozc/ConfigDialog.app is a symlink to
    # /Library/Input Method/Mozc.app/Contents/Resources/ConfigDialog.app
    os.symlink(
        resources_dir + 'ConfigDialog.app/',
        f'root/Applications/{folder}/ConfigDialog.app',
    )
    os.symlink(
        resources_dir + 'DictionaryTool.app/',
        f'root/Applications/{folder}/DictionaryTool.app',
    )

  os.chdir(orig_dir)


def Codesign(top_dir: str, identity: str) -> None:
  """Codesign the installer files."""
  # remove existing _CodeSignature before overwriting the codesigns.
  dir_name = '_CodeSignature'
  for cur_dir, dirs, _ in os.walk(top_dir):  # symbolic links are not followed.
    if dir_name in dirs:
      shutil.rmtree(os.path.join(cur_dir, dir_name))
      dirs.remove(dir_name)  # skip walking the removed directory.

  args = ['--force', '--sign', identity, '--keychain', 'login.keychain']

  # --option=runtime is required for notarization.
  # On the other hand, do not add the option for the pseudo identity ('-').
  # https://github.com/google/mozc/issues/1412
  if identity != '-':
    args.append('--option=runtime')

  # codesign libqcocoa.dylib
  file_name = 'libqcocoa.dylib'
  for cur_dir, _, files in os.walk(top_dir):  # symbolic links are not followed.
    if file_name in files:
      path = os.path.join(cur_dir, file_name)
      codesign = ['/usr/bin/codesign', *args, path]
      util.RunOrDie(codesign)

  # codesign apps
  # Walk the directory from the bottom to the top. This is necessary because
  # the sub apps should be signed before the main app.
  # Note, os.walk does not folow symbolic links.
  for cur_dir, dirs, _ in os.walk(top_dir, topdown=False):
    for dir_name in dirs:
      path = os.path.join(cur_dir, dir_name)
      ext = os.path.splitext(dir_name)[1]
      is_app = ext in ['.app', '.bundle', '.framework']
      if is_app and not os.path.islink(path):
        codesign = ['/usr/bin/codesign', *args, path]
        util.RunOrDie(codesign)


def TweakInstallerFiles(args: argparse.Namespace, work_dir: str) -> None:
  """Tweak the zip file of installer files to optimize the structure."""
  # Remove top_dir if it already exists.
  top_dir = os.path.join(work_dir, 'installer')
  if os.path.exists(top_dir):
    shutil.rmtree(top_dir)

  # The zip file should contain the 'installer' directory.
  util.RunOrDie(['unzip', '-q', args.input, '-d', work_dir])

  tweak_qt = not args.noqt

  if tweak_qt:
    TweakQtApps(top_dir, args.oss)

  if args.productbuild:
    TweakForProductbuild(top_dir, tweak_qt, args.oss, args.channel)
    Codesign(top_dir, args.codesign_identity)

  # Create a zip file with the zip command.
  # It's not easy to contain symlinks with shutil.make_archive.
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
