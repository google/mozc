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

r"""Script to invoke protoc with considering project root directory.

  % python protoc_wrapper.py               \
      --protoc_command=protoc              \
      --protoc_dir=/usr/bin    (optional)  \
      --proto=../protos/my_data.proto      \
      --proto_path=../protos   (optional)  \
      --cpp_out=../out/debug/gen           \
      --project_root=../
"""

__author__ = "yukawa"

import optparse
import os
import subprocess
import sys


def ParseOption():
  """Parse command line options."""
  parser = optparse.OptionParser()
  parser.add_option('--protoc_command', dest='protoc_command',
                    help='executable name of protoc')
  parser.add_option('--protoc_dir', dest='protoc_dir',
                    help='directory where protoc is located')
  parser.add_option('--proto', dest='proto', help='path of the *.proto file')
  parser.add_option('--cpp_out', dest='cpp_out', default='',
                    help='path where cpp files should be generated')
  parser.add_option('--project_root', dest='project_root', default='.',
                    help='run protoc after moving this directory')
  parser.add_option('--proto_path', dest='proto_path', default='',
                    help='directory to be passed to --proto_path option')

  (opts, _) = parser.parse_args()

  return opts


def CreateProtoH(cpp_out, proto_file):
  proto_h = os.path.join(cpp_out, proto_file + '.h')
  pb_h = proto_file[:-len('.proto')] + '.pb.h'
  with open(proto_h, 'w') as output:
    output.write('#include "%s"\n' % pb_h)


def main():
  """The main function."""
  opts = ParseOption()

  # Convert to absolute paths before changing the current directory.
  project_root = os.path.abspath(opts.project_root)
  proto_path = os.path.abspath(opts.proto_path) if opts.proto_path else ''
  cpp_out = os.path.abspath(opts.cpp_out) if opts.cpp_out else ''

  protoc_path = opts.protoc_command
  if opts.protoc_dir:
    protoc_path = os.path.join(os.path.abspath(opts.protoc_dir), protoc_path)

  # The path of proto file should be transformed as a relative path from
  # the project root so that correct relative paths should be embedded into
  # generated files.
  proto_files = [os.path.relpath(os.path.abspath(p), project_root)
                 for p in opts.proto.split(' ')]

  # Move to the project root.
  os.chdir(project_root)

  commands = [protoc_path] + proto_files
  if cpp_out:
    commands += ['--cpp_out=' + cpp_out]
  if proto_path:
    rel_proto_path = os.path.relpath(proto_path, project_root)
    commands += ['--proto_path=' + rel_proto_path]

  for proto_file in proto_files:
    CreateProtoH(cpp_out, proto_file)

  sys.exit(subprocess.call(commands))


if __name__ == '__main__':
  main()
