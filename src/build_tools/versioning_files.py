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

r"""Creates versioned files and information files of the files.

  When the version is "1.2.3.4", the following command creates these four files.
    /PATH/TO/file1-1.2.3.4.ext      : Copied from /PATH/TO/file1.ext
    /PATH/TO/file1-1.2.3.4.info.ext : File information about file1.ext
    /PATH/TO/file2-1.2.3.4          : Copied from /PATH/TO/file2
    /PATH/TO/file2-1.2.3.4.info     : File information about file2

  % python versioning_files.py --version_file mozc_version.txt \
      --configuration Release /PATH/TO/file1.ext /PATH/TO/file2

  If configuration is "Debug" these file names will be
    "/PATH/TO/file1-Debug-1.2.3.4.ext"
    "/PATH/TO/file1-Debug-1.2.3.4.info.ext"
    "/PATH/TO/file2-Debug-1.2.3.4"
    "/PATH/TO/file2-Debug-1.2.3.4.info"
"""


import base64
import hashlib
import logging
import optparse
import os
import shutil

from build_tools import mozc_version


def _GetSha1Digest(file_path):
  """Returns the sha1 hash of the file."""
  sha = hashlib.sha1()
  with open(file_path, 'rb') as f:
    data = f.read()
    sha.update(data)
  # NOTE: The linter complains that sha1 gets too many args.
  # pylint: disable=too-many-function-args
  return sha.digest()


def _GetBuildId():
  """Returns the build ID or an empty string."""
  envvar_list = []

  for envvar in envvar_list:
    # If the value is empty, it should be skipped.
    if os.environ.get(envvar):
      return os.environ[envvar]
  return ''


def _VersioningFile(version_string, is_debug, file_path):
  """Creates a versioned file and a information file of the file."""
  build_id = _GetBuildId()
  file_path_base, ext = os.path.splitext(file_path)
  if is_debug:
    file_path_base += '-Debug'
  new_file_path = '%s-%s%s' % (file_path_base, version_string, ext)
  shutil.copyfile(file_path, new_file_path)
  package = os.path.basename(new_file_path)
  file_size = os.path.getsize(new_file_path)
  sha1_digest = _GetSha1Digest(new_file_path)
  sha1_hash = base64.b64encode(sha1_digest).decode('latin1')
  sha1_hash_hex = sha1_digest.hex()
  with open('%s.info' % new_file_path, 'w', newline='\n') as output:
    output.write('package\t%s\n' % package)
    output.write('build_id\t%s\n' % build_id)
    output.write('version\t%s\n' % version_string)
    output.write('size\t%s\n' % file_size)
    output.write('sha1_hash\t%s\n' % sha1_hash)
    output.write('sha1_hash_hex\t%s\n' % sha1_hash_hex)


def VersioningFiles(version_string, is_debug, file_paths):
  """Creates versioned files and information files of the files."""
  for file_path in file_paths:
    _VersioningFile(version_string, is_debug, file_path)


def main():
  """The main function."""
  parser = optparse.OptionParser()
  parser.add_option('--version_file', dest='version_file')
  parser.add_option('--configuration', dest='configuration')

  (options, args) = parser.parse_args()
  if options.version_file is None:
    logging.error('--version_file is not specified.')
    exit(-1)
  if options.configuration is None:
    logging.error('--configuration is not specified.')
    exit(-1)
  version = mozc_version.MozcVersion(options.version_file)
  version_string = version.GetVersionString()
  is_debug = (options.configuration == 'Debug')
  VersioningFiles(version_string, is_debug, args)

if __name__ == '__main__':
  main()
