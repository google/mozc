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

"""A helper script to use Visual Studio."""
import json
import pathlib
import subprocess
import sys
from typing import Union


def get_vcvarsall(path_hint: Union[str, None] = None) -> pathlib.Path:
  """Returns the path of 'vcvarsall.bat'.

  Args:
    path_hint: optional path to vcvarsall.bat

  Returns:
    The path of 'vcvarsall.bat'.

  Raises:
    FileNotFoundError: When 'vcvarsall.bat' cannot be found.
  """
  if path_hint is not None:
    path = pathlib.Path(path_hint).resolve()
    if path.exists():
      return path

  for program_files in ['Program Files', 'Program Files (x86)']:
    for edition in ['Community', 'Professional', 'Enterprise', 'BuildTools']:
      vcvarsall = pathlib.Path('C:\\', program_files, 'Microsoft Visual Studio',
                               '2022', edition, 'VC', 'Auxiliary', 'Build',
                               'vcvarsall.bat')
      if vcvarsall.exists():
        return vcvarsall

  raise FileNotFoundError(
      'Could not find vcvarsall.bat. '
      'Consider using --vcvarsall_path option e.g.\n'
      r' --vcvarsall_path=C:\VS\VC\Auxiliary\Build\vcvarsall.bat'
  )


def get_vs_env_vars(
    arch: str,
    vcvarsall_path_hint: Union[str, None] = None,
) -> dict[str, str]:
  """Returns environment variables for the specified Visual Studio C++ tool.

  Oftentimes commandline build process for Windows requires to us to set up
  environment variables (especially 'PATH') first by executing an appropriate
  Visual C++ batch file ('vcvarsall.bat').  As a result, commands to be passed
  to methods like subprocess.run() can easily become complicated and difficult
  to maintain.

  With get_vs_env_vars() you can easily decouple environment variable handling
  from the actual command execution as follows.

    cwd = ...
    env = get_vs_env_vars('amd64_x86')
    subprocess.run(command_fullpath, shell=False, check=True, cwd=cwd, env=env)

  or

    cwd = ...
    env = get_vs_env_vars('amd64_x86')
    subprocess.run(command, shell=True, check=True, cwd=cwd, env=env)

  For the 'arch' argument, see the following link to find supported values.
  https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line#vcvarsall-syntax

  Args:
    arch: The architecture to be used, e.g. 'amd64_x86'
    vcvarsall_path_hint: optional path to vcvarsall.bat

  Returns:
    A dict of environment variable.

  Raises:
    ChildProcessError: When 'vcvarsall.bat' cannot be executed.
    FileNotFoundError: When 'vcvarsall.bat' cannot be found.
  """
  vcvarsall = get_vcvarsall(vcvarsall_path_hint)

  pycmd = (r'import json;'
           r'import os;'
           r'print(json.dumps(dict(os.environ), ensure_ascii=True))')
  cmd = f'("{vcvarsall}" {arch}>nul)&&("{sys.executable}" -c "{pycmd}")'
  process = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
  stdout, _ = process.communicate()
  exitcode = process.wait()
  if exitcode != 0:
    raise ChildProcessError(f'Failed to execute {vcvarsall}')
  return json.loads(stdout.decode('ascii'))

