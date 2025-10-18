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
import dataclasses
import hashlib
import os
import pathlib
import shutil
import stat
import subprocess
import tarfile
from typing import Union
import urllib.error
import urllib.request
import zipfile

from progress_printer import ProgressPrinter


ABS_SCRIPT_PATH = pathlib.Path(__file__).absolute()
# src/build_tools/fetch_deps.py -> src/
ABS_MOZC_SRC_DIR = ABS_SCRIPT_PATH.parents[1]
ABS_THIRD_PARTY_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party')
CACHE_DIR = ABS_MOZC_SRC_DIR.joinpath('third_party_cache')
TIMEOUT = 600


@dataclasses.dataclass
class ArchiveInfo:
  """Third party archive file to be used to build Mozc binaries.

  Attributes:
    url: URL of the archive.
    size: File size of the archive.
    sha256: SHA-256 of the archive.
  """
  url: str
  size: int
  sha256: str

  @property
  def filename(self) -> str:
    """The filename of the archive."""
    return self.url.split('/')[-1]

  def __hash__(self):
    return hash(self.sha256)


QT6 = ArchiveInfo(
    url='https://download.qt.io/archive/qt/6.9/6.9.1/submodules/qtbase-everywhere-src-6.9.1.tar.xz',
    size=49755912,
    sha256='40caedbf83cc9a1959610830563565889878bc95f115868bbf545d1914acf28e',
)

NDK_LINUX = ArchiveInfo(
    url='https://dl.google.com/android/repository/android-ndk-r29-linux.zip',
    size=783549481,
    sha256='4abbbcdc842f3d4879206e9695d52709603e52dd68d3c1fff04b3b5e7a308ecf',
)

NDK_MAC = ArchiveInfo(
    url='https://dl.google.com/android/repository/android-ndk-r29-darwin.zip',
    size=1049519838,
    sha256='ce5e4b100ec5fe5be4eb3edcb2c02528824ff9cda3860f5304619be6c3da34d3',
)

NINJA_MAC = ArchiveInfo(
    url='https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-mac.zip',
    size=277298,
    sha256='21915277db59756bfc61f6f281c1f5e3897760b63776fd3d360f77dd7364137f',
)

NINJA_WIN = ArchiveInfo(
    url='https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-win.zip',
    size=285411,
    sha256='d0ee3da143211aa447e750085876c9b9d7bcdd637ab5b2c5b41349c617f22f3b',
)

LLVM_WIN = ArchiveInfo(
    url='https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.1/clang+llvm-20.1.1-x86_64-pc-windows-msvc.tar.xz',
    size=939286624,
    sha256='f8114cb674317e8a303731b1f9d22bf37b8c571b64f600abe528e92275ed4ace',
)

MSYS2 = ArchiveInfo(
    url='https://github.com/msys2/msys2-installer/releases/download/2025-02-21/msys2-base-x86_64-20250221.tar.xz',
    size=50089256,
    sha256='850589091e731d14b234447084737ca62aee1cc1e3c10be62fcdc12b8263d70b',
)


def get_sha256(path: pathlib.Path) -> str:
  """Returns SHA-256 hash digest of the specified file.

  Args:
    path: Local path the file to calculate SHA-256 about.
  Returns:
    SHA-256 hash digestd of the specified file.
  """
  with open(path, 'rb') as f:
    try:
      # hashlib.file_digest is available in Python 3.11+
      return hashlib.file_digest(f, 'sha256').hexdigest()
    except AttributeError:
      # Fallback to f.read().
      h = hashlib.sha256()
      h.update(f.read())
      return h.hexdigest()


def download(archive: ArchiveInfo, dryrun: bool = False) -> None:
  """Download the specified file.

  Args:
    archive: ArchiveInfo to be downloaded.
    dryrun: True if this is a dry-run.

  Raises:
    RuntimeError: When the downloaded file looks to be corrupted.
  """

  path = CACHE_DIR.joinpath(archive.filename)
  if path.exists():
    if (
        path.stat().st_size == archive.size
        and get_sha256(path) == archive.sha256
    ):
      # Cache hit.
      return
    else:
      if dryrun:
        print(f'dryrun: Verification failed. removing {path}')
      else:
        path.unlink()

  if dryrun:
    print(f'Download {archive.url} to {path}')
    return

  CACHE_DIR.mkdir(parents=True, exist_ok=True)
  saved = 0
  hasher = hashlib.sha256()

  try:
    req = urllib.request.Request(archive.url)
    with urllib.request.urlopen(req, timeout=TIMEOUT) as response:
      with ProgressPrinter() as printer:
        with open(path, 'wb') as f:
          chunk_size = 8192
          while True:
            chunk = response.read(chunk_size)
            if not chunk:
              break
            f.write(chunk)
            hasher.update(chunk)
            saved += len(chunk)
            printer.print_line(f'{archive.filename}: {saved}/{archive.size}')
  except urllib.error.URLError as e:
    raise RuntimeError(f'Failed to download {archive.url}: {e}') from e

  if saved != archive.size:
    raise RuntimeError(
        f'{archive.filename} size mismatch.'
        f' expected={archive.size} actual={saved}'
    )
  actual_sha256 = hasher.hexdigest()
  if actual_sha256 != archive.sha256:
    raise RuntimeError(
        f'{archive.filename} sha256 mismatch.'
        f' expected={archive.sha256} actual={actual_sha256}'
    )


def _remove_readonly(func, path, _):
  os.chmod(path, stat.S_IWRITE)
  func(path)


def remove_directory(path: os.PathLike[str]):
  """Remove directory with ignoring read-only attribute.

  Args:
    path: the directory path to be deleted.
  """
  # 'onexc' is added in Python 3.12.
  if 'onexc' in shutil.rmtree.__code__.co_varnames:
    shutil.rmtree(path, onexc=_remove_readonly)
  else:
    shutil.rmtree(path, onerror=_remove_readonly)


def llvm_extract_filter(
    members: Iterator[tarfile.TarInfo],
) -> Iterator[tarfile.TarInfo]:
  """Custom extract filter for the LLVM Tar file.

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
      skipping = True
      if (
          len(paths) == 3
          and paths[1] == 'bin'
          and paths[2] in ['clang-cl.exe', 'llvm-lib.exe', 'lld-link.exe']
      ):
        skipping = False
      elif len(paths) >= 2 and paths[1] in ['include', 'lib']:
        skipping = False
      if skipping:
        printer.print_line('skipping   ' + info.name)
        continue
      printer.print_line('extracting ' + info.name)
      yield info


class StatefulLLVMExtractionFilter:
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
      return None

    skipping = True
    paths = member.name.split('/')
    if (
        len(paths) == 3
        and paths[1] == 'bin'
        and paths[2] in ['clang-cl.exe', 'llvm-lib.exe', 'lld-link.exe']
    ):
      skipping = False
    elif len(paths) >= 2 and paths[1] in ['include', 'lib']:
      skipping = False
    if skipping:
      self.printer.print_line('skipping   ' + member.name)
      return None
    self.printer.print_line('extracting ' + member.name)
    return member


def extract_llvm(dryrun: bool = False) -> None:
  """Extract LLVM archive.

  Args:
    dryrun: True if this is a dry-run.
  """
  if not is_windows():
    return

  archive = LLVM_WIN
  src = CACHE_DIR.joinpath(archive.filename)
  dest = ABS_THIRD_PARTY_DIR.joinpath('llvm').absolute()

  if dest.exists():
    if dryrun:
      print(f"dryrun: remove_directory(r'{dest}')")
    else:
      remove_directory(dest)

  if dryrun:
    print(f'dryrun: Extracting {src} into {dest}')
  else:
    dest.mkdir(parents=True)
    with tarfile.open(src, mode='r|xz') as f:
      # tarfile.data_filter is available in Python 3.12+.
      # See https://peps.python.org/pep-0706/ for details.
      if getattr(tarfile, 'data_filter', None):
        with StatefulLLVMExtractionFilter() as filter:
          f.extractall(path=dest, filter=filter)
      else:
        f.extractall(path=dest, members=llvm_extract_filter(f))


def msys2_extract_filter(
    members: Iterator[tarfile.TarInfo],
) -> Iterator[tarfile.TarInfo]:
  """Custom extract filter for the msys2 Tar file.

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
      if len(paths) < 2:
        continue
      paths = paths[1:]
      new_path = '/'.join(paths)
      skipping = paths[0] not in ['etc', 'home', 'opt', 'tmp', 'usr', 'var']
      if skipping:
        printer.print_line('skipping   ' + new_path)
        continue
      else:
        printer.print_line('extracting ' + new_path)
        info.name = new_path
        yield info


class StatefulMSYS2ExtractionFilter:
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
      return None

    paths = member.name.split('/')
    if len(paths) < 2:
      return None
    paths = paths[1:]
    new_path = '/'.join(paths)
    skipping = paths[0] not in ['etc', 'home', 'opt', 'tmp', 'usr', 'var']
    if skipping:
      self.printer.print_line('skipping   ' + new_path)
      return None
    self.printer.print_line('extracting ' + new_path)
    return member.replace(name=new_path, deep=False)


def extract_msys2(archive: ArchiveInfo, dryrun: bool = False) -> None:
  """Extract MSSY2 archive.

  Args:
    archive: MSYS2 archive.
    dryrun: True if this is a dry-run.
  """
  if not is_windows():
    return

  src = CACHE_DIR.joinpath(archive.filename)
  dest = ABS_THIRD_PARTY_DIR.joinpath('msys64').absolute()

  if dest.exists():
    if dryrun:
      print(f"dryrun: remove_directory(r'{dest}')")
    else:
      remove_directory(dest)

  if dryrun:
    print(f'dryrun: Extracting {src} into {dest}')
  else:
    dest.mkdir(parents=True)
    with tarfile.open(src, mode='r|xz') as f:
      # tarfile.data_filter is available in Python 3.12+.
      # See https://peps.python.org/pep-0706/ for details.
      if getattr(tarfile, 'data_filter', None):
        with StatefulMSYS2ExtractionFilter() as filter:
          f.extractall(path=dest, filter=filter)
      else:
        f.extractall(path=dest, members=msys2_extract_filter(f))


def extract_ninja(dryrun: bool = False) -> None:
  """Extract ninja-win archive.

  Args:
    dryrun: True if this is a dry-run.
  """
  dest = ABS_THIRD_PARTY_DIR.joinpath('ninja').absolute()
  if is_mac():
    archive = NINJA_MAC
    exe = 'ninja'
  elif is_windows():
    archive = NINJA_WIN
    exe = 'ninja.exe'
  else:
    return
  src = CACHE_DIR.joinpath(archive.filename)

  if dryrun:
    print(f'dryrun: Extracting {exe} from {src} into {dest}')
    return

  with zipfile.ZipFile(src) as z:
    z.extract(exe, path=dest)

  if is_mac():
    ninja = dest.joinpath(exe)
    ninja.chmod(ninja.stat().st_mode | stat.S_IXUSR)


def extract_ndk(archive: ArchiveInfo, dryrun: bool = False) -> None:
  """Extract Android NDK archive.

  Args:
    archive: Android NDK archive
    dryrun: True if this is a dry-run.
  """
  dest = ABS_THIRD_PARTY_DIR.joinpath('ndk').absolute()
  src = CACHE_DIR.joinpath(archive.filename)

  if dest.exists():
    if dryrun:
      print(f"dryrun: remove_directory(r'{dest}')")
    else:
      remove_directory(dest)

  if dryrun:
    print(f"dryrun: mkdir -p '{dest}')")
  else:
    dest.mkdir(parents=True, exist_ok=True)

  # Python's zipfile doesn't support symlink.
  # We have to fallback to unzip command here.
  args = ['unzip', str(src)]

  if dryrun:
    print(f'dryrun: exec_command({args}, cwd={dest})')
  else:
    exec_command(args, cwd=dest, progress=True)
  return


def is_windows() -> bool:
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def is_mac() -> bool:
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def is_linux() -> bool:
  """Returns true if the platform is Linux."""
  return os.name == 'posix' and os.uname()[0] == 'Linux'


def update_submodules(dryrun: bool = False) -> None:
  """Run 'git submodule update --init --recursive'.

  Args:
    dryrun: true to perform dryrun.
  """
  command = ' '.join(['git', 'submodule', 'update', '--init', '--recursive'])
  if dryrun:
    print(f'dryrun: subprocess.run({command}, shell=True, check=True)')
  else:
    subprocess.run(command, shell=True, check=True)


def exec_command(
    args: list[str], cwd: os.PathLike[str], progress: bool = False
) -> None:
  """Runs the given command then returns the output.

  Args:
    args: The command to be executed.
    cwd: The working directory to execute the command.
    progress: True to show progress.

  Raises:
    ChildProcessError: When the given command cannot be executed.
  """
  process = subprocess.Popen(
      args,
      cwd=cwd,
      encoding='utf-8',
      shell=False,
      stdout=subprocess.PIPE,
      text=True,
  )
  if progress:
    with ProgressPrinter() as printer:
      for line in process.stdout:
        line = line.strip()
        printer.print_line(line)
        exitcode = process.poll()
        if exitcode is None:
          continue
        if exitcode == 0:
          return
        raise ChildProcessError(
            f'Failed to execute {args}. exitcode={exitcode}'
        )
    return
  _, _ = process.communicate()
  exitcode = process.wait()
  if exitcode != 0:
    raise ChildProcessError(f'Failed to execute {args}')


def restore_dotnet_tools(dryrun: bool = False) -> None:
  """Run 'dotnet tool restore'.

  Args:
    dryrun: true to perform dryrun.
  """
  args = ['dotnet', 'tool', 'restore']
  if dryrun:
    print(f'dryrun: exec_command({args}, cwd={ABS_MOZC_SRC_DIR})')
  else:
    exec_command(args, cwd=ABS_MOZC_SRC_DIR)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--dryrun', action='store_true', default=False)
  parser.add_argument('--noninja', action='store_true', default=False)
  parser.add_argument('--noqt', action='store_true', default=False)
  parser.add_argument('--nollvm', action='store_true', default=False)
  parser.add_argument('--nomsys2', action='store_true', default=False)
  parser.add_argument('--nowix', action='store_true', default=False)
  parser.add_argument('--nondk', action='store_true', default=False)
  parser.add_argument('--nosubmodules', action='store_true', default=False)
  parser.add_argument('--cache_only', action='store_true', default=False)

  args = parser.parse_args()

  archives = []
  if (not args.noqt) and (is_windows() or is_mac()):
    archives.append(QT6)
  if (not args.noninja):
    if is_mac():
      archives.append(NINJA_MAC)
    elif is_windows():
      archives.append(NINJA_WIN)
  if not args.nondk:
    if is_linux():
      archives.append(NDK_LINUX)
    elif is_mac():
      archives.append(NDK_MAC)
  if is_windows():
    if not args.nollvm:
      archives.append(LLVM_WIN)
    if not args.nomsys2:
      archives.append(MSYS2)

  for archive in archives:
    download(archive, args.dryrun)

  if args.cache_only:
    return

  if LLVM_WIN in archives:
    extract_llvm(args.dryrun)

  if MSYS2 in archives:
    extract_msys2(MSYS2, args.dryrun)

  if (not args.nowix) and is_windows():
    restore_dotnet_tools(args.dryrun)

  if (NINJA_WIN in archives) or (NINJA_MAC in archives):
    extract_ninja(args.dryrun)

  for ndk in [NDK_LINUX, NDK_MAC]:
    if ndk in archives:
      extract_ndk(ndk, args.dryrun)

  if not args.nosubmodules:
    update_submodules(args.dryrun)


if __name__ == '__main__':
  main()
