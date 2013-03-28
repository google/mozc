# -*- coding: utf-8 -*-
# Copyright 2010-2013, Google Inc.
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

"""Splits the specified apk file into each ABIs.

"Fat APK" contains multipul shared objects in order to run on all the ABIs.
But this means such APK is larger than "Thin" APK.
This script creates Thin APKs from Fat APK.

Version code format:
 000ABBBBBB
 A: ABI (0: Fat, 5: x86, 4: armeabi-v7a, 3: armeabi, 1:mips)
 B: Build number

Note:
- This process must be done before signing.
- ABI number 2 is unused because of historical reason.
  It had been used for x86 but x86 was assigned 5 later.
  If in the future new ABI will is introduced you can reuse 2 for it,
  but *avoid reodering*, which must make wrong installation.
"""


__author__ = "matsuzakit"

import cStringIO
import logging
import optparse
import os
import re
import shutil
import tempfile
import zipfile

from build_tools import android_binary_xml


_UNSIGNED_APK_SUFFIX = '-unsigned.apk'


class Error(Exception):
  """Base exception class."""


class UnexpectedFormatError(Error):
  pass


class IllegalArgumentError(Error):
  pass


def ParseArgs():
  parser = optparse.OptionParser()
  parser.add_option('--dir', dest='bin_dir',
                    help='Binary directory. Files of which name ends with '
                    '"-unsigned.apk" are processed.')
  options = parser.parse_args()[0]
  if not options.bin_dir:
    raise IllegalArgumentError('--dir is mandatory')
  return options


# TODO(matsuzakit): Make zip relating logics independent
#   from file-based operations.
#   Currently they are file-based for reuseabilty.
#   But file-based design is not good from the view points of
#   performance and testability
def DeleteEntriesFromZip(zip_path, delete_file_names):
  """Deletes entries from zip file.

  Args:
    zip_path: Path to zip file.
    delete_file_names: File names in archive to be deleted.
  """
  logging.info('Deleting %s from %s', delete_file_names, zip_path)
  tmp_file = cStringIO.StringIO()
  in_zip_file = zipfile.ZipFile(zip_path)
  try:
    out_zip_file = zipfile.ZipFile(tmp_file, 'w')
    try:
      for zipinfo in in_zip_file.infolist():
        if zipinfo.filename not in delete_file_names:
          # Reusing zipinfo as 1st argument is mandatory
          # because compression_type must be kept.
          out_zip_file.writestr(zipinfo,
                                in_zip_file.read(zipinfo.filename))
    finally:
      out_zip_file.close()
  finally:
    in_zip_file.close()
  with open(zip_path, 'w') as out_file:
    out_file.write(tmp_file.getvalue())


def ReplaceFilesInZip(zip_path, base_directory, file_names,
                      compress_type=zipfile.ZIP_DEFLATED):
  """Replaces files in zip file with given file_names.

  If no corresponding entries in zip file, simply appended.

  Args:
    zip_path: Path to zip file.
    base_directory: Base direcotry of file_names.
    file_names: File names to be appended.
    compress_type: zipfile.ZIP_DEFLATED or zipfile.ZIP_STORED.
  """
  DeleteEntriesFromZip(zip_path, file_names)
  logging.info('Appending %s to %s', file_names, zip_path)
  zip_file = zipfile.ZipFile(zip_path, 'a')
  try:
    for file_name in file_names:
      zip_file.write(os.path.join(base_directory, file_name),
                     file_name, compress_type)
  finally:
    zip_file.close()


def UnzipFiles(zip_path, file_names, out_dir):
  """Extracts files from zip file.

  Args:
    zip_path: Path to zip file.
    file_names: File names to be extracted.
    out_dir: Destination directory.
  Returns:
    Paths of extracted files.
  """
  logging.info('Extracting %s from %s', file_names, zip_path)
  result = []
  zip_file = zipfile.ZipFile(zip_path)
  try:
    for zip_info in zip_file.infolist():
      if zip_info.filename in file_names:
        out_path = os.path.join(out_dir, zip_info.filename)
        if not os.path.isdir(os.path.dirname(out_path)):
          os.makedirs(os.path.dirname(out_path))
        with open(out_path, 'w') as out_file:
          out_file.write(zip_file.read(zip_info.filename))
          result.append(out_path)
  finally:
    zip_file.close()
  return result


def GetVersionCode(base_version_code, abi):
  """Gets version code based on base version code and abi."""
  # armeabi-v7a's version code must be greater than armeabi's.
  # By this v7a's apk is prioritized on the Play.
  # Without this all the ARM devices download armeabi version
  # because armeabi can be run on all of them (including v7a).
  if abi == 'x86':
    abi_code = 5
  elif abi == 'armeabi-v7a':
    abi_code = 4
  elif abi == 'armeabi':
    abi_code = 3
  elif abi == 'mips':
    abi_code = 1
  else:
    raise IllegalArgumentError('Unexpected ABI; %s' % abi)
  return int('%d%06d' % (abi_code, base_version_code))


def ModifyAndroidManifestFile(apk_path, abi):
  """Modifies given apk file to make it thin apk.

  After the execution of this method,
  unneeded .so files (evaluated by given abi name)
  are removed and AndroidManifest.xml file's
  version code is modified.

  Args:
    apk_path: the path to the apk file to be modified.
    abi: the ABI name.
  Raises:
    UnexpectedFormatError: manifest element must be only one.
  """
  logging.info('Modifing %s to ABI %s', apk_path, abi)
  temp_dir_in = tempfile.mkdtemp()
  temp_dir_out = tempfile.mkdtemp()
  original_file_paths = UnzipFiles(apk_path,
                                   'AndroidManifest.xml', temp_dir_in)
  if len(original_file_paths) != 1:
    raise UnexpectedFormatError(
        'AndroidManifest.xml file is expected to be only one.')
  original_file_path = original_file_paths[0]
  document = android_binary_xml.AndroidBinaryXml(original_file_path)
  manifest_elements = document.FindElements(None, 'manifest')
  if len(manifest_elements) != 1:
    raise UnexpectedFormatError('manifest element is expected to be only one.')
  manifest_element = manifest_elements[0]
  version_code_attribute = manifest_element.GetAttribute(
      'http://schemas.android.com/apk/res/android', 'versionCode')
  base_version_code = version_code_attribute.GetIntValue()
  logging.info('new ver code %s', GetVersionCode(base_version_code, abi))
  version_code_attribute.SetIntValue(GetVersionCode(base_version_code, abi))
  document.Write(os.path.join(temp_dir_out, 'AndroidManifest.xml'))
  ReplaceFilesInZip(apk_path, temp_dir_out, ['AndroidManifest.xml'])


def GetUnneededFiles(abi_to_files, abi):
  unneeded_files = []
  for entry_abi, entry_files in abi_to_files.iteritems():
    if entry_abi != abi:
      unneeded_files.extend(entry_files)
  logging.info('Unneeded files are %s', unneeded_files)
  return unneeded_files


def CreateCopyFile(original_file, abi_name):
  # Original : Mozc-unsigned.apk
  # Copy : Mozc-x86-unsigned.apk
  copied_file = ''.join(
      [original_file[:original_file.find(_UNSIGNED_APK_SUFFIX)],
       '-', abi_name, _UNSIGNED_APK_SUFFIX])
  logging.info('Copying from %s to %s', original_file, copied_file)
  shutil.copyfile(original_file, copied_file)
  return copied_file


def CreateAbiToFileMapping(file_name):
  zip_file = zipfile.ZipFile(file_name)
  try:
    abi_to_files = {}
    for zip_info in zip_file.infolist():
      m = re.match(r'lib/(.+?)/.*', zip_info.filename)
      if m:
        files = abi_to_files.setdefault(m.group(1), [])
        files.append(zip_info.filename)
    logging.info('ABIs are: %s', abi_to_files.keys())
  finally:
    zip_file.close()
  return abi_to_files


def main():
  # Enable logging.info.
  logging.getLogger().setLevel(logging.INFO)
  options = ParseArgs()

  for apk_file in [os.path.join(options.bin_dir, f)
                   for f in os.listdir(options.bin_dir)
                   if f.endswith(_UNSIGNED_APK_SUFFIX)]:
    logging.info('Processing %s', apk_file)
    abi_to_files = CreateAbiToFileMapping(apk_file)

    for abi in abi_to_files:
      logging.info('Processing ABI: %s', abi)
      copied_file = CreateCopyFile(apk_file, abi)
      unneeded_files = GetUnneededFiles(abi_to_files, abi)
      DeleteEntriesFromZip(copied_file, unneeded_files)
      ModifyAndroidManifestFile(copied_file, abi)


if __name__ == '__main__':
  main()
