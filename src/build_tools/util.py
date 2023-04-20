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

"""Utilitis for build_mozc script."""

import logging
import multiprocessing
import os
import shutil
import subprocess
import sys
import zipfile


def IsWindows():
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def IsMac():
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def IsLinux():
  """Returns true if the platform is Linux."""
  return os.name == 'posix' and os.uname()[0] == 'Linux'


def CaseAwareAbsPath(path: str) -> str:
  """Wraps os.path.abspath() with case normalization.

  On Windows the return value of os.path.abspath(path) may have different
  upper/lowercase letters than the actual ones in the filesystem.
  This function normalizes them to be consistent with the filesystem.

  This function just returns os.path.abspath(path) on Linux with an assumption
  that the filesystem is always case-sensitive.

  This function does not normalize the result if 'path' contains any symlink.

  See https://github.com/google/mozc/issues/719.

  See also https://bugs.python.org/issue40368.

  Args:
    path: path to be passed to os.path.abspath() then normalized.
  Returns:
    os.path.abspath(path) with normalizing the result to be consistent with the
    filesystem.
  """
  abs_path = os.path.abspath(path)
  # Assume filesystem is always case-sensitive on Linux.
  if IsLinux():
    return abs_path

  real_path = os.path.realpath(path)
  if abs_path == real_path:
    # Already the same. Nothing to worry about.
    return abs_path

  if abs_path.casefold() == real_path.casefold():
    # Only different in cases. Use real_path to normalize it.
    return real_path

  # abs_path probably contains symlink.
  logging.warning(
      (
          'Could not noramlize abs_path: %s (path: %s, real_path: %s), but you'
          ' can still ignore this warning on case-sensitive filesystem.'
      ),
      abs_path,
      path,
      real_path,
  )
  return abs_path


def GetNumberOfProcessors():
  """Returns the number of CPU cores available.

  Returns:
    An integer which represents the number of CPU cores available on
    the host environment. Returns 1 if something fails.
  """
  try:
    return multiprocessing.cpu_count()
  except NotImplementedError as e:
    logging.warning('Failed to retrieve the CPU count. Error: %s.', e)
  return 1


class RunOrDieError(Exception):
  """The exception class for RunOrDie."""

  def __init__(self, message):
    Exception.__init__(self, message)


def RunOrDie(argv):
  """Run the command, or die if it failed."""
  # Rest are the target program name and the parameters, but we special
  # case if the target program name ends with '.py'
  if argv[0].endswith('.py'):
    argv.insert(0, sys.executable)  # Inject the python interpreter path.
  # We don't capture stdout and stderr from Popen. The output will just
  # be emitted to a terminal or console.
  logging.info('Running: %s', ' '.join(argv))
  process = subprocess.Popen(argv)

  if process.wait() != 0:
    error_label = ColoredText('ERROR', logging.ERROR)
    raise RunOrDieError('\n'.join(['',
                                   '==========',
                                   ' %s: %s' % (error_label, ' '.join(argv)),
                                   '==========']))


def RemoveFile(file_name):
  """Removes the specified file."""
  if not (os.path.isfile(file_name) or os.path.islink(file_name)):
    return  # Do nothing if not exist.
  if IsWindows():
    # Read-only files cannot be deleted on Windows.
    os.chmod(file_name, 0o700)
  logging.debug('Removing file: %s', file_name)
  os.unlink(file_name)


def CopyFile(source, destination):
  """Copies a file to the destination. Remove an old version if needed."""
  if os.path.isfile(destination):  # Remove the old one if exists.
    RemoveFile(destination)
  dest_dirname = os.path.dirname(destination)
  if not os.path.isdir(dest_dirname):
    os.makedirs(dest_dirname)
  logging.info('Copying file to: %s', destination)
  shutil.copy(source, destination)


def RemoveDirectoryRecursively(directory):
  """Removes the specified directory recursively."""
  if os.path.isdir(directory):
    logging.debug('Removing directory: %s', directory)
    if IsWindows():
      # Use RD because shutil.rmtree fails when the directory is readonly.
      RunOrDie(['CMD.exe', '/C', 'RD', '/S', '/Q',
                os.path.normpath(directory)])
    else:
      shutil.rmtree(directory, ignore_errors=True)


def PrintErrorAndExit(error_message):
  """Prints the error message and exists."""
  logging.critical('\n==========\n%s\n==========', error_message)
  sys.exit(1)


class _ZipFileWithPermissions(zipfile.ZipFile):
  """Subclass of zipfile.ZipFile to support permissions."""

  def _extract_member(self, member, targetpath, pwd):
    """Calls superclass's function, then keep the file permissions."""
    if not isinstance(member, zipfile.ZipInfo):
      member = self.getinfo(member)

    # _extract_member is only available in Python3.6 and later.
    # TODO(b/179457623): Support ZipFileWithPermission for Python3.5 and former.
    if hasattr(super(), '_extract_member'):
      targetpath = super()._extract_member(member, targetpath, pwd)

    attr = member.external_attr >> 16
    if attr != 0:
      os.chmod(targetpath, attr)
    return targetpath


def ExtractZip(zip_path, out_dir):
  with _ZipFileWithPermissions(zip_path) as zip_file:
    zip_file.extractall(path=out_dir)


# ANSI Color sequences
_TERMINAL_COLORS = {
    'CLEAR': '\x1b[0m',
    'BLACK': '\x1b[30m',
    'RED': '\x1b[31m',
    'GREEN': '\x1b[32m',
    'YELLOW': '\x1b[33m',
    'BLUE': '\x1b[34m',
    'MAGENTA': '\x1b[35m',
    'CYAN': '\x1b[36m',
    'WHITE': '\x1b[37m',
}

# Indicates the output stream supports colored text or not.
# It is disabled on windows because cmd.exe doesn't support ANSI color escape
# sequences.
# TODO(team): Considers to use ctypes.windll.kernel32.SetConsoleTextAttribute
#             on Windows. b/6260694
_COLORED_TEXT_SUPPORT = (
    not IsWindows() and sys.stdout.isatty() and sys.stderr.isatty())


def ColoredText(text, level):
  """Generates a colored text according to log level."""
  if not _COLORED_TEXT_SUPPORT:
    return text

  if level <= logging.INFO:
    color = 'GREEN'
  elif level <= logging.WARNING:
    color = 'YELLOW'
  else:
    color = 'RED'

  return _TERMINAL_COLORS[color] + text + _TERMINAL_COLORS['CLEAR']


class ColoredLoggingFilter(logging.Filter):
  """Supports colored text on logging library.

  Sample code.
  logging.getLogger().addFilter(util.ColoredLoggingFilter())
  """

  def filter(self, record):
    level_name = record.levelname
    level_no = record.levelno
    if level_name and level_no:
      record.levelname = ColoredText(level_name, level_no)

    return True
