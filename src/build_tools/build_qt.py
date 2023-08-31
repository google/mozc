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

By default, this script assumes that Qt archives are stored as

  src/third_party_cache/qtbase-everywhere-opensource-src-5.15.10.tar.xz
  src/third_party_cache/qtbase-everywhere-src-6.5.2.tar.xz

and Qt src files that are necessary to build Mozc will be checked out into

  src/third_party/qt_src.

This way we can later delete src/third_party/qt_src to free up disk space by 2GB
or so.
"""

import argparse
from collections.abc import Iterator
import dataclasses
import functools
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import time
from typing import Any, Union


ABS_SCRIPT_PATH = pathlib.Path(__file__).absolute()
# src/build_tools/build_qt.py -> src/
ABS_MOZC_SRC_DIR = ABS_SCRIPT_PATH.parents[1]
ABS_QT_SRC_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party', 'qt_src')
ABS_QT_DEST_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party', 'qt')
# The archive filename should be consistent with update_deps.py.
ABS_QT6_ARCHIVE_PATH = ABS_MOZC_SRC_DIR.joinpath(
    'third_party_cache', 'qtbase-everywhere-src-6.5.2.tar.xz')
ABS_DEFAULT_NINJA_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party', 'ninja')


def is_windows() -> bool:
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def is_mac() -> bool:
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def is_linux() -> bool:
  """Returns true if the platform is Linux."""
  return os.name == 'posix' and os.uname()[0] == 'Linux'


class ProgressPrinter:
  """A utility to print progress message with carriage return and trancatoin."""

  def __enter__(self):
    if not sys.stdout.isatty():

      class NoOpImpl:
        """A no-op implementation in case stdout is not attached to concole."""

        def print_line(self, msg: str) -> None:
          """No-op implementation.

          Args:
            msg: Unused.
          """
          del msg  # Unused
          return

      self.cleaner = None
      return NoOpImpl()

    class Impl:
      """A real implementation in case stdout is attached to concole."""
      last_output_time_ns = time.time_ns()

      def print_line(self, msg: str) -> None:
        """Print the given message with carriage return and trancatoin.

        Args:
          msg: Message to be printed.
        """
        colmuns = os.get_terminal_size().columns
        now = time.time_ns()
        if (now - self.last_output_time_ns) < 25000000:
          return
        msg = msg + ' ' * max(colmuns - len(msg), 0)
        msg = msg[0 : (colmuns)] + '\r'
        sys.stdout.write(msg)
        sys.stdout.flush()
        self.last_output_time_ns = now

    class Cleaner:
      def cleanup(self) -> None:
        colmuns = os.get_terminal_size().columns
        sys.stdout.write(' ' * colmuns + '\r')
        sys.stdout.flush()

    self.cleaner = Cleaner()
    return Impl()

  def __exit__(self, *exc):
    if self.cleaner:
      self.cleaner.cleanup()


def qt_extract_filter(
    members: Iterator[tarfile.TarInfo],
) -> Iterator[tarfile.TarInfo]:
  """Custom extract filter for the Qt Tar file.

  This custom filter can be used to adjust directory structure and drop
  unnecessary files/directories to save disk space.

  Args:
    members: an iterator of TarInfo from the Tar file.

  Yields:
    An iterator of TarInfo to be extracted.
  """
  with ProgressPrinter() as printer:
    for info in members:
      paths = info.name.split('/')
      if '..' in paths:
        continue
      if len(paths) < 1:
        continue
      paths = paths[1:]
      new_path = '/'.join(paths)
      skipping = False
      if len(paths) >= 1 and paths[0] == 'examples':
        skipping = True
      elif len(paths) >= 1 and paths[0] == 'tests':
        skipping = True
      if skipping:
        printer.print_line('skipping   ' + new_path)
        continue
      else:
        printer.print_line('extracting ' + new_path)
        info.name = new_path
        yield info


@dataclasses.dataclass
@functools.total_ordering
class QtVersion:
  """Data type for Qt version.

  Attributes:
    major: Major version.
    minor: Minor version.
    patch: Patch level.
  """
  major: int
  minor: int
  patch: int

  def __hash__(self) -> int:
    return hash(self.major, self.minor, self.patch)

  def __eq__(self, other: Any) -> bool:
    if not isinstance(other, QtVersion):
      return NotImplemented
    return (
        self.major == other.major
        and self.minor == other.minor
        and self.patch == other.patch
    )

  def __lt__(self, other: Any) -> bool:
    if not isinstance(other, QtVersion):
      return NotImplemented
    if self.major != other.major:
      return self.major < other.major
    if self.minor != other.minor:
      return self.minor < other.minor
    return self.patch < other.patch


def get_qt_version(args: argparse.Namespace) -> QtVersion:
  """Get the Qt version.

  Args:
    args: build options to be used to customize configure options of Qt.

  Returns:
    QtVersion object.
  """
  archive_name = pathlib.Path(args.qt_archive_path).resolve().name
  ver_string_tuple = (
      archive_name
      .removeprefix('qtbase-everywhere-opensource-src-')
      .removeprefix('qtbase-everywhere-src-')
      .removesuffix('.tar.xz')
      .split('.')
  )
  if len(ver_string_tuple) != 3:
    raise ValueError(f'Unsupported qt archive name: {archive_name}')
  return QtVersion(
      major=int(ver_string_tuple[0]),
      minor=int(ver_string_tuple[1]),
      patch=int(ver_string_tuple[2]),
  )


def make_configure_options(args: argparse.Namespace) -> list[str]:
  """Makes necessary configure options based on args.

  Args:
    args: build options to be used to customize configure options of Qt.

  Returns:
    A list of configure options to be passed to configure of Qt.
  Raises:
    ValueError: When Qt major version is not 6.
  """

  qt_version = get_qt_version(args)

  if qt_version.major != 6:
    raise ValueError(f'Only Qt6 is supported but specified {qt_version}.')

  qt_configure_options = ['-opensource',
                          '-silent',
                          '-no-cups',
                          '-no-dbus',
                          '-no-feature-concurrent',
                          '-no-feature-imageformatplugin',
                          '-no-feature-network',
                          '-no-feature-sql',
                          '-no-feature-sqlmodel',
                          '-no-feature-testlib',
                          '-no-feature-xml',
                          '-no-icu',
                          '-no-opengl',
                          '-no-sql-db2',
                          '-no-sql-ibase',
                          '-no-sql-mysql',
                          '-no-sql-oci',
                          '-no-sql-odbc',
                          '-no-sql-psql',
                          '-no-sql-sqlite',
                          '-nomake', 'examples',
                          '-nomake', 'tests',
                         ]

  if is_mac():
    qt_configure_options += [
        '-platform', 'macx-clang',
        '-qt-libpng',
        '-qt-pcre',
    ]
  elif is_windows():
    qt_configure_options += ['-c++std', 'c++20',
                             '-force-debug-info',
                             '-ltcg',  # Note: ignored in debug build
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

  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_dest_dir = pathlib.Path(args.qt_dest_dir).resolve()
  if qt_src_dir != qt_dest_dir:
    qt_configure_options += ['-prefix', str(qt_dest_dir)]

  return qt_configure_options


def parse_args() -> argparse.Namespace:
  """Parse command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--debug', dest='debug', default=False,
                      action='store_true',
                      help='make debug build')
  parser.add_argument('--release', dest='release', default=False,
                      action='store_true',
                      help='make release build')
  parser.add_argument('--qt_src_dir', help='qt src directory', type=str,
                      default=str(ABS_QT_SRC_DIR))
  parser.add_argument('--qt_archive_path', help='qtbase archive path', type=str,
                      default=str(ABS_QT6_ARCHIVE_PATH))
  parser.add_argument('--qt_dest_dir', help='qt dest directory', type=str,
                      default=str(ABS_QT_DEST_DIR))
  parser.add_argument('--confirm_license',
                      help='set to accept Qt OSS license',
                      action='store_true', default=False)
  parser.add_argument('--dryrun', action='store_true', default=False)
  parser.add_argument('--ninja_dir', help='Directory of ninja executable',
                      type=str, default=None)
  if is_windows():
    parser.add_argument('--vcvarsall_path', help='Path of vcvarsall.bat',
                        type=str, default=None)
  return parser.parse_args()


def get_ninja_dir(args: argparse.Namespace) -> Union[pathlib.Path, None]:
  """Return the directory of ninja executable to be used, or None.

  Args:
    args: build options to be used to customize configure options of Qt.
  Returns:
    The directory of ninja executable if ninja should be used. None otherwise.
  """
  ninja_exe = 'ninja.exe' if is_windows() else 'ninja'

  if args.ninja_dir:
    ninja_dir = pathlib.Path(args.ninja_dir).resolve()
    if ninja_dir.joinpath(ninja_exe).exists():
      return ninja_dir
    else:
      return None

  if ABS_DEFAULT_NINJA_DIR.joinpath(ninja_exe).exists():
    return ABS_DEFAULT_NINJA_DIR
  return None


def build_on_mac(args: argparse.Namespace) -> None:
  """Build Qt from the source code on Mac.


  Args:
    args: build options to be used to customize configure options of Qt.
  Raises:
    FileNotFoundError: when any required file is not found.
  """
  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_dest_dir = pathlib.Path(args.qt_dest_dir).resolve()

  if not qt_src_dir.exists():
    raise FileNotFoundError('Could not find qt_src_dir=%s' % qt_src_dir)

  env = dict(os.environ)

  # Use locally checked out ninja.exe if exists.
  ninja_dir = get_ninja_dir(args)
  if ninja_dir:
    env['PATH'] = str(ninja_dir) + os.pathsep + env['PATH']

  configure_cmds = ['./configure'] + make_configure_options(args)
  cmake = str(shutil.which('cmake', path=env['PATH']))
  build_cmds = [cmake, '--build', '.', '--parallel']
  install_cmds = [cmake, '--install', '.']

  exec_command(configure_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)
  exec_command(build_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  if qt_src_dir == qt_dest_dir:
    # No need to run 'install' command.
    return

  if qt_dest_dir.exists():
    if args.dryrun:
      print(f'dryrun: delete {qt_dest_dir}')
    else:
      shutil.rmtree(qt_dest_dir)

  exec_command(install_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)


def get_vcvarsall(path_hint: Union[str, None] = None) -> pathlib.Path:
  """Returns the path of 'vcvarsall.bat'.

  Args:
    path_hint: optional path to vcvarsall.bat

  Returns:
    The path of 'vcvarsall.bat'.

  Raises:
    FileNotFoundError: When 'vcvarsall.bat' cannot be found.
  """
  if path_hint is not None:
    path = pathlib.Path(path_hint).resolve()
    if path.exists():
      return path

  for program_files in ['Program Files', 'Program Files (x86)']:
    for edition in ['Community', 'Professional', 'Enterprise', 'BuildTools']:
      vcvarsall = pathlib.Path('C:\\', program_files, 'Microsoft Visual Studio',
                               '2022', edition, 'VC', 'Auxiliary', 'Build',
                               'vcvarsall.bat')
      if vcvarsall.exists():
        return vcvarsall

  raise FileNotFoundError(
      'Could not find vcvarsall.bat. '
      'Consider using --vcvarsall_path option e.g.\n'
      'python build_qt.py --release --confirm_license '
      r' --vcvarsall_path=C:\VS\VC\Auxiliary\Build\vcvarsall.bat'
  )


def get_vs_env_vars(
    arch: str,
    vcvarsall_path_hint: Union[str, None] = None,
) -> dict[str, str]:
  """Returns environment variables for the specified Visual Studio C++ tool.

  Oftentimes commandline build process for Windows requires to us to set up
  environment variables (especially 'PATH') first by executing an appropriate
  Visual C++ batch file ('vcvarsall.bat').  As a result, commands to be passed
  to methods like subprocess.run() can easily become complicated and difficult
  to maintain.

  With get_vs_env_vars() you can easily decouple environment variable handling
  from the actual command execution as follows.

    cwd = ...
    env = get_vs_env_vars('amd64_x86')
    subprocess.run(command_fullpath, shell=False, check=True, cwd=cwd, env=env)

  or

    cwd = ...
    env = get_vs_env_vars('amd64_x86')
    subprocess.run(command, shell=True, check=True, cwd=cwd, env=env)

  For the 'arch' argument, see the following link to find supported values.
  https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line#vcvarsall-syntax

  Args:
    arch: The architecture to be used to build Qt, e.g. 'amd64_x86'
    vcvarsall_path_hint: optional path to vcvarsall.bat

  Returns:
    A dict of environment variable.

  Raises:
    ChildProcessError: When 'vcvarsall.bat' cannot be executed.
    FileNotFoundError: When 'vcvarsall.bat' cannot be found.
  """
  vcvarsall = get_vcvarsall(vcvarsall_path_hint)

  pycmd = (r'import json;'
           r'import os;'
           r'print(json.dumps(dict(os.environ), ensure_ascii=True))')
  cmd = f'("{vcvarsall}" {arch}>nul)&&("{sys.executable}" -c "{pycmd}")'
  process = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
  stdout, _ = process.communicate()
  exitcode = process.wait()
  if exitcode != 0:
    raise ChildProcessError(f'Failed to execute {vcvarsall}')
  return json.loads(stdout.decode('ascii'))


def exec_command(command: list[str], cwd: Union[str, pathlib.Path],
                 env: dict[str, str], dryrun: bool = False) -> None:
  """Run the specified command.

  Args:
    command: Command to run.
    cwd: Directory to execute the command.
    env: Environment variables.
    dryrun: True to execute the specified command as a dry run.
  Raises:
    CalledProcessError: When the process failed.
  """
  if dryrun:
    print(f"dryrun: subprocess.run('{command}', shell=False, check=True,"
          f' cwd={cwd}, env={env})')
  else:
    subprocess.run(command, shell=False, check=True, cwd=cwd, env=env)


def build_on_windows(args: argparse.Namespace) -> None:
  """Build Qt from the source code on Windows.

  Args:
    args: build options to be used to customize configure options of Qt.

  Raises:
    FileNotFoundError: when any required file is not found.
  """
  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_dest_dir = pathlib.Path(args.qt_dest_dir).resolve()

  if not qt_src_dir.exists():
    raise FileNotFoundError('Could not find qt_src_dir=%s' % qt_src_dir)

  env = get_vs_env_vars('amd64_x86', args.vcvarsall_path)

  # Use locally checked out ninja.exe if exists.
  ninja_dir = get_ninja_dir(args)
  if ninja_dir:
    env['PATH'] = str(ninja_dir) + os.pathsep + env['PATH']

  # Add qt_src_dir to 'PATH'.
  # https://doc.qt.io/qt-6/windows-building.html#step-3-set-the-environment-variables
  env['PATH'] = str(qt_src_dir) + os.pathsep + env['PATH']

  cmd = str(shutil.which('cmd.exe', path=env['PATH']))
  configure_cmds = [cmd, '/C', 'configure.bat'] + make_configure_options(args)
  exec_command(configure_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  cmake = str(shutil.which('cmake.exe', path=env['PATH']))
  build_cmds = [cmake, '--build', '.', '--parallel']
  install_cmds = [cmake, '--install', '.']

  exec_command(build_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  if qt_src_dir == qt_dest_dir:
    # No need to run 'install' command.
    return

  if qt_dest_dir.exists():
    if args.dryrun:
      print(f'dryrun: shutil.rmtree({qt_dest_dir})')
    else:
      shutil.rmtree(qt_dest_dir)

  exec_command(install_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  # When both '--debug' and '--release' are specified for Qt6, we need to run
  # the command again with '--config debug' option to install debug DLLs.
  if args.debug and args.release:
    install_cmds += ['--config', 'debug']
    exec_command(install_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)


def extract_qt_src(args: argparse.Namespace) -> None:
  """Extract Qt src from the archive.

  Args:
    args: build options to be used to customize configure options of Qt.
  """
  qt_src_dir = pathlib.Path(args.qt_src_dir).absolute()
  if qt_src_dir.exists():
    if args.dryrun:
      print(f'dryrun: delete {qt_src_dir}')
    else:
      shutil.rmtree(qt_src_dir)

  if args.dryrun:
    print(f'dryrun: extracting {args.qt_archive_path} to {qt_src_dir}')
  else:
    qt_src_dir.mkdir(parents=True)
    with tarfile.open(args.qt_archive_path, mode='r|xz') as f:
      f.extractall(path=qt_src_dir, members=qt_extract_filter(f))


def main():
  if is_linux():
    print('On Linux, please use shared library provided by distributions.')
    sys.exit(1)

  args = parse_args()

  if not (args.debug or args.release):
    print('neither --release nor --debug is specified.')
    sys.exit(1)

  extract_qt_src(args)

  if is_mac():
    build_on_mac(args)
  elif is_windows():
    build_on_windows(args)


if __name__ == '__main__':
  main()
