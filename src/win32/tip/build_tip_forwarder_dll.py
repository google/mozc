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

"""This script builds am Arm64X pure forwarder DLL for Mozc TIPs on Windows.

See the following document for more details:
https://learn.microsoft.com/en-us/windows/arm/arm64x-build#building-an-arm64x-pure-forwarder-dll
"""


import argparse
import dataclasses
import os
import pathlib
import shutil
import subprocess
import tempfile
from typing import Union

from build_tools import mozc_version
from build_tools import vs_util


def _get_def_file_content(filename: str) -> str:
  """Generates an DEF file content for a pure forwarder of in-proc COM DLL.

  Args:
    filename: The name of the DLL to be forwarded.
  Returns:
    A string containing the content of the DEF file.
  """
  stem = str(pathlib.Path(filename).stem)
  return '\n'.join(
      [
          'EXPORTS',
          f'    DllGetClassObject = {stem}.DllGetClassObject',
          f'    DllCanUnloadNow   = {stem}.DllCanUnloadNow',
      ]
  )


@dataclasses.dataclass
class ForwarderInfo:
  """Information about the forwarder DLL to be generated."""

  branding: str
  version: mozc_version.MozcVersion

  def get_version_string(self, separator='.') -> str:
    components = ['@MAJOR@', '@MINOR@', '@BUILD@', '@REVISION@']
    return self.version.GetVersionInFormat(separator.join(components))

  @property
  def forwarder_dll_name(self) -> str:
    return {
        'Mozc': 'mozc_tip64x.dll',
        'GoogleJapaneseInput': 'GoogleIMEJaTIP64X.dll',
    }[self.branding]

  @property
  def x64_impl_dll_name(self) -> str:
    return {
        'Mozc': 'mozc_tip64.dll',
        'GoogleJapaneseInput': 'GoogleIMEJaTIP64.dll',
    }[self.branding]

  @property
  def arm64_impl_dll_name(self) -> str:
    return {
        'Mozc': 'mozc_tip64arm.dll',
        'GoogleJapaneseInput': 'GoogleIMEJaTIP64Arm.dll',
    }[self.branding]

  @property
  def file_description(self) -> str:
    return {
        'Mozc': 'Mozc TIP Module Forwarder',
        'GoogleJapaneseInput': 'Google 日本語入力 TIP モジュール フォワーダー',
    }[self.branding]

  @property
  def product_name(self) -> str:
    return {
        'Mozc': 'Mozc',
        'GoogleJapaneseInput': 'Google 日本語入力',
    }[self.branding]

  def get_def_file_content_for_x64(self) -> str:
    return _get_def_file_content(self.x64_impl_dll_name)

  def get_def_file_content_for_arm64(self) -> str:
    return _get_def_file_content(self.arm64_impl_dll_name)

  def get_rc_file_content(self) -> str:
    comma_separated_versoin = self.get_version_string(',')
    dot_separated_versoin = self.get_version_string('.')
    file_description = self.file_description

    original_file_name = self.forwarder_dll_name
    internal_name = str(pathlib.Path(original_file_name).stem)
    product_name = self.product_name
    product_copyright = 'Google LLC'

    return '\n'.join(
        [
            '#include "winres.h"',
            'LANGUAGE LANG_JAPANESE, SUBLANG_DEFAULT',
            '',
            'VS_VERSION_INFO VERSIONINFO',
            f'FILEVERSION {comma_separated_versoin}',
            f'PRODUCTVERSION {comma_separated_versoin}',
            (
                'FILEFLAGSMASK'
                ' VS_FF_DEBUG | VS_FF_PRERELEASE | VS_FF_PATCHED'
                ' | VS_FF_INFOINFERRED'
            ),
            'FILEFLAGS 0x0L',
            'FILEOS VOS__WINDOWS32',
            'FILETYPE VFT_DLL',
            'FILESUBTYPE VFT2_UNKNOWN',
            'BEGIN',
            '    BLOCK "StringFileInfo"',
            '    BEGIN',
            '        BLOCK "041104b0"',
            '        BEGIN',
            f'            VALUE "CompanyName", "{product_copyright}"',
            f'            VALUE "FileDescription", "{file_description}"',
            f'            VALUE "FileVersion", "{dot_separated_versoin}"',
            f'            VALUE "InternalName", "{internal_name}"',
            f'            VALUE "LegalCopyright", "{product_copyright}"',
            f'            VALUE "OriginalFilename", "{original_file_name}"',
            f'            VALUE "ProductName", "{product_name}"',
            f'            VALUE "ProductVersion", "{dot_separated_versoin}"',
            '        END',
            '    END',
            '    BLOCK "VarFileInfo"',
            '    BEGIN',
            '        VALUE "Translation", 0x411, 1200',
            '    END',
            'END',
            '',
        ]
    )


def exec_command(
    command: list[str],
    cwd: Union[str, pathlib.Path],
    env: dict[str, str],
    dryrun: bool = False,
) -> None:
  """Run the specified command.

  Args:
    command: Command to run.
    cwd: Directory to execute the command.
    env: Environment variables.
    dryrun: True to execute the specified command as a dry run.

  Raises:
    CalledProcessError: When the process failed.
  """
  if dryrun:
    print(
        f"dryrun: subprocess.run('{command}', shell=False, check=True,"
        f' cwd={cwd}, env={env})'
    )
  else:
    subprocess.run(command, shell=False, check=True, cwd=cwd, env=env)


def build_on_windows(args: argparse.Namespace) -> None:
  """Build an ARM64X DLL.

  Args:
    args: build options to be used to customize behaviors.

  Raises:
    FileNotFoundError: when any required file is not found.
  """
  version = mozc_version.MozcVersion(args.version_file)
  info = ForwarderInfo(branding=args.branding, version=version)

  # Currently only x64 is supported as the host architecture.
  # TODO: https://github.com/google/mozc/issues/1296 - Support ARM64 host.
  env = vs_util.get_vs_env_vars('x64_arm64')

  dest = pathlib.Path(args.output)
  if dest.exists():
    if args.dryrun:
      print(f'dryrun: unlinking {str(dest)}')
    else:
      dest.unlink()

  with tempfile.TemporaryDirectory() as work_dir:
    empty_cc = pathlib.Path(work_dir).joinpath('empty.cc')
    empty_cc.touch()

    cl = shutil.which('cl.exe', path=env['PATH'])
    link = shutil.which('link.exe', path=env['PATH'])
    rc = shutil.which('rc.exe', path=env['PATH'])

    exec_command(
        [cl, '/nologo', '/c', '/Foempty_arm64.obj', 'empty.cc'],
        cwd=work_dir,
        env=env,
        dryrun=args.dryrun,
    )
    exec_command(
        [cl, '/nologo', '/c', '/arm64EC', '/Foempty_x64.obj', 'empty.cc'],
        cwd=work_dir,
        env=env,
        dryrun=args.dryrun,
    )

    mozc_x64_def = pathlib.Path(work_dir).joinpath('mozc_x64.def')
    mozc_x64_def.write_text(
        info.get_def_file_content_for_x64(), encoding='utf-8', newline='\r\n'
    )

    exec_command(
        [
            link,
            '/lib',
            '/nologo',
            '/machine:arm64EC',
            '/ignore:4104',
            '/def:mozc_x64.def',
            '/out:mozc_x64.lib',
        ],
        cwd=work_dir,
        env=env,
        dryrun=args.dryrun,
    )
    mozc_arm64_def = pathlib.Path(work_dir).joinpath('mozc_arm64.def')
    mozc_arm64_def.write_text(
        info.get_def_file_content_for_arm64(), encoding='utf-8', newline='\r\n'
    )
    exec_command(
        [
            link,
            '/lib',
            '/nologo',
            '/machine:arm64',
            '/ignore:4104',
            '/def:mozc_arm64.def',
            '/out:mozc_arm64.lib',
        ],
        cwd=work_dir,
        env=env,
        dryrun=args.dryrun,
    )

    rc_file = pathlib.Path(work_dir).joinpath('mozc_tip_shim.rc')
    rc_file.write_text(
        info.get_rc_file_content(), encoding='utf-8', newline='\r\n'
    )

    exec_command(
        [rc, '/nologo', '/r', '/8', str(rc_file)],
        cwd=work_dir,
        env=env,
        dryrun=args.dryrun,
    )

    forwarder_filename = pathlib.Path(info.forwarder_dll_name).name
    exec_command(
        [
            link,
            '/dll',
            '/nologo',
            '/noentry',
            '/machine:arm64x',
            '/emitpogophaseinfo',
            '/largeaddressaware',
            '/dynamicbase',
            '/highentropyva',
            '/ignore:4104',
            '/defArm64Native:mozc_arm64.def',
            '/def:mozc_x64.def',
            f'/out:{forwarder_filename}',
            'empty_arm64.obj',
            'empty_x64.obj',
            'mozc_arm64.lib',
            'mozc_x64.lib',
            'mozc_tip_shim.res',
        ],
        cwd=work_dir,
        env=env,
        dryrun=args.dryrun,
    )

    src = pathlib.Path(work_dir).joinpath(forwarder_filename)
    if args.dryrun:
      print(f'dryrun: Copying {src} to {dest}')
    else:
      shutil.copy2(src=src, dst=dest)


def parse_args() -> argparse.Namespace:
  """Parse command line options."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--dryrun', action='store_true', default=False)

  parser.add_argument(
      '--version_file',
      dest='version_file',
      help='the path to version.txt',
  )
  parser.add_argument(
      '--branding',
      dest='branding',
      default='Mozc',
      choices=['Mozc', 'GoogleJapaneseInput'],
      help='branding',
  )
  parser.add_argument(
      '--output',
      dest='output',
      help='the path of the generated forwarder DLL file',
  )

  return parser.parse_args()


def main():
  args = parse_args()

  if os.name == 'nt':
    build_on_windows(args)


if __name__ == '__main__':
  main()
