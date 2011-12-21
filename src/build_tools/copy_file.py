# -*- coding: utf-8 -*-
# Copyright 2010-2011, Google Inc.
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
2. Changes the last update time according to a reference file.
(This is the same as `touch -r REFERENCE_FILE'.)
3. Is able to copy directories recursively.
"""

__author__ = "yukishiino"

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
  print >>sys.stderr, message
  sys.exit(1)


def CopyFiles(src_list, dst, src_base='',
              preserve=False, recursive=False, reference=None):
  """Copy files like 'cp' command on Unix.

  Args:
    src_list: List of files to be copied.
    dst: The destination file or directory.
    src_base: The base directory which holds |src_list|.
    preserve: Preserves last update time.  Permissions for files are always
        copied.
    recursive: Copies files recursively.
    reference: Last update time in the form of (atime, mtime) to be copied
        to files.  The reference time is prioritized over preserving.
  """
  if not src_list:
    return

  def _CopyFile(src_file, dst_file):
    if os.path.isdir(dst_file):
      _ErrorExit('An unexpected dir `%s\' exists' % dst_file)
    shutil.copy(src_file, dst_file)
    _CopyUpdateTimeAndPermission(src_file, dst_file)

  def _CopyDir(src_dir, dst_dir, file_list=None):
    if file_list is None:
      file_list = os.listdir(src_dir)
    for filename in file_list:
      src = os.path.join(src_dir, filename)
      dst = os.path.join(dst_dir, filename)
      _Copy(src, dst)

  def _Copy(src, dst):
    if os.path.isdir(src):
      if not recursive:
        _ErrorExit('Cannot copy a directory `%s\'' % src)
      if not os.path.isdir(dst):
        os.mkdir(dst)
        _CopyDir(src, dst)
        _CopyUpdateTimeAndPermission(src, dst)
      else:
        _CopyDir(src, dst)
    else:
      _CopyFile(src, dst)

  def _CopyUpdateTimeAndPermission(src, dst):
    if preserve:
      shutil.copystat(src, dst)
    if reference:
      os.utime(dst, reference)
    # Changes the file writable so we can overwrite it later.
    os.chmod(dst, os.stat(dst).st_mode | stat.S_IWRITE)

  # Create the parent directory of dst.
  dst_parent = os.path.abspath(os.path.dirname(dst))
  if dst_parent and not os.path.exists(dst_parent):
    os.makedirs(dst_parent)

  if len(src_list) is 1:
    src = os.path.join(src_base, src_list[0])
    if os.path.isdir(dst):
      dst = os.path.join(dst, os.path.basename(os.path.abspath(src)))
    _Copy(src, dst)
  else:  # len(src_list) > 1
    if not os.path.isdir(dst):
      os.mkdir(dst)
    for src in src_list:
      src = os.path.abspath(os.path.join(src_base, src))
      (src_dir, src_file) = os.path.split(src)
      _CopyDir(src_dir, dst, [src_file])


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
  parser.add_option('--preserve', '-p', dest='preserve',
                    action='store_true', default=False,
                    help='Preserves last update time.')
  parser.add_option('--recursive', '-r', dest='recursive',
                    action='store_true', default=False,
                    help='Copies directories recursively.')
  parser.add_option('--reference', dest='reference',
                    help='Uses this file\'s last update time.')
  (options, args) = parser.parse_args()

  if len(args) < 2:
    _ErrorExit('The arguments must be source(s) and destination files.')

  reference_time = (_GetUpdateTime(options.reference)
                    if options.reference else None)

  src_list = args[:-1]
  dst = args[-1]

  CopyFiles(src_list, dst,
            preserve=options.preserve, recursive=options.recursive,
            reference=reference_time)


if __name__ == '__main__':
  main()
