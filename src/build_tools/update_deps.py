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

"""A helper script to update OSS Mozc build dependencies.

This helper script takes care of updaring build dependencies for legacy GYP
build for OSS Mozc.
"""

import argparse
from collections.abc import Iterator
import io
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import zipfile

import requests

ABS_SCRIPT_PATH = pathlib.Path(__file__).absolute()
# src/build_tools/fetch_deps.py -> src/
ABS_MOZC_SRC_DIR = ABS_SCRIPT_PATH.parents[1]
ABS_THIRD_PARTY_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party')

QT_ARCHIVE_URL = 'https://download.qt.io/archive/qt/5.15/5.15.9/submodules/qtbase-everywhere-opensource-src-5.15.9.tar.xz'
JOM_ARCHIVE_URL = 'https://download.qt.io/official_releases/jom/jom_1_1_3.zip'
WIX_ARCHIVE_URL = 'https://wixtoolset.org/downloads/v3.14.0.6526/wix314-binaries.zip'


def IsWindows() -> bool:
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def IsMac() -> bool:
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def DownloadQt(third_party_dir: pathlib.Path, dryrun: bool = False) -> None:
  """Download Qt archive file and extract it.

  Args:
    third_party_dir: third_party dir under which qt dir will be placed.
    dryrun: true to perform dryrun.
  """
  qt_dir = third_party_dir.joinpath('qt').absolute()
  if qt_dir.exists():
    if dryrun:
      print(f"dryrun: shutil.rmtree(r'{qt_dir}')")
    else:
      shutil.rmtree(qt_dir)
  isatty = sys.stdout.isatty()
  colmuns = os.get_terminal_size().columns if isatty else None

  def QtExtractFilter(
      members: Iterator[tarfile.TarInfo],
  ) -> Iterator[tarfile.TarInfo]:
    for info in members:
      paths = info.name.split('/')
      if '..' in paths:
        continue
      if len(paths) < 1:
        continue
      if len(paths) >= 2 and paths[1] in ['examples']:
        continue
      info.name = '/'.join(paths[1:])
      if isatty:
        msg = 'extracting ' + info.name
        msg = msg + ' ' * max(colmuns - len(msg), 0)
        msg = msg[0:(colmuns)] + '\r'
        sys.stdout.write(msg)
        sys.stdout.flush()
      if dryrun:
        continue
      else:
        yield info
  print('Downloading', QT_ARCHIVE_URL)
  with requests.get(QT_ARCHIVE_URL, stream=True) as r:
    with tarfile.open(fileobj=r.raw, mode='r|xz') as f:
      f.extractall(path=qt_dir, members=QtExtractFilter(f))

  # For Windows, also check out jom.exe
  if IsWindows():
    print('Downloading', JOM_ARCHIVE_URL)
    with requests.get(JOM_ARCHIVE_URL) as r:
      with zipfile.ZipFile(io.BytesIO(r.content)) as f:
        if not dryrun:
          f.extract('jom.exe', path=qt_dir)


def DownloadWiX(third_party_dir: pathlib.Path, dryrun: bool = False) -> None:
  """Download WiX archive file and extract it.

  Args:
    third_party_dir: third_party dir under which wix dir will be placed.
    dryrun: true to perform dryrun.
  """
  wix_dir = third_party_dir.joinpath('wix').absolute()
  if wix_dir.exists():
    if dryrun:
      print(f"dryrun: shutil.rmtree(r'{wix_dir}')")
    else:
      shutil.rmtree(wix_dir)

  print('Downloading', WIX_ARCHIVE_URL)
  with requests.get(WIX_ARCHIVE_URL) as r:
    with zipfile.ZipFile(io.BytesIO(r.content)) as f:
      if not dryrun:
        f.extractall(path=wix_dir)


def RunUpdateSubmodules(dryrun: bool = False) -> None:
  """Run 'git submodule update --init --recursive'.

  Args:
    dryrun: true to perform dryrun.
  """
  command = ' '.join(['git', 'submodule', 'update', '--init', '--recursive'])
  if dryrun:
    print(f'dryrun: subprocess.run({command}, shell=True, check=True)')
  else:
    print(f'Running {command}')
    subprocess.run(command, shell=True, check=True)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--dryrun', action='store_true', default=False)
  parser.add_argument('--noqt', action='store_true', default=False)
  parser.add_argument('--nowix', action='store_true', default=False)
  parser.add_argument('--nosubmodules', action='store_true', default=False)
  parser.add_argument('--third_party_dir', help='third_party directory',
                      type=str, default=ABS_THIRD_PARTY_DIR)
  args = parser.parse_args()
  third_party_dir = pathlib.Path(args.third_party_dir)
  if (not args.noqt) and (IsWindows() or IsMac()):
    DownloadQt(third_party_dir, args.dryrun)
  if (not args.nowix) and IsWindows():
    DownloadWiX(third_party_dir, args.dryrun)
  if not args.nosubmodules:
    RunUpdateSubmodules(args.dryrun)


if __name__ == '__main__':
  main()
