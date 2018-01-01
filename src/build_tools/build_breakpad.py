# -*- coding: utf-8 -*-
# Copyright 2010-2018, Google Inc.
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

"""Script building Breakpad for Mozc/Mac.

././tools/build_breakpad.py
  --pbdir ./third_party/breakpad --outdir /tmp/breakpad
"""

import optparse
import os
import subprocess
import sys


def ParseOption():
  parser = optparse.OptionParser()
  parser.add_option('--pbdir', default='./third_party/breakpad')
  parser.add_option('--outdir', default='./out_mac/Release/Breakpad')
  parser.add_option('--sdk', default='macosx10.11')

  (opts, _) = parser.parse_args()
  return opts


def ProcessCall(command):
  try:
    subprocess.check_output(command)
  except subprocess.CalledProcessError as e:
    print e.output
    sys.exit(e.returncode)
  print 'Done: %s' % ' '.join(command)


def Xcodebuild(projdir, target, arch, sdk, outdir):
  ProcessCall([
      'xcodebuild', '-project', projdir, '-configuration', 'Release',
      '-target', target, '-arch', arch, '-sdk', sdk,
      'GCC_VERSION=com.apple.compilers.llvm.clang.1_0',
      'CONFIGURATION_BUILD_DIR=%s' % outdir,
  ])


def BuildBreakpad(outdir, sdk):
  projdir = os.path.join(outdir, 'src/client/mac/Breakpad.xcodeproj')
  Xcodebuild(projdir, 'Breakpad', 'x86_64', sdk, outdir)


def BuildDumpSyms(outdir, sdk):
  projdir = os.path.join(outdir, 'src/tools/mac/dump_syms/dump_syms.xcodeproj')
  Xcodebuild(projdir, 'dump_syms', 'x86_64', sdk, outdir)


def BuildSymupload(outdir, sdk):
  projdir = os.path.join(outdir, 'src/tools/mac/symupload/symupload.xcodeproj')
  # This build fails with Xcode8/i386.
  Xcodebuild(projdir, 'symupload', 'x86_64', sdk, outdir)


def CreateOutDir(pbdir, outdir):
  workdir = os.path.join(outdir, 'src')
  if not os.path.isdir(workdir):
    os.makedirs(workdir)
  ProcessCall(['rsync', '-avH', os.path.join(pbdir, 'src/'), workdir])


def main():
  opts = ParseOption()
  pbdir = os.path.abspath(opts.pbdir)
  outdir = os.path.abspath(opts.outdir)

  CreateOutDir(pbdir, outdir)
  BuildBreakpad(outdir, opts.sdk)
  BuildDumpSyms(outdir, opts.sdk)
  BuildSymupload(outdir, opts.sdk)


if __name__ == '__main__':
  main()
