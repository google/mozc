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

  src/third_party_cache/qtbase-everywhere-src-6.9.1.tar.xz

and Qt src files that are necessary to build Mozc will be checked out into

  src/third_party/qt_src.

This way we can later delete src/third_party/qt_src to free up disk space by 2GB
or so.
"""

import argparse
from collections.abc import Iterator
import dataclasses
import functools
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import tarfile
from typing import Any, Union

from progress_printer import ProgressPrinter
from vs_util import get_vs_env_vars


ABS_SCRIPT_PATH = pathlib.Path(__file__).absolute()
# src/build_tools/build_qt.py -> src/
ABS_MOZC_SRC_DIR = ABS_SCRIPT_PATH.parents[1]
ABS_QT_SRC_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party', 'qt_src')
ABS_QT_DEST_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party', 'qt')
ABS_QT_HOST_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party', 'qt_host')
# The archive filename should be consistent with update_deps.py.
ABS_QT6_ARCHIVE_PATH = ABS_MOZC_SRC_DIR.joinpath(
    'third_party_cache', 'qtbase-everywhere-src-6.9.1.tar.xz'
)
ABS_DEFAULT_NINJA_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party', 'ninja')
QT_CONFIGURE_COMMON = [
    '-opensource',
    '-c++std',
    'c++20',
    '-silent',
    '-no-cups',
    '-no-dbus',
    '-no-feature-androiddeployqt',
    '-no-feature-animation',
    '-no-feature-calendarwidget',
    '-no-feature-completer',
    '-no-feature-concatenatetablesproxymodel',
    '-no-feature-concurrent',
    '-no-feature-dial',
    '-no-feature-effects',
    '-no-feature-fontcombobox',
    '-no-feature-fontdialog',
    '-no-feature-identityproxymodel',
    '-no-feature-image_heuristic_mask',
    '-no-feature-imageformatplugin',
    '-no-feature-islamiccivilcalendar',
    '-no-feature-itemmodeltester',
    '-no-feature-jalalicalendar',
    '-no-feature-macdeployqt',
    '-no-feature-mdiarea',
    '-no-feature-mimetype',
    '-no-feature-movie',
    '-no-feature-network',
    '-no-feature-poll-exit-on-error',
    '-no-feature-qmake',
    '-no-feature-sha3-fast',
    '-no-feature-sharedmemory',
    '-no-feature-socks5',
    '-no-feature-splashscreen',
    '-no-feature-sql',
    '-no-feature-sqlmodel',
    '-no-feature-sspi',
    '-no-feature-stringlistmodel',
    '-no-feature-tabletevent',
    '-no-feature-testlib',
    '-no-feature-textbrowser',
    '-no-feature-textmarkdownreader',
    '-no-feature-textmarkdownwriter',
    '-no-feature-textodfwriter',
    '-no-feature-timezone',
    '-no-feature-topleveldomain',
    '-no-feature-undoview',
    '-no-feature-whatsthis',
    '-no-feature-windeployqt',
    '-no-feature-wizard',
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
    '-nomake',
    'examples',
    '-nomake',
    'tests',
]


def is_windows() -> bool:
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def is_mac() -> bool:
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def is_linux() -> bool:
  """Returns true if the platform is Linux."""
  return os.name == 'posix' and os.uname()[0] == 'Linux'


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


class StatefulQtExtractionFilter:
  """A stateful extraction filter for PEP 706.

  See https://peps.python.org/pep-0706/ for details.
  """

  def __enter__(self):
    self.printer = ProgressPrinter().__enter__()
    return self

  def __exit__(self, *exc):
    self.printer.__exit__(exc)

  def __call__(
      self,
      member: tarfile.TarInfo,
      dest_path: Union[str, pathlib.Path],
  ) -> Union[tarfile.TarInfo, None]:
    data = tarfile.data_filter(member, dest_path)
    if data is None:
      return data

    paths = member.name.split('/')
    if len(paths) < 1:
      return None
    paths = paths[1:]
    new_path = '/'.join(paths)
    skipping = False
    if len(paths) >= 1 and paths[0] == 'examples':
      skipping = True
    elif len(paths) >= 1 and paths[0] == 'tests':
      skipping = True
    if skipping:
      self.printer.print_line('skipping   ' + new_path)
      return None
    self.printer.print_line('extracting ' + new_path)
    return member.replace(name=new_path, deep=False)


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
    return hash((self.major, self.minor, self.patch))

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
      archive_name.removeprefix('qtbase-everywhere-opensource-src-')
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


def make_host_configure_options(args: argparse.Namespace) -> list[str]:
  """Makes necessary configure options to build Qt host tools based on args.

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

  qt_configure_options = QT_CONFIGURE_COMMON + [
      '-no-accessibility',
      '-no-gui',
      '-no-widgets',
      '-make',
      'tools',
      # Always build Qt tools (e.g. "uic") as release build.
      '-release',
      # Qt tools (e.g. "uic") are not directly or indirectly linked to Mozc's
      # artifacts. Thus it's OK to build them with statically linking to Qt
      # libraries without worrying about license.
      '-static',
  ]

  if is_mac():
    qt_configure_options += [
        '-platform',
        'macx-clang',
        '-qt-libpng',
        '-qt-pcre',
    ]
  elif is_windows():
    qt_configure_options += [
        '-no-freetype',
        '-no-harfbuzz',
        '-platform',
        'win32-msvc',
    ]
  if args.confirm_license:
    qt_configure_options += ['-confirm-license']

  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_host_dir = pathlib.Path(args.qt_host_dir).resolve()
  if qt_src_dir != qt_host_dir:
    qt_configure_options += ['-prefix', str(qt_host_dir)]

  return qt_configure_options


def make_configure_options(args: argparse.Namespace) -> list[str]:
  """Makes necessary configure options based on args.

  Args:
    args: build options to be used to customize configure options of Qt.

  Returns:
    A list of configure options to be passed to configure of Qt.
  Raises:
    ValueError: When Qt major version is not 6.
    ValueError: When --macos_cpus=arm64 is set on Intel64 mac.
  """

  qt_version = get_qt_version(args)

  if qt_version.major != 6:
    raise ValueError(f'Only Qt6 is supported but specified {qt_version}.')

  qt_configure_options = QT_CONFIGURE_COMMON
  cmake_options = []

  if is_mac():
    qt_configure_options += [
        '-platform',
        'macx-clang',
        '-qt-libpng',
        '-qt-pcre',
    ]

    # Set the target CPUs for macOS
    # Host: arm64  -> Targets: --macos_cpus or ["x86_64", "arm64"]
    # Host: x86_64 -> Targets: --macos_cpus or ["x86_64"]
    host_arch = os.uname().machine
    macos_cpus = ['x86_64', 'arm64'] if host_arch == 'arm64' else ['x86_64']
    if args.macos_cpus:
      macos_cpus = args.macos_cpus.split(',')
      if host_arch == 'x86_64' and macos_cpus == ['arm64']:
        # Haven't figured out how to make this work...
        raise ValueError('--macos_cpus=arm64 on Intel64 mac is not supported.')
      # 'x86_64' needs to be the first entry if exists.
      # https://doc-snapshots.qt.io/qt6-6.5/macos-building.html#step-2-build-the-qt-library
      if 'x86_64' in macos_cpus and macos_cpus[0] != 'x86_64':
        macos_cpus.remove('x86_64')
        macos_cpus = ['x86_64'] + macos_cpus
    cmake_options += [
        '-DCMAKE_OSX_ARCHITECTURES:STRING=' + ';'.join(macos_cpus),
    ]

  elif is_windows():
    qt_configure_options += [
        '-force-debug-info',
        '-ltcg',  # Note: ignored in debug build
        '-no-freetype',
        '-no-harfbuzz',
        '-platform',
        'win32-msvc',
    ]
    if args.target_arch in ['x64', 'amd64']:
      qt_configure_options += ['-intelcet']

  if args.confirm_license:
    qt_configure_options += ['-confirm-license']

  if args.debug and args.release:
    qt_configure_options += ['-debug-and-release']
  elif args.debug:
    qt_configure_options += ['-debug']
  elif args.release:
    qt_configure_options += ['-release']

  if args.release:
    qt_configure_options += ['-optimize-size']

  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_dest_dir = pathlib.Path(args.qt_dest_dir).resolve()
  if qt_src_dir != qt_dest_dir:
    qt_configure_options += ['-prefix', str(qt_dest_dir)]

  return qt_configure_options + (
      (['--'] + cmake_options) if cmake_options else []
  )


def parse_args() -> argparse.Namespace:
  """Parse command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--debug',
      dest='debug',
      default=False,
      action='store_true',
      help='make debug build',
  )
  parser.add_argument(
      '--release',
      dest='release',
      default=False,
      action='store_true',
      help='make release build',
  )
  parser.add_argument(
      '--qt_src_dir',
      help='qt src directory',
      type=str,
      default=str(ABS_QT_SRC_DIR),
  )
  parser.add_argument(
      '--qt_archive_path',
      help='qtbase archive path',
      type=str,
      default=str(ABS_QT6_ARCHIVE_PATH),
  )
  parser.add_argument(
      '--qt_dest_dir',
      help='qt dest directory',
      type=str,
      default=str(ABS_QT_DEST_DIR),
  )
  parser.add_argument(
      '--qt_host_dir',
      help='qt host tools directory',
      type=str,
      default=str(ABS_QT_HOST_DIR),
  )
  parser.add_argument(
      '--confirm_license',
      help='set to accept Qt OSS license',
      action='store_true',
      default=False,
  )
  parser.add_argument('--dryrun', action='store_true', default=False)
  parser.add_argument(
      '--ninja_dir',
      help='Directory of ninja executable',
      type=str,
      default=None,
  )
  if is_windows():
    parser.add_argument(
        '--target_arch', help='"x64" or "arm64"', type=str, default='x64'
    )
    parser.add_argument(
        '--vcvarsall_path', help='Path of vcvarsall.bat', type=str, default=None
    )
  elif is_mac():
    parser.add_argument(
        '--macos_cpus',
        help=(
            'comma-separated CPU archs for mac Build (e.g. '
            '"arm64", "x86_64,arm64"). Corresponds to the '
            'same option in Bazel.'
        ),
        type=str,
        default=None,
    )
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


def build_host_on_mac(args: argparse.Namespace) -> None:
  """Build Qt host tools from the source code on mac.

  Args:
    args: build options to be used to customize configure options of Qt.

  Raises:
    FileNotFoundError: when any required file is not found.
  """
  extract_qt_src(args)

  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_host_dir = pathlib.Path(args.qt_host_dir).resolve()

  if not (args.dryrun or qt_src_dir.exists()):
    raise FileNotFoundError('Could not find qt_src_dir=%s' % qt_src_dir)

  env = dict(os.environ)

  # Use locally checked out ninja.exe if exists.
  ninja_dir = get_ninja_dir(args)
  if ninja_dir:
    env['PATH'] = str(ninja_dir) + os.pathsep + env['PATH']

  configure_cmds = ['./configure'] + make_host_configure_options(args)
  cmake = str(shutil.which('cmake', path=env['PATH']))
  build_cmds = [cmake, '--build', '.', '--parallel']
  install_cmds = [cmake, '--install', '.']

  exec_command(configure_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)
  exec_command(build_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  if qt_src_dir == qt_host_dir:
    # No need to run 'install' command.
    return

  if qt_host_dir.exists():
    if args.dryrun:
      print(f'dryrun: delete {qt_host_dir}')
    else:
      shutil.rmtree(qt_host_dir)

  exec_command(install_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)


def build_on_mac(args: argparse.Namespace) -> None:
  """Build Qt from the source code on Mac.

  Args:
    args: build options to be used to customize configure options of Qt.

  Raises:
    FileNotFoundError: when any required file is not found.
  """
  extract_qt_src(args)

  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_dest_dir = pathlib.Path(args.qt_dest_dir).resolve()
  qt_host_dir = pathlib.Path(args.qt_host_dir).resolve()

  if not (args.dryrun or qt_src_dir.exists()):
    raise FileNotFoundError('Could not find qt_src_dir=%s' % qt_src_dir)

  env = dict(os.environ)

  # Use locally checked out ninja.exe if exists.
  ninja_dir = get_ninja_dir(args)
  if ninja_dir:
    env['PATH'] = str(ninja_dir) + os.pathsep + env['PATH']

  env['QT_HOST_PATH'] = str(qt_host_dir)

  configure_cmds = ['./configure'] + make_configure_options(args)
  cmake = str(shutil.which('cmake', path=env['PATH']))
  build_cmds = [cmake, '--build', '.', '--parallel']
  install_cmds = [cmake, '--install', '.']

  exec_command(configure_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)
  exec_command(build_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  if qt_src_dir != qt_dest_dir:
    if qt_dest_dir.exists():
      if args.dryrun:
        print(f'dryrun: delete {qt_dest_dir}')
      else:
        shutil.rmtree(qt_dest_dir)

    exec_command(install_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

    # Copy include files.
    for include in ['QtCore', 'QtGui', 'QtPrintSupport', 'QtWidgets']:
      src = qt_src_dir.joinpath('include').joinpath(include)
      dest = qt_dest_dir.joinpath('include').joinpath(include)
      if args.dryrun:
        print(f'dryrun: copy {src} => {dest}')
      else:
        shutil.copytree(src=src, dst=dest)

  for tool in ['moc', 'rcc', 'uic']:
    src = qt_host_dir.joinpath('libexec').joinpath(tool)
    dest = qt_dest_dir.joinpath('libexec').joinpath(tool)
    if args.dryrun:
      print(f'dryrun: copy {src} => {dest}')
    else:
      shutil.copy2(src=src, dst=dest)

  if args.dryrun:
    print(f'dryrun: shutil.rmtree({qt_host_dir})')
  else:
    shutil.rmtree(qt_host_dir)


def exec_command(
    command: list[str],
    cwd: Union[str, pathlib.Path],
    env: dict[str, str],
    dryrun: bool = False,
) -> None:
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
    print(
        f"dryrun: subprocess.run('{command}', shell=False, check=True,"
        f' cwd={cwd}, env={env})'
    )
  else:
    subprocess.run(command, shell=False, check=True, cwd=cwd, env=env)


def build_host_on_windows(args: argparse.Namespace) -> None:
  """Build Qt host tools from the source code on Windows.

  Args:
    args: build options to be used to customize configure options of Qt.

  Raises:
    FileNotFoundError: when any required file is not found.
  """
  extract_qt_src(args)

  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_host_dir = pathlib.Path(args.qt_host_dir).resolve()

  if not (args.dryrun or qt_src_dir.exists()):
    raise FileNotFoundError('Could not find qt_src_dir=%s' % qt_src_dir)

  arch = platform.uname().machine.lower()
  env = get_vs_env_vars(arch, args.vcvarsall_path)

  # Use locally checked out ninja.exe if exists.
  ninja_dir = get_ninja_dir(args)
  if ninja_dir:
    env['PATH'] = str(ninja_dir) + os.pathsep + env['PATH']

  # Add qt_src_dir to 'PATH'.
  # https://doc.qt.io/qt-6/windows-building.html#step-3-set-the-environment-variables
  env['PATH'] = str(qt_src_dir) + os.pathsep + env['PATH']

  cmd = str(shutil.which('cmd.exe', path=env['PATH']))

  configure_cmds = [cmd, '/C', 'configure.bat'] + make_host_configure_options(
      args
  )
  exec_command(configure_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  cmake = str(shutil.which('cmake.exe', path=env['PATH']))
  build_cmds = [cmake, '--build', '.', '--parallel']
  install_cmds = [cmake, '--install', '.']

  exec_command(build_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  if qt_src_dir == qt_host_dir:
    # No need to run 'install' command.
    return

  if qt_host_dir.exists():
    if args.dryrun:
      print(f'dryrun: shutil.rmtree({qt_host_dir})')
    else:
      shutil.rmtree(qt_host_dir)

  exec_command(install_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)


def normalize_win_arch(arch: str) -> str:
  """Normalize the architecture name for Windows build environment.

  Args:
    arch: a string representation of a CPU architecture to be normalized.

  Returns:
    String representation of a CPU architecture (e.g. 'x64' and 'arm64')
  """
  normalized = arch.lower()
  if normalized == 'amd64':
    return 'x64'
  return normalized


def build_on_windows(args: argparse.Namespace) -> None:
  """Build Qt from the source code on Windows.

  Args:
    args: build options to be used to customize configure options of Qt.

  Raises:
    FileNotFoundError: when any required file is not found.
  """
  extract_qt_src(args)

  qt_src_dir = pathlib.Path(args.qt_src_dir).resolve()
  qt_dest_dir = pathlib.Path(args.qt_dest_dir).resolve()
  qt_host_dir = pathlib.Path(args.qt_host_dir).resolve()

  if not (args.dryrun or qt_src_dir.exists()):
    raise FileNotFoundError('Could not find qt_src_dir=%s' % qt_src_dir)

  host_arch = normalize_win_arch(platform.uname().machine)
  target_arch = normalize_win_arch(args.target_arch)
  arch = host_arch if host_arch == target_arch else f'{host_arch}_{target_arch}'
  env = get_vs_env_vars(arch, args.vcvarsall_path)

  # Use locally checked out ninja.exe if exists.
  ninja_dir = get_ninja_dir(args)
  if ninja_dir:
    env['PATH'] = str(ninja_dir) + os.pathsep + env['PATH']

  # Add qt_src_dir to 'PATH'.
  # https://doc.qt.io/qt-6/windows-building.html#step-3-set-the-environment-variables
  env['PATH'] = str(qt_src_dir) + os.pathsep + env['PATH']

  cmd = str(shutil.which('cmd.exe', path=env['PATH']))

  env['QT_HOST_PATH'] = str(qt_host_dir)

  configure_cmds = [cmd, '/C', 'configure.bat'] + make_configure_options(args)

  exec_command(configure_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  cmake = str(shutil.which('cmake.exe', path=env['PATH']))
  build_cmds = [cmake, '--build', '.', '--parallel']
  install_cmds = [cmake, '--install', '.']

  exec_command(build_cmds, cwd=qt_src_dir, env=env, dryrun=args.dryrun)

  if qt_src_dir != qt_dest_dir:
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

  for tool in ['moc.exe', 'rcc.exe', 'uic.exe']:
    src = qt_host_dir.joinpath('bin').joinpath(tool)
    dest = qt_dest_dir.joinpath('bin').joinpath(tool)
    if args.dryrun:
      print(f'dryrun: copy {src} => {dest}')
    else:
      shutil.copy2(src=src, dst=dest)

  if args.dryrun:
    print(f'dryrun: shutil.rmtree({qt_host_dir})')
  else:
    shutil.rmtree(qt_host_dir)


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
      # tarfile.data_filter is available in Python 3.12+.
      # See https://peps.python.org/pep-0706/ for details.
      if getattr(tarfile, 'data_filter', None):
        with StatefulQtExtractionFilter() as filter:
          f.extractall(path=qt_src_dir, filter=filter)
      else:
        f.extractall(path=qt_src_dir, members=qt_extract_filter(f))


def main():
  if is_linux():
    print('On Linux, please use shared library provided by distributions.')
    sys.exit(1)

  args = parse_args()

  if not (args.debug or args.release):
    print('neither --release nor --debug is specified.')
    sys.exit(1)

  if is_mac():
    build_host_on_mac(args)
    build_on_mac(args)
  elif is_windows():
    build_host_on_windows(args)
    build_on_windows(args)


if __name__ == '__main__':
  main()
