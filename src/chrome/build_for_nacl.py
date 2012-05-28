# -*- coding: utf-8 -*-
# Copyright 2010-2012, Google Inc.
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

# Copyright 2011 Google Inc. All Rights Reserved.

"""Builds binaries using NaCl compiler toochain.

This script invokes build_mozc.py with environment variables (PATH, CC, LD,
etc.) set for NaCl SDK.  This is a sort of trampoline code.
"""

__author__ = "sekia"

import json
import optparse
import os
import subprocess
import sys


def SetNaclSdkPath(nacl_sdk_root, toolchain_dir, env):
  """Adds NaCl SDK toolchain to PATH envorinment variable."""
  if nacl_sdk_root and toolchain_dir:
    nacl_bin = os.path.abspath(os.path.join(nacl_sdk_root,
                                            'toolchain', toolchain_dir, 'bin'))
    env['PATH'] = nacl_bin + os.pathsep + env.get('PATH', '')


def SetNaclToolchainEnv(toolchain_prefix, env):
  """Set NaCl toolchain environment variables."""
  commands = {
      'AR': 'ar',
      'AS': 'as',
      'CC': 'gcc',
      'CXX': 'g++',
      'LD': 'ld',
      'NM': 'nm',
      'RANLIB': 'ranlib',
      'STRIP': 'strip',
      }
  if toolchain_prefix:
    for env_var in commands:
      env[env_var] = toolchain_prefix + commands[env_var]


def InvokeBuildMozcScript(build_base, configuration, depth, args, env):
  """Invokes build_mozc.py to build targets with specified configuration."""
  command_line = [sys.executable, './build_mozc.py',
                  'build']
  if build_base:
    command_line.append('--build_base=%s' % build_base)
  if configuration:
    command_line.append('--configuration=%s' % configuration)
  if args:
    command_line.extend(args)

  print 'Running: ' + ' '.join(command_line)
  subprocess.check_call(command_line, cwd=depth, env=env)


def main():
  parser = optparse.OptionParser()
  parser.add_option('--build_base', dest='build_base')
  parser.add_option('--depth', dest='depth')
  parser.add_option('--nacl_sdk_root', dest='nacl_sdk_root',
                    help='Path to Native Client SDK directory')
  parser.add_option('--target_settings', dest='target_settings',
                    help='JSON object to specify cross build settings')
  parser.add_option('--touch_when_done', dest='touch_when_done',
                    help='Touches the given file when finished.')
  (options, args) = parser.parse_args()
  target_settings = json.loads(options.target_settings)

  # Check the structure of target_settings before a long way to build.
  if type(target_settings) is not list:
    raise TypeError('target_settings must be a list.')
  for entry in target_settings:
    if type(entry) is not dict:
      raise TypeError('entries of target_settings must be a dict.')
    for key in ('configuration', 'toolchain_dir', 'toolchain_prefix'):
      if key not in entry:
        raise KeyError('\`%s\' was not found in \`%s\'' % (key, entry))
      if type(entry[key]) not in (str, unicode):
        raise TypeError('value of \`%s\' is not in type string' % key)
      # The type for strings is unicode by default.  Converts them to str.
      utf8_value = entry[key].encode('utf-8')
      del entry[key]
      entry[key.encode('utf-8')] = utf8_value

  for entry in target_settings:
    env = os.environ.copy()
    SetNaclSdkPath(options.nacl_sdk_root, entry['toolchain_dir'], env)
    SetNaclToolchainEnv(entry['toolchain_prefix'], env)
    InvokeBuildMozcScript(options.build_base, entry['configuration'],
                          options.depth, args, env)

  # Touch the given file.
  if options.touch_when_done:
    with file(options.touch_when_done, 'a'):
      os.utime(options.touch_when_done, None)


if __name__ == '__main__':
  main()
