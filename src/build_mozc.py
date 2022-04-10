#!/usr/bin/python
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

"""Script building Mozc.

Typical usage:

  % python build_mozc.py gyp
  % python build_mozc.py build base/base.gyp:base
"""

from __future__ import print_function

import glob
import logging
import optparse
import os
import re
import sys

from build_tools import mozc_version
from build_tools.mozc_version import GenerateVersionFile
from build_tools.test_tools import test_launcher
from build_tools.util import ColoredLoggingFilter
from build_tools.util import ColoredText
from build_tools.util import CopyFile
from build_tools.util import GetNumberOfProcessors
from build_tools.util import IsLinux
from build_tools.util import IsMac
from build_tools.util import IsWindows
from build_tools.util import PrintErrorAndExit
from build_tools.util import RemoveDirectoryRecursively
from build_tools.util import RemoveFile
from build_tools.util import RunOrDie
from build_tools.util import RunOrDieError

SRC_DIR = '.'
# We need to obtain the absolute path of this script before change directory.
# Note that if any import above has already changed the current
# directory, this code cannot work anymore.
ABS_SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
MOZC_ROOT = ABS_SCRIPT_DIR
EXT_THIRD_PARTY_DIR = os.path.join(MOZC_ROOT, 'third_party')

sys.path.append(SRC_DIR)


def GetMozcVersion():
  """Returns MozcVersion instance."""
  # TODO(matsuzakit): Caching might be better.
  return mozc_version.MozcVersion('%s/mozc_version.txt' % SRC_DIR)


def GetBuildShortBaseName(target_platform):
  """Returns the build base directory.

  Args:
    target_platform: Target platform.
  Returns:
    Build base directory.
  Raises:
    RuntimeError: if target_platform is not supported.
  """

  platform_dict = {
      'Windows': 'out_win',
      'Mac': 'out_mac',
      'Linux': 'out_linux',
      'iOS': 'out_ios',
  }

  if target_platform not in platform_dict:
    raise RuntimeError(
        'Unknown target_platform: ' + (target_platform or 'None'))

  return platform_dict[target_platform]


def GetBuildBaseName(target_platform):
  return os.path.join(MOZC_ROOT, GetBuildShortBaseName(target_platform))


# TODO(team): Move this to build_tools/util.py
def PkgExists(*packages):
  """Return if the specified package exists or not.

  Args:
    *packages: packages to be examined.

  Returns:
    True if the specified package exists.
  """
  try:
    command_line = ['pkg-config', '--exists']
    command_line.extend(packages)
    RunOrDie(command_line)
    return True
  except RunOrDieError:
    logging.info('%s failed', ' '.join(command_line))
  return False


def GetGypFileNames(options):
  """Gets the list of gyp file names."""
  gyp_file_names = []
  exclude_top_dirs = []
  mozc_top_level_names = glob.glob('%s/*' % SRC_DIR)

  mozc_top_level_names = [x for x in mozc_top_level_names if
                          os.path.basename(x) not in exclude_top_dirs]

  for name in mozc_top_level_names:
    gyp_file_names.extend(glob.glob(name + '/*.gyp'))
  # Include subdirectories of data/test/session/scenario
  gyp_file_names.extend(glob.glob('%s/data/test/session/scenario/*.gyp' %
                                  SRC_DIR))
  gyp_file_names.extend(glob.glob('%s/data/test/session/scenario/*/*.gyp' %
                                  SRC_DIR))
  # Include subdirectories of data_manager
  gyp_file_names.extend(glob.glob('%s/data_manager/*/*.gyp' % SRC_DIR))
  # Include subdirectory of dictionary
  gyp_file_names.extend(glob.glob('%s/dictionary/*/*.gyp' % SRC_DIR))
  # Include subdirectory of rewriter
  gyp_file_names.extend(glob.glob('%s/rewriter/*/*.gyp' % SRC_DIR))
  # Include subdirectory of win32 and breakpad for Windows
  if options.target_platform == 'Windows':
    gyp_file_names.extend(glob.glob('%s/win32/*/*.gyp' % SRC_DIR))
  elif options.target_platform == 'Linux':
    gyp_file_names.extend(glob.glob('%s/unix/*/*.gyp' % SRC_DIR))
    # Add ibus.gyp if ibus version is >=1.4.1.
    if not PkgExists('ibus-1.0 >= 1.4.1'):
      logging.info('removing ibus.gyp.')
      gyp_file_names.remove('%s/unix/ibus/ibus.gyp' % SRC_DIR)
  gyp_file_names.sort()
  return gyp_file_names


def AddCommonOptions(parser):
  """Adds the options common among the commands."""
  parser.add_option('--verbose', '-v', dest='verbose',
                    action='callback', callback=ParseVerbose,
                    help='show verbose message.')
  return parser


def ParseVerbose(unused_option, unused_opt_str, unused_value, unused_parser):
  # Set log level to DEBUG if required.
  logging.getLogger().setLevel(logging.DEBUG)


# TODO(b/68382821): Remove this method. We no longer need --target_platform.
def AddTargetPlatformOption(parser):
  if IsLinux():
    default_target = 'Linux'
  elif IsWindows():
    default_target = 'Windows'
  elif IsMac():
    default_target = 'Mac'
  parser.add_option('--target_platform', dest='target_platform',
                    default=default_target,
                    help=('This option enables this script to know which build'
                          'should be done.'))


def GetDefaultWixPath():
  """Returns the default Wix directory.."""
  abs_path = ''
  return abs_path


def ParseGypOptions(args):
  """Parses command line options for the gyp command."""
  parser = optparse.OptionParser(usage='Usage: %prog gyp [options]')
  AddCommonOptions(parser)
  default_branding = 'Mozc'
  parser.add_option('--branding', dest='branding', default=default_branding,
                    help='Specifies the branding. [default: %default]')
  parser.add_option('--gypdir', dest='gypdir',
                    help='Specifies the location of GYP to be used.')
  parser.add_option('--gyp_python_path', dest='gyp_python_path',
                    help='File path to Python to execute GYP.')
  parser.add_option('--noqt', action='store_true', dest='noqt', default=False)
  parser.add_option('--version_file', dest='version_file',
                    help='use the specified version template file',
                    default='data/version/mozc_version_template.bzl')
  AddTargetPlatformOption(parser)

  # Mac and Linux
  parser.add_option('--warn_as_error', action='store_true',
                    dest='warn_as_error', default=(default_branding != 'Mozc'),
                    help='Treat compiler warning as error. This option is used '
                    'on Mac and Linux.')

  # Linux
  parser.add_option('--server_dir', dest='server_dir',
                    default='',
                    help='A path to the directory where the server executable'
                    'is installed. This option is used only on Linux.')

  if IsWindows():
    parser.add_option('--msvs_version', dest='msvs_version',
                      default='2017',
                      help='Version of the Visual Studio.')
    parser.add_option('--wix_dir', dest='wix_dir',
                      default=GetDefaultWixPath(),
                      help='A path to the binary directory of wix.')

  if IsWindows() or IsMac():
    parser.add_option('--qtdir', dest='qtdir',
                      default=os.getenv('QTDIR', None),
                      help='Qt base directory to be used.')

  return parser.parse_args(args)


def ExpandMetaTarget(options, meta_target_name):
  """Returns a list of build targets with expanding meta target name.

  If the specified name is 'package', returns a list of build targets for
  building production package.

  If the specified name is not a meta target name, returns it as a list.

  Args:
    options: option object.
    meta_target_name: meta target name to be expanded.

  Returns:
    A list of build targets with meta target names expanded.
  """
  version = GetMozcVersion()
  target_platform = version.GetTargetPlatform()
  config = options.configuration
  dependencies = []

  if meta_target_name != 'package':
    return dependencies + [meta_target_name]

  if target_platform == 'Linux':
    targets = [SRC_DIR + '/server/server.gyp:mozc_server',
               SRC_DIR + '/renderer/renderer.gyp:mozc_renderer',
               SRC_DIR + '/gui/gui.gyp:mozc_tool']
    if PkgExists('ibus-1.0 >= 1.4.1'):
      targets.append(SRC_DIR + '/unix/ibus/ibus.gyp:ibus_mozc')
  elif target_platform == 'Mac':
    targets = [SRC_DIR + '/mac/mac.gyp:codesign_DiskImage']
  elif target_platform == 'Windows':
    targets = [
        'out_win/%s:mozc_win32_build32' % config,
        'out_win/%sDynamic:mozc_win32_build32_dynamic' % config,
        'out_win/%s_x64:mozc_win32_build64' % config,
    ]

  return dependencies + targets


def ParseBuildOptions(args):
  """Parses command line options for the build command."""
  parser = optparse.OptionParser(usage='Usage: %prog build [options]')
  AddCommonOptions(parser)
  parser.add_option('--configuration', '-c', dest='configuration',
                    default='Debug', help='specify the build configuration.')

  (options, args) = parser.parse_args(args)

  targets = []
  for arg in args:
    targets.extend(ExpandMetaTarget(options, arg))
  return (options, targets)


def ParseRunTestsOptions(args):
  """Parses command line options for the runtests command."""
  parser = optparse.OptionParser(
      usage='Usage: %prog runtests [options] [test_targets] [-- build options]')
  AddCommonOptions(parser)
  default_test_jobs = GetNumberOfProcessors()
  parser.add_option('--test_jobs', dest='test_jobs', type='int',
                    default=default_test_jobs,
                    metavar='N', help='run test jobs in parallel')
  parser.add_option('--test_size', dest='test_size', default='small')
  parser.add_option('--configuration', '-c', dest='configuration',
                    default='Debug', help='specify the build configuration.')

  return parser.parse_args(args)


def ParseCleanOptions(args):
  """Parses command line options for the clean command."""
  parser = optparse.OptionParser(
      usage='Usage: %prog clean [-- build options]')
  AddCommonOptions(parser)
  AddTargetPlatformOption(parser)

  return parser.parse_args(args)


def AddPythonPathToEnvironmentFilesForWindows(out_dir):
  """Add PYTHONPATH to environment files for Ninja."""
  python_path_root = MOZC_ROOT
  python_path = os.path.abspath(python_path_root)
  original_python_paths = os.environ.get('PYTHONPATH', '')
  if original_python_paths:
    python_path = os.pathsep.join([original_python_paths, python_path])
  nul = chr(0)
  for d in os.listdir(out_dir):
    abs_dir = os.path.abspath(os.path.join(out_dir, d))
    with open(os.path.join(abs_dir, 'environment.x86'), 'rb') as x86_file:
      x86_content = (
          x86_file.read()[:-1] +
          ('PYTHONPATH=' + python_path + nul + nul).encode('utf-8'))
    with open(os.path.join(abs_dir, 'environment.x86'), 'wb') as x86_file:
      x86_file.write(x86_content)
      print('== x86_content ==')
      print(x86_content.decode('utf-8'))
    with open(os.path.join(abs_dir, 'environment.x64'), 'rb') as x64_file:
      x64_content = (
          x64_file.read()[:-1] +
          ('PYTHONPATH=' + python_path + nul + nul).encode('utf-8'))
    with open(os.path.join(abs_dir, 'environment.x64'), 'wb') as x64_file:
      x64_file.write(x64_content)
      print('== x64_content ==')
      print(x64_content.decode('utf-8'))


def GypMain(options, unused_args):
  """The main function for the 'gyp' command."""
  # Generate a version definition file.
  logging.info('Generating version definition file...')
  template_path = '%s/%s' % (SRC_DIR, options.version_file)
  version_path = '%s/mozc_version.txt' % SRC_DIR
  version_override = os.environ.get('MOZC_VERSION', None)
  GenerateVersionFile(template_path, version_path, options.target_platform,
                      version_override)
  version = GetMozcVersion()
  target_platform = version.GetTargetPlatform()
  logging.info('Version string is %s', version.GetVersionString())

  # Back up the original 'PYTHONPATH' environment variable in case we change
  # it in this function.
  original_python_path = os.environ.get('PYTHONPATH', '')

  # 'gypdir' option allows distributors/packages to specify the location of
  # the system-installed version of gyp to conform their packaging policy.
  if options.gypdir:
    # When distributors/packages choose to use system-installed gyp, it is
    # their responsibility to make sure if 'gyp' command works well on
    # their environment.
    gyp_dir = os.path.abspath(options.gypdir)
    gyp_command = [os.path.join(gyp_dir, 'gyp')]
  else:
    # Use third_party/gyp/gyp unless 'gypdir' option is specified. This is
    # the default and official way to build Mozc. In this case, you need to
    # consider the case where a Python package named 'gyp' has already been
    # installed in the target machine. So we update PYTHONPATH so that
    # third_party/gyp/gyp_main.py can find its own library modules (they are
    # installed under 'third_party/gyp/pylib') rather than system-installed
    # GYP modules.
    gyp_dir = os.path.abspath(os.path.join(MOZC_ROOT, 'third_party', 'gyp'))
    gyp_main_file = os.path.join(gyp_dir, 'gyp_main.py')
    # Make sure |gyp_main_file| exists.
    if not os.path.exists(gyp_main_file):
      message = (
          'GYP does not exist at %s. Please run "git submodule update --init" '
          'to check out GYP. If you want to use system-installed GYP, use '
          '--gypdir option to specify its location. e.g. '
          '"python build_mozc.py gyp --gypdir=/usr/bin"' % gyp_main_file)
      PrintErrorAndExit(message)
    gyp_python_path = options.gyp_python_path or sys.executable
    gyp_command = [gyp_python_path, gyp_main_file]
    os.environ['PYTHONPATH'] = os.pathsep.join(
        [os.path.join(gyp_dir, 'pylib'), original_python_path])

    # Following script tests if 'import gyp' is now available and its
    # location is expected one. By using this script, make sure
    # 'import gyp' actually loads 'third_party/gyp/pylib/gyp'.
    gyp_check_script = os.path.join(
        ABS_SCRIPT_DIR, 'build_tools', 'ensure_gyp_module_path.py')
    expected_gyp_module_path = os.path.join(gyp_dir, 'pylib', 'gyp')
    RunOrDie([gyp_python_path, gyp_check_script,
              '--expected=%s' % expected_gyp_module_path])

  # Set the generator name.
  os.environ['GYP_GENERATORS'] = 'ninja'
  # Disable path normalization that breaks arguments like --name=../../path
  os.environ['GYP_MSVS_DISABLE_PATH_NORMALIZATION'] = '1'

  # Get and show the list of .gyp file names.
  gyp_file_names = GetGypFileNames(options)
  logging.debug('GYP files:')
  for file_name in gyp_file_names:
    logging.debug('- %s', file_name)

  # Build GYP command line.
  logging.info('Building GYP command line...')
  gyp_options = ['--depth=.']
  if target_platform == 'Windows':
    gyp_options.extend(['--include=%s/gyp/common_win.gypi' % SRC_DIR])
  else:
    gyp_options.extend(['--include=%s/gyp/common.gypi' % SRC_DIR])

  gyp_options.extend(['-D', 'abs_depth=%s' % MOZC_ROOT])
  gyp_options.extend(['-D', 'ext_third_party_dir=%s' % EXT_THIRD_PARTY_DIR])

  if IsWindows():
    gyp_options.extend(['-D', 'python="%s"' % sys.executable])
  else:
    gyp_options.extend(['-D', 'python=%s' % sys.executable])

  gyp_options.extend(gyp_file_names)

  gyp_options.extend(['-D', 'version=' + version.GetVersionString()])
  gyp_options.extend(['-D', 'short_version=' + version.GetShortVersionString()])

  if options.branding:
    gyp_options.extend(['-D', 'branding=%s' % options.branding])

  # Qt configurations
  if options.noqt:
    gyp_options.extend(['-D', 'use_qt=NO'])
    gyp_options.extend(['-D', 'qt_dir='])
  elif target_platform == 'Linux':
    gyp_options.extend(['-D', 'use_qt=YES'])
    gyp_options.extend(['-D', 'qt_dir='])

    # Check if Qt libraries are installed.
    if not PkgExists('Qt5Core', 'Qt5Gui', 'Qt5Widgets'):
      PrintErrorAndExit('Qt is required to build GUI Tool. '
                        'Specify --noqt to skip building GUI Tool.')

  else:
    gyp_options.extend(['-D', 'use_qt=YES'])
    if options.qtdir:
      gyp_options.extend(['-D', 'qt_dir=%s' % os.path.abspath(options.qtdir)])
    else:
      gyp_options.extend(['-D', 'qt_dir='])

  if target_platform == 'Windows' and options.wix_dir:
    gyp_options.extend(['-D', 'use_wix=YES'])
    gyp_options.extend(['-D', 'wix_dir=%s' % options.wix_dir])
  else:
    gyp_options.extend(['-D', 'use_wix=NO'])

  gyp_options.extend(['-D', 'build_base=%s' %
                      GetBuildBaseName(target_platform)])
  gyp_options.extend(['-D', 'build_short_base=%s' %
                      GetBuildShortBaseName(target_platform)])

  if options.warn_as_error:
    gyp_options.extend(['-D', 'warn_as_error=1'])
  else:
    gyp_options.extend(['-D', 'warn_as_error=0'])

  if version.IsDevChannel():
    gyp_options.extend(['-D', 'channel_dev=1'])

  target_platform_value = target_platform
  gyp_options.extend(['-D', 'target_platform=%s' % target_platform_value])

  if IsWindows():
    gyp_options.extend(['-G', 'msvs_version=%s' % options.msvs_version])

  if (target_platform == 'Linux' and
      '%s/unix/ibus/ibus.gyp' % SRC_DIR in gyp_file_names):
    gyp_options.extend(['-D', 'use_libibus=1'])

  if options.server_dir:
    gyp_options.extend([
        '-D', 'server_dir=%s' % os.path.abspath(options.server_dir)])

  gyp_options.extend(['--generator-output=.'])
  short_basename = GetBuildShortBaseName(target_platform)
  gyp_options.extend(['-G', 'output_dir=%s' % short_basename])

  # Enable cross-compile
  # TODO(yukawa): Enable GYP_CROSSCOMPILE for Windows.
  if not IsWindows():
    os.environ['GYP_CROSSCOMPILE'] = '1'

  # Run GYP.
  logging.info('Running GYP...')
  RunOrDie(gyp_command + gyp_options)

  # Restore PYTHONPATH just in case.
  if not original_python_path:
    os.environ['PYTHONPATH'] = original_python_path

  # Done!
  logging.info('Done')

  # For internal Ninja build on Windows, set up environment files
  if IsWindows():
    out_dir = os.path.join(MOZC_ROOT, 'out_win')
    AddPythonPathToEnvironmentFilesForWindows(out_dir)

  if IsWindows() and (not options.noqt):
    # When Windows build is configured to use DLL version of Qt, copy Qt's DLLs
    # and debug symbols into Mozc's build directory. This is important because:
    # - We can easily back up all the artifacts if relevant product binaries and
    #   debug symbols are placed at the same place.
    # - Some post-build tools such as bind.exe can easily look up the dependent
    #   DLLs (QtCore4.dll and QtQui4.dll in this case).
    # Perhaps the following code can also be implemented in gyp, but we atopt
    # this ad hock workaround as a first step.
    # TODO(yukawa): Implement the following logic in gyp, if magically possible.
    abs_qtdir = os.path.abspath(options.qtdir)
    abs_qt_bin_dir = os.path.join(abs_qtdir, 'bin')
    abs_qt_lib_dir = os.path.join(abs_qtdir, 'lib')
    abs_out_win = GetBuildBaseName(target_platform)
    abs_out_win_debug_dynamic = os.path.join(abs_out_win, 'DebugDynamic')
    abs_out_win_release_dynamic = os.path.join(abs_out_win, 'ReleaseDynamic')
    copy_script = os.path.join(ABS_SCRIPT_DIR, 'build_tools',
                               'copy_dll_and_symbol.py')
    copy_params = [{
        'basenames': 'Qt5Cored;Qt5Guid;Qt5Widgetsd',
        'dll_paths': abs_qt_bin_dir,
        'pdb_paths': abs_qt_lib_dir,
        'target_dir': abs_out_win_debug_dynamic,
    }, {
        'basenames': 'Qt5Core;Qt5Gui;Qt5Widgets',
        'dll_paths': abs_qt_bin_dir,
        'pdb_paths': abs_qt_lib_dir,
        'target_dir': abs_out_win_release_dynamic,
    }, {
        'basenames': 'qwindowsd',
        'dll_paths': os.path.join(abs_qtdir, 'plugins', 'platforms'),
        'pdb_paths': os.path.join(abs_qtdir, 'plugins', 'platforms'),
        'target_dir': os.path.join(abs_out_win_debug_dynamic, 'platforms'),
    }, {
        'basenames': 'qwindows',
        'dll_paths': os.path.join(abs_qtdir, 'plugins', 'platforms'),
        'pdb_paths': os.path.join(abs_qtdir, 'plugins', 'platforms'),
        'target_dir': os.path.join(abs_out_win_release_dynamic, 'platforms'),
    }]
    for copy_param in copy_params:
      copy_commands = [
          copy_script,
          '--basenames', copy_param['basenames'],
          '--dll_paths', copy_param['dll_paths'],
          '--pdb_paths', copy_param['pdb_paths'],
          '--target_dir', copy_param['target_dir'],
      ]
      RunOrDie(copy_commands)


def GetNinjaPath():
  """Returns the path to Ninja."""
  ninja = 'ninja'
  if IsWindows():
    ninja = 'ninja.exe'
  return ninja


def BuildWithNinja(options, targets):
  """Build the targets with Ninja."""
  short_basename = GetBuildShortBaseName(GetMozcVersion().GetTargetPlatform())
  build_arg = '%s/%s' % (short_basename, options.configuration)

  ninja = GetNinjaPath()

  for target in targets:
    (_, target_name) = target.split(':')
    RunOrDie([ninja, '-C', build_arg, target_name])


def BuildOnWindows(targets):
  """Build the target on Windows."""
  ninja = GetNinjaPath()

  for target in targets:
    (build_arg, target_name) = target.split(':')
    RunOrDie([ninja, '-C', build_arg, target_name])


def BuildMain(options, targets):
  """The main function for the 'build' command."""
  if not targets:
    PrintErrorAndExit('No build target is specified.')

  # Add the mozc root directory to PYTHONPATH.
  original_python_path = os.environ.get('PYTHONPATH', '')
  depot_path = MOZC_ROOT
  python_path = os.pathsep.join([original_python_path, depot_path])
  os.environ['PYTHONPATH'] = python_path

  if IsWindows():
    BuildOnWindows(targets)
  else:
    BuildWithNinja(options, targets)

  # Revert python path.
  os.environ['PYTHONPATH'] = original_python_path


def RunTest(binary_path, output_dir, options):
  """Run test with options.

  Args:
    binary_path: The path of unittest.
    output_dir: The directory of output results.
    options: options to be passed to the unittest.
  """
  binary_filename = os.path.basename(binary_path)
  tmp_xml_path = os.path.join(output_dir, '%s.xml.running' % binary_filename)
  test_options = options[:]
  test_options.extend(['--gunit_output=xml:%s' % tmp_xml_path,
                       '--gtest_output=xml:%s' % tmp_xml_path])
  RunOrDie([binary_path] + test_options)

  xml_path = os.path.join(output_dir, '%s.xml' % binary_filename)
  CopyFile(tmp_xml_path, xml_path)
  RemoveFile(tmp_xml_path)


def RunTestOnIos(binary_path, output_dir, _):
  """Run test with options.

  Args:
    binary_path: The path of unittest.
    output_dir: The directory of output results.
    _: Unused arg for the compatibility with RunTest.
  """
  iossim = '%s/third_party/iossim/iossim' % MOZC_ROOT
  binary_filename = os.path.basename(binary_path)
  tmp_xml_path = os.path.join(output_dir, '%s.xml.running' % binary_filename)
  env_options = [
      '-e', 'GUNIT_OUTPUT=xml:%s' % tmp_xml_path,
      '-e', 'GTEST_OUTPUT=xml:%s' % tmp_xml_path,
  ]
  RunOrDie([iossim] + env_options + [binary_path])

  xml_path = os.path.join(output_dir, '%s.xml' % binary_filename)
  CopyFile(tmp_xml_path, xml_path)
  RemoveFile(tmp_xml_path)


def RunTests(target_platform, configuration, parallel_num):
  """Run built tests actually.

  Args:
    target_platform: The build target ('Linux', 'Windows', etc.)
    configuration: build configuration ('Release' or 'Debug')
    parallel_num: allows specified jobs at once.

  Raises:
    RunOrDieError: One or more tests have failed.
  """
  # TODO(nona): move this function to build_tools/test_tools
  base_path = os.path.join(GetBuildBaseName(target_platform), configuration)

  options = []

  # Specify the log_dir directory.
  # base_path looks like out_mac/Debug.
  options.append('--log_dir=%s' % base_path)

  failed_tests = []
  # This is a silly algorithm: it runs *all* tests built in the target
  # directory.  Therefore, if you build multiple tests without
  # cleaning, the second runtests runs every test.
  # TODO(mukai): parses gyp files and get the target binaries, if possible.
  executable_suffix = ''
  test_function = RunTest
  if target_platform == 'Windows':
    executable_suffix = '.exe'
  elif target_platform == 'iOS':
    executable_suffix = '.app'
    test_function = RunTestOnIos
    parallel_num = 1

  test_binaries = glob.glob(
      os.path.join(base_path, '*_test' + executable_suffix))

  # Prepare gtest_report directory.
  gtest_report_dir = os.path.abspath(os.path.join(base_path, 'gtest_report'))
  if os.path.exists(gtest_report_dir):
    # Clear existing gtest reports.
    RemoveDirectoryRecursively(gtest_report_dir)
  os.makedirs(gtest_report_dir)

  failed_tests = []

  # Create default test reports in case any test process crashes and cannot
  # leave test result as a XML report.
  # TODO(yukawa): Move this template to test_tools/gtest_report.py.
  xml_template = (
      '<?xml version="1.0" encoding="UTF-8"?>\n'
      '  <testsuite name="%s" tests="1" errors="1">\n'
      '    <testcase name="No reporting XML">\n'
      '      <error message="No reporting XML has been generated. '
      'Process crash?" />\n'
      '    </testcase>\n'
      '</testsuite>\n')
  for binary in test_binaries:
    binary_filename = os.path.basename(binary)
    xml_path = os.path.join(gtest_report_dir, '%s.xml' % binary_filename)
    with open(xml_path, 'w') as f:
      f.write(xml_template % binary_filename)

  if parallel_num == 1:
    for binary in test_binaries:
      logging.info('running %s...', binary)
      try:
        test_function(binary, gtest_report_dir, options)
      except RunOrDieError as e:
        logging.error(e)
        failed_tests.append(binary)
  else:
    launcher = test_launcher.TestLauncher(gtest_report_dir)
    for binary in test_binaries:
      launcher.AddTestCommand([binary] + options)
    failed_tests = launcher.Execute(parallel_num)

  if failed_tests:
    error_text = ColoredText('following tests failed', logging.ERROR)
    raise RunOrDieError('\n'.join([error_text] + failed_tests))


def RunTestsMain(options, args):
  """The main function for 'runtests' command."""
  # extracting test targets and build flags.  To avoid parsing build
  # options from ParseRunTestsOptions(), user has to specify the test
  # targets at first, and then add '--' and build flags.  Although
  # optparse removes the '--' argument itself, the first element of
  # build options has to start with '-' character because they are
  # flags.
  # ex: runtests foo.gyp:foo_test bar.gyp:bar_test -- -c Release
  #   => ['foo.gyp:foo_test', 'bar.gyp:bar_test', '-c', 'Release']
  # here 'foo.gyp:foo_test' and 'bar.gyp:bar_test' are test targets
  # and '-c' and 'Release' are build options.
  targets = []
  build_options = []
  for i in range(len(args)):
    if args[i].startswith('-'):
      # starting with build options
      build_options = args[i:]
      break
    target = args[i]
    # If a directory name is specified as a target, it builds
    # *_all_test target instead.
    if args[i].endswith('/'):
      matched = re.search(r'([^/]+)/$', args[i])
      if matched:
        dirname = matched.group(1)
        target = '%s%s.gyp:%s_all_test' % (args[i], dirname, dirname)
    targets.append(target)

  # configuration flags are shared among runtests options and
  # build options.
  if 'jobs' in vars(options).keys():
    build_options.extend(['-j', options.jobs])
  if options.configuration:
    build_options.extend(['-c', options.configuration])

  target_platform = GetMozcVersion().GetTargetPlatform()

  if not targets:
    # TODO(yukawa): Change the notation rule of 'targets' to reduce the gap
    # between Ninja and make.
    if target_platform == 'Windows':
      targets.append('out_win/%s:unittests' % options.configuration)
    else:
      targets.append('%s/gyp/tests.gyp:unittests' % SRC_DIR)

  # Build the test targets
  (build_opts, build_args) = ParseBuildOptions(build_options + targets)
  BuildMain(build_opts, build_args)

  # Run tests actually
  RunTests(target_platform, options.configuration, options.test_jobs)


def CleanMain(options, unused_args):
  """The main function for the 'clean' command."""

  # File and directory names to be removed.
  file_names = []
  directory_names = []

  # Collect stuff in the gyp directories.
  gyp_directory_names = [os.path.dirname(f) for f in GetGypFileNames(options)]
  for gyp_directory_name in gyp_directory_names:
    if IsWindows():
      for pattern in ['*.ncb', '*.rules', '*.props', '*.sdf', '*.sln', '*.suo',
                      '*.targets', '*.vcproj', '*.vcproj.*.user', '*.vcxproj',
                      '*.vcxproj.filters', '*.vcxproj.user', 'gen_*_files.xml']:
        file_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                                 pattern)))
      for build_type in ['Debug', 'Release']:
        directory_names.append(os.path.join(gyp_directory_name,
                                            build_type))
    elif IsMac():
      directory_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                                    '*.xcodeproj')))

  # mozc_version.txt does not always exist.
  version_file = '%s/mozc_version.txt' % SRC_DIR
  if os.path.exists(version_file):
    file_names.append(version_file)
    build_base = GetBuildBaseName(GetMozcVersion().GetTargetPlatform())
    if build_base:
      directory_names.append(build_base)

  # Remove files.
  for file_name in file_names:
    RemoveFile(file_name)
  # Remove directories.
  for directory_name in directory_names:
    RemoveDirectoryRecursively(directory_name)


def ShowHelpAndExit():
  """Shows the help message."""
  print('Usage: build_mozc.py COMMAND [ARGS]')
  print('Commands: ')
  print('  gyp          Generate project files.')
  print('  build        Build the specified target.')
  print('  runtests     Build all tests and run them.')
  print('  clean        Clean all the build files and directories.')
  print('')
  print('See also the comment in the script for typical usage.')


def main():
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
  logging.getLogger().addFilter(ColoredLoggingFilter())

  if len(sys.argv) < 2:
    ShowHelpAndExit()
    return 0

  # Move to the Mozc root source directory only once since os.chdir
  # affects functions in os.path and that causes troublesome errors.
  os.chdir(MOZC_ROOT)

  command = sys.argv[1]
  args = sys.argv[2:]

  if IsWindows() and (not os.environ.get('VCToolsRedistDir', '')):
    print('VCToolsRedistDir is not defined.')
    print('Please use Developer Command Prompt or run vcvarsamd64_x86.bat')
    return 1

  if command == 'gyp':
    (cmd_opts, cmd_args) = ParseGypOptions(args)
    GypMain(cmd_opts, cmd_args)
  elif command == 'build':
    (cmd_opts, cmd_args) = ParseBuildOptions(args)
    BuildMain(cmd_opts, cmd_args)
  elif command == 'runtests':
    (cmd_opts, cmd_args) = ParseRunTestsOptions(args)
    RunTestsMain(cmd_opts, cmd_args)
  elif command == 'clean':
    (cmd_opts, cmd_args) = ParseCleanOptions(args)
    CleanMain(cmd_opts, cmd_args)
  else:
    logging.error('Unknown command: %s', command)
    ShowHelpAndExit()
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
