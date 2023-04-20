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

"""A helper script to build Qt from the source code.

A helper script to build Qt from the source code mainly for masOS and Windows
with dropping unnecessary features to minimize the installer size.

  python build_tools/build_qt.py --debug --release --confirm_license

By default, this script assumes that Qt is checked out at src/third_party/qt.
"""

import argparse
import os
import pathlib
import subprocess
import sys

ABS_SCRIPT_PATH = pathlib.Path(__file__).absolute()
# src/build_tools/build_qt.py -> src/
ABS_MOZC_SRC_DIR = ABS_SCRIPT_PATH.parents[1]
ABS_QT_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party', 'qt')


def IsWindows() -> bool:
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def IsMac() -> bool:
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def IsLinux() -> bool:
  """Returns true if the platform is Linux."""
  return os.name == 'posix' and os.uname()[0] == 'Linux'


def MakeConfigureOption(args: argparse.Namespace) -> list[str]:
  """Makes necessary configure options based on args.

  Args:
    args: build options to be used to customize configure options of Qt.

  Returns:
    A list of configure options to be passed to configure of Qt.
  """

  qt_configure_options = ['-opensource',
                          '-no-cups',
                          '-no-dbus',
                          '-no-icu',
                          '-no-opengl',
                          '-no-sql-db2',
                          '-no-sql-ibase',
                          '-no-sql-mysql',
                          '-no-sql-oci',
                          '-no-sql-odbc',
                          '-no-sql-psql',
                          '-no-sql-sqlite',
                          '-no-sql-sqlite2',
                          '-no-sql-tds',
                          '-nomake', 'examples',
                          '-nomake', 'tests',
                          '-nomake', 'tools',
                         ]

  if IsMac():
    qt_configure_options += [
        '-platform', 'macx-clang',
        '-qt-libpng',
        '-qt-pcre',
    ]
  elif IsWindows():
    qt_configure_options += ['-force-debug-info',
                             '-ltcg',  # Note: ignored in debug build
                             '-mp',    # enable parallel build
                             '-no-angle',
                             '-no-freetype',
                             '-no-harfbuzz',
                             '-platform', 'win32-msvc']
  if args.confirm_license:
    qt_configure_options += ['-confirm-license']

  if args.debug and args.release:
    qt_configure_options += ['-debug-and-release']
  elif args.debug:
    qt_configure_options += ['-debug']
  elif args.release:
    qt_configure_options += ['-release']

  return qt_configure_options


def ParseOption() -> argparse.Namespace:
  """Parse command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--debug', dest='debug', default=False,
                      action='store_true',
                      help='make debug build')
  parser.add_argument('--release', dest='release', default=False,
                      action='store_true',
                      help='make release build')
  parser.add_argument('--qt_dir', help='qt directory', type=str,
                      default=ABS_QT_DIR)
  parser.add_argument('--confirm_license',
                      help='set to accept Qt OSS license',
                      action='store_true', default=False)
  parser.add_argument('--dryrun', action='store_true', default=False)
  if IsWindows():
    parser.add_argument('--msvs_version', help='Visual Studio ver (e.g. 2022)',
                        type=str, default='2022')
  return parser.parse_args()


def BuildOnMac(args: argparse.Namespace) -> None:
  """Build the Qt5 library on Mac.


  Args:
    args: build options to be used to customize configure options of Qt.
  """
  abs_qtdir = args.qt_dir.absolute()

  jobs = os.cpu_count() * 2
  os.environ['MAKEFLAGS'] = '--jobs=%s' % jobs
  os.chdir(abs_qtdir)

  commands = ['./configure'] + MakeConfigureOption(args)
  if args.dryrun:
    print(f'dryrun: RunOrDie({commands})')
    print('dryrun: make')
  else:
    RunOrDie(commands)
    RunOrDie(['make'])


def BuildOnWindows(args: argparse.Namespace) -> None:
  """Build Qt from the source code on Windows.


  Args:
    args: build options to be used to customize configure options of Qt.

  Raises:
    FileNotFoundError: when some required file is not found.
  """
  vcvarsall = None
  if args.msvs_version == '2022':
    program_files = 'Program Files'
  else:
    program_files = 'Program Files (x86)'
  for edition in ['Community', 'Professional', 'Enterprise']:
    vcvarsall = pathlib.Path('C:\\', program_files, 'Microsoft Visual Studio',
                             args.msvs_version, edition, 'VC', 'Auxiliary',
                             'Build', 'vcvarsall.bat')
    if vcvarsall.exists():
      break
  if vcvarsall is None:
    raise FileNotFoundError('Could not find vcvarsall.bat')

  qt_dir = args.qt_dir.resolve()

  if not qt_dir.exists():
    raise FileNotFoundError('Could not find qt_dir=%s' % qt_dir)

  has_jom = qt_dir.joinpath('jom.exe').exists()

  options = MakeConfigureOption(args)

  arch = 'amd64_x86'
  configs = ' '.join(options)
  make = 'jom.exe' if has_jom else 'nmake.exe'
  command = (
      f'("{vcvarsall}" {arch})&&'
      f'(cd {qt_dir})&&'
      f'(configure.bat {configs})&&'
      f'({make})'
  )
  if args.dryrun:
    print(f"dryrun: subprocess.run('{command}', shell=True, check=True)")
  else:
    subprocess.run(command, shell=True, check=True)

  # Run post-build process for Windows
  post_build_script = ABS_MOZC_SRC_DIR.joinpath('win32', 'installer',
                                                'postbuilds_win.py')
  target_filenames = []
  if args.debug:
    target_filenames += ['bin/Qt5Cored.dll',
                         'bin/Qt5Guid.dll',
                         'bin/Qt5Widgetsd.dll',
                         'plugins/platforms/qwindowsd.dll']
  if args.release:
    target_filenames += ['bin/Qt5Core.dll',
                         'bin/Qt5Gui.dll',
                         'bin/Qt5Widgets.dll',
                         'plugins/platforms/qwindows.dll']
  for target_filename in target_filenames:
    abs_pdbpath = qt_dir.joinpath('lib')
    abs_targetpath = qt_dir.joinpath(target_filename)
    commands = [post_build_script,
                '--pdbpath', abs_pdbpath,
                '--targetpath', abs_targetpath]
    if args.dryrun:
      print(f'dryrun: RunOrDie({commands})')
    else:
      RunOrDie(commands)


def RunOrDie(argv: list[str | pathlib.Path]) -> None:
  """Run the command, or die if it failed."""

  # Rest are the target program name and the parameters, but we special
  # case if the target program name ends with '.py'
  if pathlib.Path(argv[0]).suffix == '.py':
    argv.insert(0, sys.executable)  # Inject the python interpreter path.

  print('Running: ' + ' '.join([str(arg) for arg in argv]))
  try:
    subprocess.check_output(argv, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    print(e.output.decode('utf-8'))
    sys.exit(e.returncode)
  print('Done.')


def main():
  if IsLinux():
    print('On Linux, please use shared library provided by distributions.')
    sys.exit(1)

  args = ParseOption()

  if not (args.debug or args.release):
    print('neither --release nor --debug is specified.')
    sys.exit(1)

  if IsMac():
    BuildOnMac(args)
  elif IsWindows():
    BuildOnWindows(args)


if __name__ == '__main__':
  main()
