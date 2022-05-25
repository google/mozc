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

"""Copies files.

This script provides more features than 'copies' rule of GYP.
1. The destination file name can be different from the original name.
2. Is able to copy directories recursively.
"""

import optparse
import os
import shutil
import stat
import sys


def _ErrorExit(message):
  """Prints the error message and exits with the code 1.

  The function never returns.

  Args:
    message: The error message to be printed to stderr.
  """
  print(message, file=sys.stderr)
  sys.exit(1)


def CopyFiles(src_list, dst, src_base='',
              preserve=False, recursive=False,
              ignore_existence_check=False):
  """Copy files like 'cp' command on Unix.

  Args:
    src_list: List of files to be copied.
    dst: The destination file or directory.
    src_base: Base directory of the source files.
       The following path is copied to the destination.
       e.g. src: a/b/c/d/e.txt , base_dir: a/b/ -> dest: c/d/e.txt
    preserve: Preserves last update time.  Permissions for files are always
        copied.
    recursive: Copies files recursively.
    ignore_existence_check: Ignores existence check for src files.
  """
  if not src_list:
    return

  def _MakeDirs(dir):
    if not os.path.exists(dir):
      os.makedirs(dir)

  def _CopyDir(src_dir, dst_dir):
    _MakeDirs(dst_dir)

    for filename in os.listdir(src_dir):
      src = os.path.join(src_dir, filename)
      _Copy(src, dst_dir)

  def _Copy(src, dst):
    if os.path.isdir(dst):
      dst = os.path.join(dst, os.path.basename(src))

    if os.path.isdir(src):
      _CopyDir(src, dst)
    else:
      shutil.copy(src, dst)

    # Copy update time and permission
    if preserve:
      shutil.copystat(src, dst)
    # Changes the file writable so we can overwrite it later.
    os.chmod(dst, os.stat(dst).st_mode | stat.S_IWRITE)

  def _ErrorCheck(src, src_base):
    if not os.path.exists(src):
      if ignore_existence_check:
        return False
      else:
        _ErrorExit('No such file or directory: "%s"' % src)

    if os.path.isdir(src) and not recursive:
      _ErrorExit('Cannot copy a directory: "%s"' % src)

    if not src.startswith(src_base):
      _ErrorExit('Source file does not start with src_base: "%s"' % src)

    return True

  dst = os.path.abspath(dst)
  if src_base:
    src_base = os.path.abspath(src_base)

  # dst may be a file instead of a directory.
  if len(src_list) == 1 and not os.path.exists(dst) and not src_base:
    src = os.path.abspath(src_list[0])
    if _ErrorCheck(src, src_base):
      _MakeDirs(os.path.dirname(dst))
      _Copy(src, dst)
    return

  # dst should be a directory here.
  for src in src_list:
    src = os.path.abspath(src)

    if src_base:
      dst_dir = os.path.join(dst,
                             os.path.relpath(os.path.dirname(src), src_base))
    else:
      dst_dir = dst

    if _ErrorCheck(src, src_base):
      _MakeDirs(dst_dir)
      _Copy(src, dst_dir)


def _GetUpdateTime(filename):
  """Returns the update time of the file.

  Args:
    filename: Path to the file.  The file can be a directory.  Symbolic links
        are followed.

  Returns:
    The update time in the form of (atime, mtime).
  """
  stat_info = os.stat(filename)
  return (stat_info.st_atime, stat_info.st_mtime)


def main():
  parser = optparse.OptionParser(
      usage='Usage: %prog [OPTION]... SOURCE... DEST')
  parser.add_option('--src_base', dest='src_base', default='',
                    help=('Base directory of the source files. '
                          'The following path is copied to the destination.'))
  parser.add_option('--preserve', '-p', dest='preserve',
                    action='store_true', default=False,
                    help='Preserves last update time.')
  parser.add_option('--recursive', '-r', dest='recursive',
                    action='store_true', default=False,
                    help='Copies directories recursively.')
  parser.add_option('--ignore_existence_check', dest='ignore_existence_check',
                    action='store_true', default=False,
                    help='Ignore existence check for src files.')
  (options, args) = parser.parse_args()

  if len(args) < 2:
    _ErrorExit('The arguments must be source(s) and destination files.')

  src_list = args[:-1]
  dst = args[-1]

  CopyFiles(src_list, dst, src_base=options.src_base,
            preserve=options.preserve, recursive=options.recursive,
            ignore_existence_check=options.ignore_existence_check)


if __name__ == '__main__':
  main()
