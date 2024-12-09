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

"""Runs wix.exe to build the installer."""

import argparse
import os
import pathlib
import subprocess

from build_tools import mozc_version
from build_tools import vs_util


def exec_command(args: list[str], cwd: str) -> None:
  """Runs the given command then returns the output.

  Args:
    args: The command to be executed.
    cwd: The current working directory.

  Raises:
    ChildProcessError: When the given command cannot be executed.
  """
  process = subprocess.Popen(
      args,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      shell=False,
      text=True,
      encoding='utf-8',
      cwd=cwd)
  stdout, stderr = process.communicate()
  exitcode = process.wait()

  if exitcode != 0:
    command = ' '.join(args)
    msgs = ['Failed to execute command:', '', command, '', f'cwd={cwd}']
    if stdout:
      msgs += ['-----stdout-----', stdout]
    if stderr:
      msgs += ['-----stderr-----', stderr]
    raise ChildProcessError('\n'.join(msgs))


def run_wix4(args) -> None:
  """Run 'dotnet tool run wix build ...'.

  Args:
    args: args
  """
  vs_env_vars = vs_util.get_vs_env_vars('x64')
  redist_root = pathlib.Path(vs_env_vars['VCTOOLSREDISTDIR']).resolve()
  redist_x86 = redist_root.joinpath('x86').joinpath('Microsoft.VC143.CRT')
  redist_x64 = redist_root.joinpath('x64').joinpath('Microsoft.VC143.CRT')
  version_file = pathlib.Path(args.version_file).resolve()
  version = mozc_version.MozcVersion(version_file)
  credit_file = pathlib.Path(args.credit_file).resolve()
  document_dir = credit_file.parent
  qt_dir = pathlib.Path(args.qt_core_dll).parent.parent.resolve()
  icon_path = pathlib.Path(args.icon_path).resolve()
  mozc_tip32 = pathlib.Path(args.mozc_tip32).resolve()
  mozc_tip64 = pathlib.Path(args.mozc_tip64).resolve()
  mozc_tip64_pdb = mozc_tip64.with_suffix('.pdb')
  if args.mozc_tip64_pdb:
    mozc_tip64_pdb = pathlib.Path(args.mozc_tip64_pdb).resolve()
  mozc_broker = pathlib.Path(args.mozc_broker).resolve()
  mozc_server = pathlib.Path(args.mozc_server).resolve()
  mozc_cache_service = pathlib.Path(args.mozc_cache_service).resolve()
  mozc_renderer = pathlib.Path(args.mozc_renderer).resolve()
  mozc_tool = pathlib.Path(args.mozc_tool).resolve()
  custom_action = pathlib.Path(args.custom_action).resolve()
  wix_path = pathlib.Path(args.wix_path).resolve()

  branding = args.branding
  upgrade_code = ''
  omaha_guid = ''
  omaha_client_key = ''
  omaha_clientstate_key = ''
  if branding == 'GoogleJapaneseInput':
    upgrade_code = 'C1A818AF-6EC9-49EF-ADCF-35A40475D156'
    omaha_guid = 'DDCCD2A9-025E-4142-BCEB-F467B88CF830'
    omaha_client_key = 'Software\\Google\\Update\\Clients\\' + omaha_guid
    omaha_clientstate_key = (
        'Software\\Google\\Update\\ClientState\\' + omaha_guid
    )
  elif branding == 'Mozc':
    upgrade_code = 'DD94B570-B5E2-4100-9D42-61930C611D8A'

  omaha_channel_type = 'dev' if version.IsDevChannel() else 'stable'
  vs_configuration_name = 'Debug' if args.debug_build else 'Release'

  commands = [
      f'{wix_path}',
      'build',
      '-nologo',
      '-arch', 'x64',
      '-define', f'MozcVersion={version.GetVersionString()}',
      '-define', f'UpgradeCode={upgrade_code}',
      '-define', f'OmahaGuid={omaha_guid}',
      '-define', f'OmahaClientKey={omaha_client_key}',
      '-define', f'OmahaClientStateKey={omaha_clientstate_key}',
      '-define', f'OmahaChannelType={omaha_channel_type}',
      '-define', f'VSConfigurationName={vs_configuration_name}',
      '-define', f'ReleaseRedistCrt32Dir={redist_x86}',
      '-define', f'ReleaseRedistCrt64Dir={redist_x64}',
      '-define', f'AddRemoveProgramIconPath={icon_path}',
      '-define', f'MozcTIP32Path={mozc_tip32}',
      '-define', f'MozcTIP64Path={mozc_tip64}',
      '-define', f'MozcTIP64PdbPath={mozc_tip64_pdb}',
      '-define', f'MozcBroker64Path={mozc_broker}',
      '-define', f'MozcServer64Path={mozc_server}',
      '-define', f'MozcCacheService64Path={mozc_cache_service}',
      '-define', f'MozcRenderer64Path={mozc_renderer}',
      '-define', f'MozcToolPath={mozc_tool}',
      '-define', f'CustomActions64Path={custom_action}',
      '-define', f'DocumentsDir={document_dir}',
      '-define', f'QtDir={qt_dir}',
      '-define', 'QtVer=6',
      '-out', args.output,
      '-src', args.wxs_path,
  ]
  exec_command(commands, cwd=os.getcwd())


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output', type=str)
  parser.add_argument('--version_file', type=str)
  parser.add_argument('--mozc_tool', type=str)
  parser.add_argument('--mozc_renderer', type=str)
  parser.add_argument('--mozc_server', type=str)
  parser.add_argument('--mozc_broker', type=str)
  parser.add_argument('--mozc_cache_service', type=str)
  parser.add_argument('--mozc_tip32', type=str)
  parser.add_argument('--mozc_tip64', type=str)
  parser.add_argument('--mozc_tip64_pdb', type=str)
  parser.add_argument('--custom_action', type=str)
  parser.add_argument('--icon_path', type=str)
  parser.add_argument('--credit_file', type=str)
  parser.add_argument('--qt_core_dll', type=str)
  parser.add_argument('--wxs_path', type=str)
  parser.add_argument('--wix_path', type=str)
  parser.add_argument('--branding', type=str)
  parser.add_argument(
      '--debug_build', dest='debug_build', default=False, action='store_true'
  )

  args = parser.parse_args()

  run_wix4(args)


if __name__ == '__main__':
  main()
