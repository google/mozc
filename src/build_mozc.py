#!/usr/bin/python
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

"""Script building Mozc.

Typical usage:

  % python build_mozc.py gyp
  % python build_mozc.py build base/base.gyp:base
"""

__author__ = "komatsu"

import glob
import logging
import optparse
import os
import re
import sys

from build_tools import mozc_version
from build_tools.mozc_version import GenerateVersionFile
from build_tools.test_tools import test_launcher
from build_tools.util import CheckFileOrDie
from build_tools.util import ColoredLoggingFilter
from build_tools.util import ColoredText
from build_tools.util import CopyFile
from build_tools.util import FindFileFromPath
from build_tools.util import GetNumberOfProcessors
from build_tools.util import GetRelPath
from build_tools.util import IsLinux
from build_tools.util import IsMac
from build_tools.util import IsWindows
from build_tools.util import PrintErrorAndExit
from build_tools.util import RemoveDirectoryRecursively
from build_tools.util import RemoveFile
from build_tools.util import RunOrDie
from build_tools.util import RunOrDieError

if not IsWindows():
  # android_util depends on fcntl module which doesn't exist in Windows.
  # pylint: disable=g-import-not-at-top
  from build_tools.android_util import Emulator

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
      'Android': 'out_android',
      'NaCl': 'out_nacl'
  }

  if target_platform not in platform_dict:
    raise RuntimeError('Unkown target_platform: ' + (target_platform or 'None'))

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

  # Exclude gyp files for Android unless the target platform is Android so as
  # not to generate unnecessary files under android directory when you are
  # building Mozc for other platforms.
  if options.target_platform != 'Android':
    exclude_top_dirs.append('android')

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
  elif options.target_platform == 'NaCl':
    # Add chrome NaCl Mozc gyp scripts.
    gyp_file_names.append('%s/chrome/nacl/nacl_extension.gyp' % SRC_DIR)
  elif options.target_platform == 'Android':
    # Add Android Mozc gyp scripts.
    gyp_file_names.extend(glob.glob('%s/android/*/*.gyp' % SRC_DIR))
  gyp_file_names.sort()
  return gyp_file_names


def GetAndroidHome(options):
  """Gets the root directory of the SDK from options, ANDROID_HOME or PATH."""

  if options.android_home:
    logging.info('Android home is set from  '
                 '--android_home option: %s', options.android_home)
    return options.android_home
  else:
    # If --android_home is not set, find the SDK directory from
    # ANDROID_HOME or PATH.
    if os.environ.get('ANDROID_HOME'):
      logging.info('Android home is set from ANDROID_HOME: %s',
                   os.environ['ANDROID_HOME'])
      return os.environ['ANDROID_HOME']
    for command_name in ['android',  # for ANDROID_HOME/tools
                         'adb']:  # for ANDROID_HOME/platform-tools
      command_path = FindFileFromPath(command_name)
      if command_path:
        android_home = os.path.abspath(os.path.join(
            os.path.dirname(command_path), '..'))
        logging.info('Android home is set from PATH: %s', android_home)
        return android_home
  return None


def GetAndroidNdkHome(options):
  if options.android_ndk_home:
    logging.info('Android NDK home is set from  '
                 '--android_ndk_home option: %s', options.android_ndk_home)
    return options.android_ndk_home
  else:
    command_path = FindFileFromPath('ndk-build')
    if command_path:
      android_ndk_home = os.path.abspath(os.path.dirname(command_path))
      logging.info('Android NDK home is set from PATH: %s', android_ndk_home)
      return android_ndk_home
    else:
      return None


def AddCommonOptions(parser):
  """Adds the options common among the commands."""
  parser.add_option('--verbose', '-v', dest='verbose',
                    action='callback', callback=ParseVerbose,
                    help='show verbose message.')
  return parser


def ParseVerbose(unused_option, unused_opt_str, unused_value, unused_parser):
  # Set log level to DEBUG if required.
  logging.getLogger().setLevel(logging.DEBUG)


def AddTargetPlatformOption(parser):
  # Linux environment can build for Linux, Android and NaCl.
  # --target_platform option enables this script to know which build
  # should be done. If you want NaCl build, specify "NaCl".
  # If you want Android build, specify "Android".
  if IsLinux():
    default_target = 'Linux'
  elif IsWindows():
    default_target = 'Windows'
  elif IsMac():
    default_target = 'Mac'
  parser.add_option('--target_platform', dest='target_platform',
                    default=default_target,
                    help=('Linux environment can build for Linux, Android  and '
                          'NaCl. This option enables this script to know '
                          'which build should be done. '
                          'If you want Android build, specify "Android". '
                          'If you want NaCl build, specify "NaCl".'))


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

  # Android
  parser.add_option('--android_arch', dest='android_arch',
                    type='choice',
                    choices=('arm', 'x86', 'mips', 'arm64', 'x86_64', 'mips64'),
                    default='arm',
                    help='[Android build only] Android architecture '
                    '(arm, x86, mips)')
  parser.add_option('--android_application_id', dest='android_application_id',
                    default='org.mozc.android.inputmethod.japanese',
                    help='[Android build only] Android\'s application id'
                    ' (==package ID). '
                    'If set, On Android2.1 preference screen works '
                    'incorrectly and some Java test cases failes because of '
                    'test framework\'s update.')
  parser.add_option('--android_home', dest='android_home', default=None,
                    help='[Android build only] A path to the Android SDK Home. '
                    'If not specified, automatically detected from ANDROID_HOME'
                    ' or PATH.')
  parser.add_option('--android_ndk_home', dest='android_ndk_home', default=None,
                    help='[Android build only] A path to the Android NDK Home. '
                    'If not specified, automatically detected from PATH.')
  parser.add_option('--android_hide_icon', action='store_true',
                    dest='android_hide_icon', default=False)
  parser.add_option('--android_release_icon', action='store_true',
                    dest='android_release_icon', default=False)

  # NaCl
  parser.add_option('--nacl_sdk_root', dest='nacl_sdk_root', default='',
                    help='A path to the root directory of Native Client SDK. '
                    'This is used when NaCl module build.')

  def AddFeatureOption(option_parser, feature_name, macro_name,
                       option_name):
    """Defines options like '--enable_foober' and '--disable_foober'.

    This function defines options like --enable_foober and --disable_foobar
    based on given parameters.

    Args:
      option_parser: An option parser to which options should be added.
      feature_name: A name of the feature. Will be used for option's help.
      macro_name: A macro name which will be defined when this feature is
          enabled. Will be used for option's help.
          Note that the macro is not automatically set.
          Do this by yourself (typically on common.gypi).
      option_name: A base name of the option. If 'foobar' is specified,
          --enable_foobar and --disable_foobar will be defined.

    Raises:
      ValueError: An error occurred when any name parameter is empty.
    """
    if not feature_name:
      raise ValueError('"feature_name" should be specified')
    if not option_name:
      raise ValueError('"option_name" should be specified')
    if not macro_name:
      raise ValueError('"macro_name" should be specified')
    params = {'feature_name': feature_name,
              'option_name': option_name,
              'macro_name': macro_name}
    help_template = ('Intentionally enable or disable %(feature_name)s '
                     'feature with the %(macro_name)s macro defined in code. '
                     '--enable_%(option_name)s enables it, and '
                     '--disable_%(option_name)s disables it. If both options '
                     'are not set, enables the %(feature_name)s feature '
                     'according to the target environment and branding.')
    option_parser.add_option('--enable_%(option_name)s' % params,
                             action='store_true',
                             dest=('enable_%(option_name)s' % params),
                             help=help_template % params)
    option_parser.add_option('--disable_%(option_name)s' % params,
                             action='store_false',
                             dest=('enable_%(option_name)s' % params),
                             help=help_template % params)

  AddFeatureOption(parser, feature_name='cloud handwriting',
                   macro_name='ENABLE_CLOUD_HANDWRITING',
                   option_name='cloud_handwriting')

  if IsWindows():
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
  if meta_target_name != 'package':
    return [meta_target_name]

  version = GetMozcVersion()
  target_platform = version.GetTargetPlatform()

  if target_platform == 'Android':
    targets = [SRC_DIR + '/android/android.gyp:apk']
  elif target_platform == 'Linux':
    targets = [SRC_DIR + '/server/server.gyp:mozc_server',
               SRC_DIR + '/renderer/renderer.gyp:mozc_renderer',
               SRC_DIR + '/gui/gui.gyp:mozc_tool']
    if PkgExists('ibus-1.0 >= 1.4.1'):
      targets.append(SRC_DIR + '/unix/ibus/ibus.gyp:ibus_mozc')
  elif target_platform == 'Mac':
    targets = [SRC_DIR + '/mac/mac.gyp:DiskImage']
  elif target_platform == 'Windows':
    targets = ['out_win/%s:mozc_win32_build32' % options.configuration]
    if version.GetQtVersion():
      targets += ['out_win/%sDynamic:mozc_win32_build32_dynamic'
                  % options.configuration]
    targets.append('out_win/%s_x64:mozc_win32_build64' % options.configuration)
  elif target_platform == 'NaCl':
    targets = [SRC_DIR + '/chrome/nacl/nacl_extension.gyp:nacl_mozc']

  return targets


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
  parser.add_option('--android_home', dest='android_home', default=None,
                    help='[Android build only] A path to the Android SDK Home. '
                    'If not specified, automatically detected from ANDROID_HOME'
                    ' or PATH.')
  parser.add_option('--android_device', dest='android_device',
                    help='[Android build only] specify which emulator/device '
                    'you test on. '
                    'If not specified emulators are launched and used.')

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
      x86_content = (x86_file.read()[:-1] + 'PYTHONPATH' + '=' +
                     python_path + nul + nul)
    with open(os.path.join(abs_dir, 'environment.x86'), 'wb') as x86_file:
      x86_file.write(x86_content)
    with open(os.path.join(abs_dir, 'environment.x64'), 'rb') as x64_file:
      x64_content = (x64_file.read()[:-1] + 'PYTHONPATH' + '=' +
                     python_path + nul + nul)
    with open(os.path.join(abs_dir, 'environment.x64'), 'wb') as x64_file:
      x64_file.write(x64_content)


def GypMain(options, unused_args):
  """The main function for the 'gyp' command."""
  # Generate a version definition file.
  logging.info('Generating version definition file...')
  template_path = '%s/%s' % (SRC_DIR, options.version_file)
  version_path = '%s/mozc_version.txt' % SRC_DIR
  if options.noqt or options.target_platform in ['Android', 'NaCl']:
    qt_version = ''
  else:
    qt_version = '5'
  GenerateVersionFile(template_path, version_path, options.target_platform,
                      options.android_application_id,
                      options.android_arch, qt_version)
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
    gyp_command = [sys.executable, gyp_main_file]
    os.environ['PYTHONPATH'] = os.pathsep.join(
        [os.path.join(gyp_dir, 'pylib'), original_python_path])
    # Following script tests if 'import gyp' is now available and its
    # location is expected one. By using this script, make sure
    # 'import gyp' actually loads 'third_party/gyp/pylib/gyp'.
    gyp_check_script = os.path.join(
        ABS_SCRIPT_DIR, 'build_tools', 'ensure_gyp_module_path.py')
    expected_gyp_module_path = os.path.join(gyp_dir, 'pylib', 'gyp')
    RunOrDie([sys.executable, gyp_check_script,
              '--expected=%s' % expected_gyp_module_path])

  # Set the generator name.
  os.environ['GYP_GENERATORS'] = 'ninja'

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

  gyp_options.extend(['-D', 'python_executable=%s' % sys.executable])

  gyp_options.extend(gyp_file_names)

  if options.branding:
    gyp_options.extend(['-D', 'branding=%s' % options.branding])

  # Qt configurations
  if options.noqt or target_platform in ['Android', 'NaCl']:
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

  # Android
  if target_platform == 'Android':
    android_home = GetAndroidHome(options)
    android_ndk_home = GetAndroidNdkHome(options)
    if not android_home or not os.path.isdir(android_home):
      raise ValueError(
          'Android Home was not found. '
          'Use --android_home option or make the home direcotry '
          'be included in PATH environment variable.')
    if not android_ndk_home or not os.path.isdir(android_ndk_home):
      raise ValueError(
          'Android NDK Home was not found. '
          'Use --android_ndk_home option or make the home direcotry '
          'be included in PATH environment variable.')

    gyp_options.extend(['-D', 'android_home=%s' % android_home])
    gyp_options.extend(['-D', 'android_arch=%s' % options.android_arch])
    gyp_options.extend(['-D', 'android_ndk_home=%s' % android_ndk_home])
    gyp_options.extend(['-D', 'android_application_id=%s' %
                        options.android_application_id])
    if options.android_hide_icon:
      gyp_options.extend(['-D', 'android_hide_icon=1'])
    else:
      gyp_options.extend(['-D', 'android_hide_icon=0'])
    if options.android_release_icon:
      gyp_options.extend(['-D', 'android_release_icon=1'])
    else:
      gyp_options.extend(['-D', 'android_release_icon=0'])

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

  def SetCommandLineForFeature(option_name, windows=False, mac=False,
                               linux=False, android=False, nacl=False):
    """Updates an option like '--enable_foober' and add a -D argument for gyp.

    This function ensures an option like '--enable_foober' exists and it has a
    default boolean for each platform based on givem parameters. This
    function also sets a '-D' command line option for gyp as
    '-D enable_foober=0' or '-D enable_foober=1' depending on the actual value
    of the target option.

    Args:
      option_name: A base name of the option. If 'foobar' is given,
          '--enable_foober' option will be checked.
      windows: A boolean which replesents the default value of the target
          option on Windows platform.
      mac: A boolean which replesents the default value of the target option
          on MacOS X platform.
      linux: A boolean which replesents the default value of the target option
          on Linux platform.
      android: A boolean which replesents the default value of the target
          option on Android platform.
      nacl: A boolean which replesents the default value of the target
          option on NaCl.

    Raises:
      ValueError: An error occurred when 'option_name' is empty.
    """
    if not option_name:
      raise ValueError('"option_name" should be specified')

    default_enabled = {'Windows': windows,
                       'Mac': mac,
                       'Linux': linux,
                       'Android': android,
                       'NaCl': nacl}.get(target_platform, False)
    enable_option_name = 'enable_%s' % option_name
    enabled = options.ensure_value(enable_option_name, default_enabled)
    gyp_options.extend(['-D',
                        '%s=%s' % (enable_option_name, 1 if enabled else 0)])

  is_official = (options.branding == 'GoogleJapaneseInput')
  is_official_dev = (is_official and version.IsDevChannel())

  SetCommandLineForFeature(option_name='cloud_handwriting',
                           linux=is_official_dev,
                           windows=is_official_dev,
                           mac=is_official_dev)

  target_platform_value = target_platform
  gyp_options.extend(['-D', 'target_platform=%s' % target_platform_value])

  if IsWindows():
    gyp_options.extend(['-G', 'msvs_version=2015'])

  if (target_platform == 'Linux' and
      '%s/unix/ibus/ibus.gyp' % SRC_DIR in gyp_file_names):
    gyp_options.extend(['-D', 'use_libibus=1'])

  if target_platform == 'NaCl':
    if options.nacl_sdk_root:
      nacl_sdk_root = os.path.abspath(options.nacl_sdk_root)
    else:
      nacl_sdk_root = os.path.abspath(os.path.join(EXT_THIRD_PARTY_DIR,
                                                   'nacl_sdk', 'pepper_40'))
    if not os.path.isdir(nacl_sdk_root):
      PrintErrorAndExit('The nacl_sdk_root directory (%s) does not exist.'
                        % options.nacl_sdk_root)
    gyp_options.extend(['-D', 'nacl_sdk_root=%s' % nacl_sdk_root])

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
    abs_out_win_dir = GetBuildBaseName(target_platform)
    copy_script = os.path.join(
        ABS_SCRIPT_DIR, 'build_tools', 'copy_dll_and_symbol.py')
    copy_params = []
    if qt_version == '5':
      copy_params.append({
          'basenames': 'Qt5Cored;Qt5Guid;Qt5Widgetsd',
          'dll_paths': abs_qt_bin_dir,
          'pdb_paths': abs_qt_lib_dir,
          'target_dir': os.path.join(abs_out_win_dir, 'DebugDynamic')})
      copy_params.append({
          'basenames': 'Qt5Core;Qt5Gui;Qt5Widgets',
          'dll_paths': abs_qt_bin_dir,
          'pdb_paths': abs_qt_lib_dir,
          'target_dir': os.path.join(abs_out_win_dir, 'ReleaseDynamic')})
      copy_params.append({
          'basenames': 'qwindowsd',
          'dll_paths': os.path.join(abs_qtdir, 'plugins', 'platforms'),
          'pdb_paths': os.path.join(abs_qtdir, 'plugins', 'platforms'),
          'target_dir': os.path.join(abs_out_win_dir, 'DebugDynamic',
                                     'platforms')})
      copy_params.append({
          'basenames': 'qwindows',
          'dll_paths': os.path.join(abs_qtdir, 'plugins', 'platforms'),
          'pdb_paths': os.path.join(abs_qtdir, 'plugins', 'platforms'),
          'target_dir': os.path.join(abs_out_win_dir, 'ReleaseDynamic',
                                     'platforms')})
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


def GetNinjaTargetName(target):
  """Extracts the build target name for Ninja."""
  if ':' not in target:
    PrintErrorAndExit('Invalid target: %s' % target)
  (_, target_name) = target.split(':')
  return target_name


def BuildWithNinja(options, targets):
  """Build the targets with Ninja."""
  if hasattr(options, 'android_device'):
    # Only for android testing.
    os.environ['ANDROID_DEVICES'] = options.android_device

  short_basename = GetBuildShortBaseName(GetMozcVersion().GetTargetPlatform())
  build_arg = '%s/%s' % (short_basename, options.configuration)

  ninja = GetNinjaPath()

  ninja_targets = [GetNinjaTargetName(target) for target in targets]
  RunOrDie([ninja, '-C', build_arg] + ninja_targets)


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
    output_dir: The directory of output resutls.
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




def RunTests(target_platform, configuration, parallel_num):
  """Run built tests actually.

  Args:
    target_platform: The build target ('Android', 'Windows', etc.)
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
      except RunOrDieError, e:
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


def RunTestsOnAndroid(options, build_args):
  """Run a test suite for the Android version."""
  try:
    emulators = []
    serialnumbers = []
    if options.android_device:
      serialnumbers.append(options.android_device)
    else:
      # Temporary AVDs are created beneath android_sdk_home.
      android_sdk_home = os.path.join(
          GetBuildBaseName(GetMozcVersion().GetTargetPlatform()),
          options.configuration,
          'android_sdk_home')
      android_home = GetAndroidHome(options)

      android_arch = GetMozcVersion().GetAndroidArch()
      if android_arch == 'arm':
        avd_configs = [
            {'--name': 'Nexus5-Api21-arm-WVGA800',
             '--target': 'android-21',
             '--abi': 'default/armeabi-v7a',
             '--device': 'Nexus 5',
             '--skin': 'WVGA800'},
            {'--name': 'Nexus10-Api21-arm-WXGA800',
             '--target': 'android-21',
             '--abi': 'default/armeabi-v7a',
             '--device': 'Nexus 10',
             '--skin': 'WXGA800'},]
      elif android_arch == 'x86':
        avd_configs = [
            {'--name': 'Nexus5-Api21-x86-WVGA800',
             '--target': 'android-21',
             '--abi': 'default/x86',
             '--device': 'Nexus 5',
             '--skin': 'WVGA800'},
            {'--name': 'Nexus10-Api21-x86-WXGA800',
             '--target': 'android-21',
             '--abi': 'default/x86',
             '--device': 'Nexus 10',
             '--skin': 'WXGA800'},]
      elif android_arch == 'mips':
        avd_configs = [
            {'--name': 'NexusS-Api15-mips-WVGA800',
             '--target': 'android-15',
             '--abi': 'default/mips',
             '--device': 'Nexus S',
             '--skin': 'WVGA800'},
            # As of 2014-05-27, the latest target of MIPS is 17.
            {'--name': 'Nexus5-Api17-mips-WVGA800',
             '--target': 'android-17',
             '--abi': 'default/mips',
             '--device': 'Nexus 5',
             '--skin': 'WVGA800'},
            {'--name': 'Nexus10-Api17-mips-WXGA800',
             '--target': 'android-17',
             '--abi': 'default/mips',
             '--device': 'Nexus 10',
             '--skin': 'WXGA800'},]
      else:
        avd_configs = []

      emulators = Emulator.LaunchAll(android_sdk_home, avd_configs,
                                     android_home)
      serialnumbers.extend([emulator.serial for emulator in emulators])

    # Run native and Java tests.
    # If --configuration is Release, Java tests are skipped.
    if options.configuration == 'Release':
      targets = ['run_native_test']
      logging.info('As this is Relase configuration, Java tests are skipped.')
    else:
      # run_java_test must be executed after run_native_test
      # because package manager, which is mandatory to run Java tests,
      # requires minutes to get ready.
      targets = ['run_native_test', 'run_java_test']

    android_gyp = os.path.join(SRC_DIR, 'android', 'android.gyp')
    for target in targets:
      (build_options, build_targets) = ParseBuildOptions(
          build_args + ['%s:%s' % (android_gyp, target)])
      # Injects android_device attribute to build_options.
      # The attribute will be used as Makefile parameter.
      # Q: Why setattr?
      # A: BuildMain is invoked from build command and the command
      # does not need 'android_device' option.
      # So ParseBuildOption method doesn't accept --android_device
      # argument. Thus build_options doesn't have android_device attribute
      # but we need the option here becuase there is no other way to specifiy
      # Makefile parameter.
      setattr(build_options, 'android_device', ','.join(serialnumbers))
      logging.info('build_options=%s', build_options)
      BuildMain(build_options, build_targets)
  finally:
    # Terminate the emulators.
    for emulator in emulators:
      emulator.Terminate()


def RunTestsOnNaCl(targets, build_args):
  """Run a test suite for the NaCl version."""
  # Currently we can only run the limited test set which is defined as
  # nacl_test_targets in nacl_extension.gyp.
  if targets:
    PrintErrorAndExit('Targets [%s] are not supported.' % ', '.join(targets))
  nacl_gyp = os.path.join(SRC_DIR, 'chrome', 'nacl', 'nacl_extension.gyp')
  (build_options, build_targets) = ParseBuildOptions(
      build_args + [nacl_gyp + ':run_nacl_test'])
  # Run the test suite in NaCl.
  BuildMain(build_options, build_targets)


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
  for i in xrange(len(args)):
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
  if target_platform == 'NaCl':
    return RunTestsOnNaCl(targets, build_options)

  if not targets:
    # TODO(yukawa): Change the notation rule of 'targets' to reduce the gap
    # between Ninja and make.
    if target_platform == 'Windows':
      targets.append('out_win/%s:unittests' % options.configuration)
    else:
      targets.append('%s/gyp/tests.gyp:unittests' % SRC_DIR)
    if target_platform == 'Android' and options.configuration != 'Release':
      targets.append('%s/android/android.gyp:build_java_test' % SRC_DIR)

  # Build the test targets
  (build_opts, build_args) = ParseBuildOptions(build_options + targets)
  BuildMain(build_opts, build_args)

  # Run tests actually
  if target_platform == 'Android':
    RunTestsOnAndroid(options, build_options)
  else:
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

  if IsLinux():
    # Remove auto-generated files.
    file_names.append(os.path.join(SRC_DIR, 'android', 'AndroidManifest.xml'))
    file_names.append(os.path.join(
        SRC_DIR, 'android', 'tests', 'AndroidManifest.xml'))
    # Remove a symbolic link to android/resources/res
    file_names.append(os.path.join(SRC_DIR, 'android', 'resources', 'res'))
    directory_names.append(os.path.join(SRC_DIR, 'android', 'assets'))
    # Delete files/dirs generated by Android SDK/NDK.
    android_library_projects = [
        '',
        'resources',
        'tests',
        ]
    android_generated_dirs = ['bin', 'gen', 'obj', 'libs', 'gen_for_adt']
    for project in android_library_projects:
      for directory in android_generated_dirs:
        directory_names.append(
            os.path.join(SRC_DIR, 'android', project, directory))

  # Remove files.
  for file_name in file_names:
    RemoveFile(file_name)
  # Remove directories.
  for directory_name in directory_names:
    RemoveDirectoryRecursively(directory_name)


def ShowHelpAndExit():
  """Shows the help message."""
  print 'Usage: build_mozc.py COMMAND [ARGS]'
  print 'Commands: '
  print '  gyp          Generate project files.'
  print '  build        Build the specified target.'
  print '  runtests     Build all tests and run them.'
  print '  clean        Clean all the build files and directories.'
  print ''
  print 'See also the comment in the script for typical usage.'
  sys.exit(1)


def main():
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
  logging.getLogger().addFilter(ColoredLoggingFilter())

  if len(sys.argv) < 2:
    ShowHelpAndExit()

  # Move to the Mozc root source directory only once since os.chdir
  # affects functions in os.path and that causes troublesome errors.
  os.chdir(MOZC_ROOT)

  command = sys.argv[1]
  args = sys.argv[2:]

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


if __name__ == '__main__':
  main()
