#!/usr/bin/python
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

"""Script building Mozc.

Typical usage:

  % python build_mozc.py gyp
  % python build_mozc.py build_tools -c Release
  % python build_mozc.py build base/base.gyp:base
"""

__author__ = "komatsu"

import glob
import logging
import optparse
import os
import re
import sys

from build_tools import android_util
from build_tools import mozc_version
from build_tools.android_util import Emulator
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

SRC_DIR = '.'
# We need to obtain the absolute path of this script before we
# call MoveToTopLevelSourceDirectory(), which may change the
# current directory.
# Note that if any import above has already changed the current
# directory, this code cannot work anymore.
ABS_SCRIPT_PATH = os.path.abspath(__file__)

sys.path.append(SRC_DIR)


def GetMozcVersion():
  """Returns MozcVersion instance."""
  # TODO(matsuzakit): Caching might be better.
  return mozc_version.MozcVersion('%s/mozc_version.txt' % SRC_DIR)


def GetBuildBaseName(options, target_platform):
  """Returns the build base directory.

  Determination priority is;
  1. options.build_base
  2. target_platform (typically in mozc_version.txt)
  3. options.target_platform

  Args:
    options: Options which might have build_base and/or target_platform
    target_platform: Target platform (typically read from mozc_version.txt)
      This can be None.
  """
  if options.build_base:
    return options.build_base
  # For some reason, xcodebuild does not accept absolute path names for
  # the -project parameter. Convert the original_directory_name to a
  # relative path from the build top level directory.
  if target_platform is None:
    target_platform = options.target_platform
  build_base = ''

  generator = GetGeneratorName(options.ensure_value('gyp_generator', None))
  if generator == 'ninja':
    build_base = 'out'
  elif generator == 'msvs':
    build_base = 'out_win'
  elif target_platform == 'Mac':
    build_base = 'out_mac'
  elif target_platform == 'Linux' or target_platform == 'ChromeOS':
    build_base = 'out_linux'
  elif target_platform == 'Android':
    build_base = 'out_android'
  elif target_platform == 'NaCl':
    build_base = 'out_nacl'
  else:
    logging.error('Unsupported platform: %s', target_platform)

  # On Linux, seems there is no way to specify the build_base directory
  # inside common.gypi
  if IsWindows() or IsMac():
    build_base = os.path.join(GetTopLevelSourceDirectoryName(), build_base)

  return build_base


def GetGeneratorName(generator):
  """Gets the generator name based on the platform."""
  if generator:
    return generator
  elif IsWindows():
    return 'ninja'
  elif IsMac():
    return 'xcode'
  else:
    return 'make'


def GetPkgConfigCommand():
  """Get the pkg-config command name.

  Returns:
    pkg-config config command name.
  """
  return os.environ.get('PKG_CONFIG', 'pkg-config')


# TODO(team): Move this to build_tools/util.py
def PkgExists(*packages):
  """Return if the specified package exists or not.

  Args:
    *packages: packages to be examined.

  Returns:
    True if the specified package exists.
  """
  try:
    command_line = [GetPkgConfigCommand(), '--exists']
    command_line.extend(packages)
    RunOrDie(command_line)
    return True
  except RunOrDieError:
    logging.info('%s failed', ' '.join(command_line))
  return False




def GetGypFileNames(options):
  """Gets the list of gyp file names."""
  gyp_file_names = []
  mozc_top_level_names = glob.glob('%s/*' % SRC_DIR)

  if not options.language == 'japanese':
    language_path = '%s/languages/%s' % (SRC_DIR, options.language)
    if not os.path.exists(language_path):
      PrintErrorAndExit('Can not find language directory: %s ' % language_path)
    mozc_top_level_names.append(language_path)

  # Exclude the gyp directory where we have special gyp files like
  # breakpad.gyp that we should exclude.
  exclude_top_dirs = ['gyp']

  # Exclude gyp files for Android unless the target platform is Android so as
  # not to generate unnecessary files under android directory when you are
  # building Mozc for other platforms.
  if options.target_platform != 'Android':
    exclude_top_dirs.append('android')

  mozc_top_level_names = [x for x in mozc_top_level_names if
                          os.path.basename(x) not in exclude_top_dirs]

  for name in mozc_top_level_names:
    gyp_file_names.extend(glob.glob(name + '/*.gyp'))
  gyp_file_names.extend(glob.glob('%s/build_tools/*/*.gyp' % SRC_DIR))
  # Include tests gyp
  gyp_file_names.append('%s/gyp/tests.gyp' % SRC_DIR)
  # Include subdirectory of dictionary
  gyp_file_names.extend(glob.glob('%s/dictionary/*/*.gyp' % SRC_DIR))
  # Include subdirectories of data_manager
  gyp_file_names.extend(glob.glob('%s/data_manager/*/*.gyp' % SRC_DIR))
  # Include subdirectory of rewriter
  gyp_file_names.extend(glob.glob('%s/rewriter/*/*.gyp' % SRC_DIR))
  # Include subdirectory of win32 and breakpad for Windows
  if options.target_platform == 'Windows':
    gyp_file_names.extend(glob.glob('%s/win32/*/*.gyp' % SRC_DIR))
    gyp_file_names.extend(glob.glob('third_party/breakpad/*.gyp'))
  elif options.target_platform == 'Linux':
    gyp_file_names.extend(glob.glob('%s/unix/*/*.gyp' % SRC_DIR))
    # Add ibus.gyp if ibus version is >=1.4.1.
    if not PkgExists('ibus-1.0 >= 1.4.1'):
      logging.info('removing ibus.gyp.')
      gyp_file_names.remove('%s/unix/ibus/ibus.gyp' % SRC_DIR)
  elif options.target_platform == 'ChromeOS':
    gyp_file_names.extend(glob.glob('%s/unix/ibus/*.gyp' % SRC_DIR))
  elif options.target_platform == 'NaCl':
    # Add chrome NaCl Mozc gyp scripts.
    gyp_file_names.append('%s/chrome/nacl/nacl_extension.gyp' % SRC_DIR)
    gyp_file_names.extend(glob.glob('third_party/zlib/v1_2_3/zlib.gyp'))
  gyp_file_names.extend(glob.glob('third_party/jsoncpp/*.gyp'))
  gyp_file_names.sort()
  return gyp_file_names


def GetTopLevelSourceDirectoryName():
  """Gets the top level source directory name."""
  script_file_directory_name = os.path.dirname(ABS_SCRIPT_PATH)
  if SRC_DIR == '.':
    return script_file_directory_name
  num_components = len(SRC_DIR.split('/'))
  return os.path.join(script_file_directory_name, *(['..'] * num_components))


def MoveToTopLevelSourceDirectory():
  """Moves to the build top level directory."""
  os.chdir(GetTopLevelSourceDirectoryName())


def GetBuildScriptDirectoryName():
  """Gets the directory name of this script."""
  return os.path.dirname(ABS_SCRIPT_PATH)


def RunPackageVerifiers(build_dir):
  """Runs script to verify created packages/binaries.

  Args:
    build_dir: the directory where build results are.
  """
  binary_size_checker = os.path.join(
      GetBuildScriptDirectoryName(), 'build_tools', 'binary_size_checker.py')
  RunOrDie([binary_size_checker, '--target_directory', build_dir])


def AddCommonOptions(parser):
  """Adds the options common among the commands."""
  parser.add_option('--build_base', dest='build_base',
                    help='Specifies the base directory of the built binaries.')
  parser.add_option('--language', dest='language', default='japanese',
                    help='Specify the target language to build.')
  return parser


def AddGeneratorOption(parser):
  """Generator option is used by gyp and clean."""
  options = {'dest': 'gyp_generator',
             'help': 'Specifies the generator for GYP.' }
  if IsWindows():
    options['default'] = 'ninja'
  parser.add_option('--gyp_generator', **options)
  return parser


def AddTargetPlatformOption(parser):
  # Linux environment can build for Linux, Android and ChromeOS.
  # --target_platform option enables this script to know which build
  # should be done. If you want ChromeOS build, specify "ChromeOS".
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
                          'ChromeOS. This option enables this script to know '
                          'which build should be done. '
                          'If you want Android build, specify "Android". '
                          'If you want ChromeOS build, specify "ChromeOS".'))


def GetDefaultWixPath():
  """Returns the default Wix directory.."""
  abs_path = ''
  return abs_path


def ParseGypOptions(args=None, values=None):
  """Parses command line options for the gyp command."""
  parser = optparse.OptionParser(usage='Usage: %prog gyp [options]')
  AddCommonOptions(parser)
  AddGeneratorOption(parser)
  default_branding = 'Mozc'
  parser.add_option('--branding', dest='branding', default=default_branding,
                    help='Specifies the branding. [default: %default]')
  parser.add_option('--gypdir', dest='gypdir',
                    help='Specifies the location of GYP to be used.')
  parser.add_option('--noqt', action='store_true', dest='noqt', default=False)
  parser.add_option('--qtdir', dest='qtdir',
                    default=os.getenv('QTDIR', None),
                    help='Qt base directory to be used.')
  parser.add_option('--channel_dev', action='store', dest='channel_dev',
                    type='int',
                    help='Pass --channel_dev=1 if you need to build mozc with '
                    'the CHANNEL_DEV macro enabled. Pass 0 if you don\'t need '
                    'the macro. If --channel_dev= flag is not passed, Mozc '
                    'version is used to deretmine if the macro is necessary.')
  parser.add_option('--version_file', dest='version_file',
                    help='use the specified version template file',
                    default='mozc_version_template.txt')
  parser.add_option('--rsync', dest='rsync', default=False, action='store_true',
                    help='use rsync to copy files instead of builtin function')
  AddTargetPlatformOption(parser)

  parser.add_option('--mac_dir', dest='mac_dir',
                    help='A path to the root directory of third party '
                    'libraries for Mac build which will be passed to gyp '
                    'files.')
  parser.add_option('--android_arch_abi', dest='android_arch_abi',
                    default='armeabi',
                    help='Arndoid abi list. e.g. "armeabi x86".')
  parser.add_option('--android_application_id', dest='android_application_id',
                    default='org.mozc.android.inputmethod.japanese',
                    help='Android\s application id (==package ID). '
                    'If set, On Android2.1 preference screen works '
                    'incorrectly and some Java test cases failes because of '
                    'test framework\s update.')
  parser.add_option('--nacl_sdk_root', dest='nacl_sdk_root', default='',
                    help='A path to the root directory of Native Client SDK. '
                    'This is used when NaCl module build.')
  parser.add_option('--android_sdk_home', dest='android_sdk_home', default=None,
                    help='[Android build only] A path to the Android SDK Home. '
                    'If not specified, automatically detected from PATH.')

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

  AddFeatureOption(parser, feature_name='webservice infolist',
                   macro_name='ENABLE_WEBSERVICE_INFOLIST',
                   option_name='webservice_infolist')
  AddFeatureOption(parser, feature_name='cloud sync',
                   macro_name='ENABLE_CLOUD_SYNC',
                   option_name='cloud_sync')
  AddFeatureOption(parser, feature_name='cloud handwriting',
                   macro_name='ENABLE_CLOUD_HANDWRITING',
                   option_name='cloud_handwriting')
  AddFeatureOption(parser, feature_name='http client',
                   macro_name='MOZC_ENABLE_HTTP_CLIENT',
                   option_name='http_client')
  AddFeatureOption(parser, feature_name='ambiguous search',
                   macro_name='MOZC_ENABLE_AMBIGUOUS_SEARCH',
                   option_name='ambiguous_search')
  AddFeatureOption(parser, feature_name='typing correction',
                   macro_name='MOZC_ENABLE_TYPING_CORRECTION',
                   option_name='typing_correction')
  AddFeatureOption(parser, feature_name='history_deletion',
                   macro_name='MOZC_ENABLE_HISTORY_DELETION',
                   option_name='history_deletion')

  # TODO(yukawa): Remove this option when Zinnia can be built on Windows with
  #               enabling Unicode.
  use_zinnia_default = True

  parser.add_option('--server_dir', dest='server_dir',
                    default='/usr/lib/mozc',
                    help='A path to the directory to be installed server '
                    'executable. This option is only available for Linux.')

  if IsWindows():
    # Zinnia on Windows cannot be enabled because of compile error.
    use_zinnia_default = False
  parser.add_option('--use_zinnia', dest='use_zinnia',
                    default=use_zinnia_default,
                    help='Use Zinnia if specified.')

  if IsWindows():
    parser.add_option('--wix_dir', dest='wix_dir',
                      default=GetDefaultWixPath(),
                      help='A path to the binary directory of wix.')

    # For internal Windows builds, gyp is expected to generate solution files
    # for Visual Studio 2010, which is a default compiler for Mozc.  However,
    # you can specify the target version explicitly via 'msvs_version' option
    # as follows.
    parser.add_option('--msvs_version', dest='msvs_version', default='2010',
                      help='Specifies the target MSVS version.')

  return parser.parse_args(args, values)


def ExpandMetaTarget(options, meta_target_name):
  """Returns a list of build targets with expanding meta target name.

  If the specified name is 'package', returns a list of build targets for
  building production package.

  If the specified name is not a meta target name, returns it as a list.

  Args:
    meta_target_name: meta target name to be expanded.

  Returns:
    A list of build targets with meta target names expanded.
  """
  if meta_target_name != 'package':
    return [meta_target_name]

  target_platform = GetMozcVersion().GetTargetPlatform()

  # Note that ChromeOS does not use this method.
  if target_platform == 'Android':
    targets = [SRC_DIR + '/android/android.gyp:apk']
  elif target_platform == 'Linux':
    targets = [SRC_DIR + '/server/server.gyp:mozc_server',
               SRC_DIR + '/renderer/renderer.gyp:mozc_renderer',
               SRC_DIR + '/gui/gui.gyp:mozc_tool']
    if PkgExists('ibus-1.0 >= 1.4.1'):
      targets.append(SRC_DIR + '/unix/ibus/ibus.gyp:ibus_mozc')
  elif target_platform == 'ChromeOS':
    targets.append(SRC_DIR + '/unix/ibus/ibus.gyp:ibus_mozc')
  elif target_platform == 'Mac':
    targets = [SRC_DIR + '/mac/mac.gyp:DiskImage']
  elif target_platform == 'Windows':
    targets = ['out/%s:mozc_win32_build32' % options.configuration]
    build_dir = os.path.abspath(os.path.join(
        GetBuildBaseName(options, target_platform),
        '%sDynamic' % options.configuration))
    qtcore_dll = os.path.join(build_dir, 'QtCore4.dll')
    qtcored_dll = os.path.join(build_dir, 'QtCored4.dll')
    if os.path.exists(qtcore_dll) or os.path.exists(qtcored_dll):
      # This means that Mozc is configured to use DLL versioin of Qt.
      # Let's build Mozc with DLL version of C++ runtime for the compatibility.
      targets += [
          'out/%sDynamic:mozc_win32_build32_dynamic' % options.configuration]
    targets += ['out/%s_x64:mozc_win32_build64' % options.configuration,
                'out/%s:mozc_installers_win' % options.configuration]
  elif target_platform == 'NaCl':
    targets = [SRC_DIR + '/chrome/nacl/nacl_extension.gyp:nacl_mozc']

  return targets


def ParseBuildOptions(args=None, values=None):
  """Parses command line options for the build command."""
  parser = optparse.OptionParser(usage='Usage: %prog build [options]')
  AddCommonOptions(parser)
  if IsLinux():
    default_build_concurrency = GetNumberOfProcessors() * 2
    parser.add_option('--jobs', '-j', dest='jobs',
                      default=('%d' % default_build_concurrency),
                      metavar='N', help='run build jobs in parallel')
  parser.add_option('--configuration', '-c', dest='configuration',
                    default='Debug', help='specify the build configuration.')

  (options, args) = parser.parse_args(args, values)
  targets = []
  for arg in args:
    targets.extend(ExpandMetaTarget(options, arg))
  return (options, targets)


def ParseRunTestsOptions(args=None, values=None):
  """Parses command line options for the runtests command."""
  parser = optparse.OptionParser(
      usage='Usage: %prog runtests [options] [test_targets] [-- build options]')
  AddCommonOptions(parser)
  if IsLinux():
    default_build_concurrency = GetNumberOfProcessors() * 2
    parser.add_option('--jobs', '-j', dest='jobs',
                      default=('%d' % default_build_concurrency),
                      metavar='N', help='run build jobs in parallel')
  default_test_jobs = GetNumberOfProcessors() * 2
  parser.add_option('--test_jobs', dest='test_jobs', type='int',
                    default=default_test_jobs,
                    metavar='N', help='run test jobs in parallel')
  parser.add_option('--test_size', dest='test_size', default='small')
  parser.add_option('--configuration', '-c', dest='configuration',
                    default='Debug', help='specify the build configuration.')
  parser.add_option('--android_device', dest='android_device',
                    help='specify which emulator/device you test on. '
                    'If not specified emulators are launched and used.')

  return parser.parse_args(args, values)


def ParseCleanOptions(args=None, values=None):
  """Parses command line options for the clean command."""
  parser = optparse.OptionParser(
      usage='Usage: %prog clean [-- build options]')
  AddCommonOptions(parser)
  AddGeneratorOption(parser)
  AddTargetPlatformOption(parser)

  return parser.parse_args(args, values)




def GypMain(options, unused_args):
  """The main function for the 'gyp' command."""
  # Generate a version definition file.
  # The version file is used in this method to check if this is dev channel.
  logging.info('Generating version definition file...')
  template_path = '%s/%s' % (SRC_DIR, options.version_file)
  version_path = '%s/mozc_version.txt' % SRC_DIR
  GenerateVersionFile(template_path, version_path, options.target_platform,
                      options.channel_dev, options.android_application_id)
  version = GetMozcVersion()
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
    gyp_command = os.path.join(gyp_dir, 'gyp')
  else:
    # Use third_party/gyp/gyp when 'gypdir' option is not specified. This is
    # the default and official way to build Mozc. In this case, you need to
    # consider the case where a Python package named 'gyp' has already been
    # installed in the target machine. So we update PYTHONPATH so that
    # third_party/gyp/gyp can find its own library modules (they are
    # installed under 'third_party/gyp/pylib') rather than system-installed
    # GYP modules.
    gyp_dir = os.path.abspath(os.path.join(
        GetTopLevelSourceDirectoryName(), 'third_party', 'gyp'))
    gyp_command = os.path.join(gyp_dir, 'gyp')
    # Make sure if 'third_party/gyp/gyp' exists. Note that Mozc's tarball
    # package no longer contains 'third_party/gyp/gyp'.
    if not os.path.exists(gyp_command):
      message = (
          'GYP does not exist at %s. Please run "gclient" to download GYP. '
          'If you want to use system-installed GYP, use --gypdir option to '
          'specify its location. e.g., '
          '"python build_mozc.py gyp --gypdir=/usr/bin"' % gyp_command)
      PrintErrorAndExit(message)
    os.environ['PYTHONPATH'] = os.pathsep.join(
        [os.path.join(gyp_dir, 'pylib'), original_python_path])
    # Following script tests if 'import gyp' is now available and its
    # location is expected one. By using this script, make sure
    # 'import gyp' actually loads 'third_party/gyp/pylib/gyp'.
    gyp_check_script = os.path.join(
        GetBuildScriptDirectoryName(), 'build_tools',
        'ensure_gyp_module_path.py')
    expected_gyp_module_path = os.path.join(gyp_dir, 'pylib', 'gyp')
    RunOrDie([sys.executable, gyp_check_script,
              '--expected=%s' % expected_gyp_module_path])

  # Copy jsoncpp.gyp to the third party directory.
  CopyFile('%s/gyp/jsoncpp.gyp' % SRC_DIR, 'third_party/jsoncpp/jsoncpp.gyp')
  if options.target_platform == 'NaCl':
    # Copy zlib.gyp to the third party directory.
    CopyFile('%s/gyp/zlib.gyp' % SRC_DIR, 'third_party/zlib/v1_2_3/zlib.gyp')
  # Copy breakpad.gyp to the third party directory, if necessary.
  if IsWindows():
    CopyFile('%s/gyp/breakpad.gyp' % SRC_DIR,
             'third_party/breakpad/breakpad.gyp')

  # Determine the generator name.
  generator = GetGeneratorName(options.gyp_generator)
  os.environ['GYP_GENERATORS'] = generator
  logging.info('Build tool: %s', generator)

  # Get and show the list of .gyp file names.
  gyp_file_names = GetGypFileNames(options)
  logging.info('GYP files:')
  for file_name in gyp_file_names:
    logging.info('- %s', file_name)

  # Build GYP command line.
  logging.info('Building GYP command line...')
  command_line = [sys.executable, gyp_command,
                  '--depth=.',
                  '--include=%s/gyp/common.gypi' % SRC_DIR]


  mozc_root = os.path.abspath(GetTopLevelSourceDirectoryName())
  command_line.extend(['-D', 'abs_depth=%s' % mozc_root])

  command_line.extend(['-D', 'python_executable=%s' % sys.executable])

  command_line.extend(gyp_file_names)

  if options.branding:
    command_line.extend(['-D', 'branding=%s' % options.branding])

  if options.noqt:
    command_line.extend(['-D', 'use_qt=NO'])
  else:
    if options.target_platform == 'Linux' and not options.qtdir:
      # Check if Qt libraries are installed.
      system_qt_found = PkgExists('QtCore >= 4.0', 'QtCore < 5.0',
                                  'QtGui >= 4.0', 'QtGui < 5.0')
      if not system_qt_found:
        PrintErrorAndExit('Qt4 is required to build GUI Tool. '
                          'Specify --noqt to skip building GUI Tool.')
    command_line.extend(['-D', 'use_qt=YES'])
  if options.qtdir:
    command_line.extend(['-D', 'qt_dir=%s' % os.path.abspath(options.qtdir)])
  else:
    command_line.extend(['-D', 'qt_dir='])

  if options.target_platform == 'Windows' and options.wix_dir:
    command_line.extend(['-D', 'use_wix=YES'])
    command_line.extend(['-D', 'wix_dir=%s' % options.wix_dir])
  else:
    command_line.extend(['-D', 'use_wix=NO'])

  android_sdk_home = options.android_sdk_home
  if version.GetTargetPlatform() == 'Android':
    if not android_sdk_home:
      # If --android_sdk_home is not set, find the SDK directory from PATH.
      # Usualy $SDK/tools and/or $SDK/platform-tools is/or in PATH.
      for command_name in ['android',  # for ANDROID_SDK_HOME/tools
                           'adb']:  # for ANDROID_SDK_HOME/platform-tools
        command_path = FindFileFromPath(command_name)
        if command_path:
          android_sdk_home = os.path.abspath(
              os.path.join(os.path.dirname(command_path), '..'))
          break
    if not android_sdk_home or not os.path.isdir(android_sdk_home):
      raise ValueError(
          'Android SDK Home was not found. '
          'Use --android_sdk_home option or make the home direcotry '
          'be included in PATH environment variable.')
  command_line.extend(['-D', 'android_sdk_home=%s' % android_sdk_home])
  command_line.extend(['-D', 'android_arch_abi=%s' % options.android_arch_abi])
  command_line.extend(['-D', 'android_application_id=%s' %
                       options.android_application_id])
  command_line.extend(['-D', 'build_base=%s' %
                       GetBuildBaseName(options, version.GetTargetPlatform())])

  disable_unittest_if_not_available = True
  if disable_unittest_if_not_available:
    required_modules = ['gmock', 'gtest']
    for module in required_modules:
      module_dir = os.path.abspath(
          os.path.join(GetTopLevelSourceDirectoryName(), 'third_party', module))
      if not os.path.exists(module_dir):
        logging.warning('%s not found. Disabling unittest.', module)
        command_line.extend(['-D', 'enable_unittest=0'])
        break



  mac_dir = options.mac_dir or '../mac'
  if not os.path.isabs(mac_dir):
    mac_dir = os.path.join('<(DEPTH)', mac_dir)
  command_line.extend(['-D', 'mac_dir=%s' % mac_dir])

  # Check the version and determine if the building version is dev-channel or
  # not. Note that if --channel_dev is explicitly set, we don't check the
  # version number.
  if options.channel_dev is None:
    options.channel_dev = version.IsDevChannel()
  if options.channel_dev:
    command_line.extend(['-D', 'channel_dev=1'])

  def SetCommandLineForFeature(option_name, windows=False, mac=False,
                               linux=False, chromeos=False, android=False,
                               nacl=False):
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
      chromeos: A boolean which replesents the default value of the target
          option on ChromeOS platform.
      android: A boolean which replesents the default value of the target
          option on Android platform.
      nacl: A boolean which replesents the default value of the target
          option on NaCl.

    Raises:
      ValueError: An error occurred when 'option_name' is empty.
    """
    if not option_name:
      raise ValueError('"option_name" should be specified')

    default_enabled = False
    default_enabled = {'Windows': windows,
                       'Mac': mac,
                       'Linux': linux,
                       'ChromeOS': chromeos,
                       'Android': android,
                       'NaCl': nacl}[options.target_platform]
    enable_option_name = 'enable_%s' % option_name
    enabled = options.ensure_value(enable_option_name, default_enabled)
    command_line.extend(['-D',
                         '%s=%s' % (enable_option_name, 1 if enabled else 0)])

  is_official = ((options.language == 'japanese') and
                 (options.branding == 'GoogleJapaneseInput'))
  is_official_dev = (is_official and options.channel_dev)

  SetCommandLineForFeature(option_name='webservice_infolist')
  SetCommandLineForFeature(option_name='cloud_sync',
                           linux=is_official_dev,
                           windows=is_official_dev,
                           mac=is_official_dev)
  SetCommandLineForFeature(option_name='cloud_handwriting',
                           linux=is_official_dev,
                           windows=is_official_dev,
                           mac=is_official_dev)
  SetCommandLineForFeature(option_name='http_client',
                           linux=is_official,
                           windows=is_official,
                           mac=is_official,
                           chromeos=False,  # not supported.
                           android=is_official,
                           # System dictionary is read with HttpClient in NaCl.
                           nacl=True)
  SetCommandLineForFeature(option_name='ambiguous_search',
                           android=True)
  SetCommandLineForFeature(option_name='typing_correction',
                           android=True)
  SetCommandLineForFeature(option_name='history_deletion',
                           linux=is_official_dev,
                           windows=is_official_dev,
                           mac=is_official_dev)

  command_line.extend(['-D', 'target_platform=%s' % options.target_platform])

  if IsWindows():
    command_line.extend(['-G', 'msvs_version=%s' % options.msvs_version])

  # On Windows, we need to determine if <qtdir>/lib/Qt*.lib are import
  # libraries for Qt DLLs or statically-linked libraries. Here we assume that
  # Qt*.lib are import libraries when <qtdir>/lib/QtCore.prl contains
  # 'QT_SHARED' in the definition of 'QMAKE_PRL_DEFINES'.
  use_dynamically_linked_qt = False
  if IsWindows() and not options.noqt and options.qtdir:
    qtcore_prl_path = os.path.join(
        os.path.abspath(options.qtdir), 'lib', 'QtCore.prl')
    with open(qtcore_prl_path) as qtcore_prl_file:
      for line in qtcore_prl_file:
        if ('QMAKE_PRL_DEFINES' in line) and ('QT_SHARED' in line):
          use_dynamically_linked_qt = True
          break

  if use_dynamically_linked_qt:
    command_line.extend(['-D', 'use_dynamically_linked_qt=1'])
  else:
    command_line.extend(['-D', 'use_dynamically_linked_qt=0'])

  if options.use_zinnia:
    command_line.extend(['-D', 'use_zinnia=YES'])
  else:
    command_line.extend(['-D', 'use_zinnia=NO'])

  if ((options.target_platform == 'Linux' or
       options.target_platform == 'ChromeOS') and
      '%s/unix/ibus/ibus.gyp' % SRC_DIR in gyp_file_names):
    command_line.extend(['-D', 'use_libibus=1'])


  # Generate make rules for Android NDK.
  if options.target_platform == 'Android':
    command_line.extend(['-G', 'android_ndk_version=r8'])

  # Dictionary configuration
  if options.target_platform == 'ChromeOS':
    # Note that the OSS version of mozc ignores the dictionary variable.
    command_line.extend(['-D', 'dictionary=small'])
    command_line.extend(['-D', 'use_separate_collocation_data=0'])
    command_line.extend(['-D', 'use_separate_connection_data=0'])
    command_line.extend(['-D', 'use_separate_dictionary=0'])
    command_line.extend(['-D', 'use_1byte_cost_for_connection_data=0'])
    command_line.extend(['-D', 'use_packed_dictionary=0'])
  elif options.target_platform == 'Android':
    command_line.extend(['-D', 'dictionary=small'])
    command_line.extend(['-D', 'use_separate_collocation_data=0'])
    command_line.extend(['-D', 'use_separate_connection_data=1'])
    command_line.extend(['-D', 'use_separate_dictionary=1'])
    command_line.extend(['-D', 'use_1byte_cost_for_connection_data=1'])
    command_line.extend(['-D', 'use_packed_dictionary=0'])
  elif options.target_platform == 'NaCl':
    command_line.extend(['-D', 'dictionary=desktop'])
    command_line.extend(['-D', 'use_separate_collocation_data=0'])
    command_line.extend(['-D', 'use_separate_connection_data=0'])
    command_line.extend(['-D', 'use_separate_dictionary=0'])
    command_line.extend(['-D', 'use_1byte_cost_for_connection_data=0'])
    command_line.extend(['-D', 'use_packed_dictionary=1'])
  else:
    command_line.extend(['-D', 'dictionary=desktop'])
    command_line.extend(['-D', 'use_separate_collocation_data=0'])
    command_line.extend(['-D', 'use_separate_connection_data=0'])
    command_line.extend(['-D', 'use_separate_dictionary=0'])
    command_line.extend(['-D', 'use_1byte_cost_for_connection_data=0'])
    command_line.extend(['-D', 'use_packed_dictionary=0'])

  # Specifying pkg-config command.  Some environment (such like
  # cross-platform ChromeOS build) requires us to call a different
  # command for pkg-config.  Here we catch the environment variable
  # and use the specified command instead of actual pkg-config
  # command.
  if IsLinux():
    command_line.extend(['-D', 'pkg_config_command=%s' % GetPkgConfigCommand()])
  else:
    command_line.extend(['-D', 'pkg_config_command='])

  if options.target_platform == 'NaCl':
    if os.path.isdir(options.nacl_sdk_root):
      nacl_sdk_root = os.path.abspath(options.nacl_sdk_root)
    else:
      PrintErrorAndExit('The directory specified with --nacl_sdk_root does not '
                        'exist: %s' % options.nacl_sdk_root)
    command_line.extend(['-D', 'nacl_sdk_root=%s' % nacl_sdk_root])
    command_line.extend(['-D', 'clang=1'])

  command_line.extend(['-D', 'language=%s' % options.language])

  command_line.extend([
      '-D', 'server_dir=%s' % os.path.abspath(options.server_dir)])

  # Run GYP.
  logging.info('Running GYP...')
  RunOrDie(command_line)

  # Restore PYTHONPATH just in case.
  if not original_python_path:
    os.environ['PYTHONPATH'] = original_python_path

  # Done!
  logging.info('Done')


  # When Windows build is configured to use DLL version of Qt, copy Qt's DLLs
  # and debug symbols into Mozc's build directory. This is important because:
  # - We can easily back up all the artifacts if relevant product binaries and
  #   debug symbols are placed at the same place.
  # - Some post-build tools such as bind.exe can easily look up the dependent
  #   DLLs (QtCore4.dll and QtQui4.dll in this case).
  # Perhaps the following code can also be implemented in gyp, but we atopt
  # this ad hock workaround as a first step.
  # TODO(yukawa): Implement the following logic in gyp, if magically possible.
  if use_dynamically_linked_qt:
    abs_qtdir = os.path.abspath(options.qtdir)
    abs_qt_bin_dir = os.path.join(abs_qtdir, 'bin')
    abs_qt_lib_dir = os.path.join(abs_qtdir, 'lib')
    abs_out_win_dir = GetBuildBaseName(options,
                                       GetMozcVersion().GetTargetPlatform())
    copy_script = os.path.join(
        GetBuildScriptDirectoryName(), 'build_tools', 'copy_dll_and_symbol.py')
    copy_modes = [
        {'configuration': 'DebugDynamic', 'basenames': 'QtCored4;QtGuid4'},
        {'configuration': 'OptimizeDynamic', 'basenames': 'QtCore4;QtGui4'},
        {'configuration': 'ReleaseDynamic', 'basenames': 'QtCore4;QtGui4'}]
    for mode in copy_modes:
      copy_commands = [
          copy_script,
          '--basenames', mode['basenames'],
          '--dll_paths', abs_qt_bin_dir,
          '--pdb_paths', os.pathsep.join([abs_qt_bin_dir, abs_qt_lib_dir]),
          '--target_dir', os.path.join(abs_out_win_dir, mode['configuration']),
      ]
      RunOrDie(copy_commands)


def BuildNinja():
  """Builds ninja.exe on the fly for Windows."""
  build_ninja_bat = os.path.join(
      GetBuildScriptDirectoryName(), 'tools', 'build_ninja.bat')
  RunOrDie(['cmd.exe', '/C', build_ninja_bat])


def GetNinjaDir():
  """Returns the directory to Ninja."""
  return os.path.abspath(os.path.join(
      GetTopLevelSourceDirectoryName(),
      'third_party',
      'ninja',
      'files'))


def BuildToolsMain(options, unused_args, original_directory_name):
  """The main function for 'build_tools' command."""
  # Make sure Ninja.exe is ready to be used on Windows.
  if IsWindows():
    BuildNinja()

  build_tools_dir = os.path.join(GetRelPath(os.getcwd(),
                                            original_directory_name),
                                 SRC_DIR, 'build_tools')
  # Build targets in this order.
  if IsWindows():
    build_tools_targets = [
        'out/%s:primitive_tools' % options.configuration,
        'out/%s:build_tools' % options.configuration,
    ]
  else:
    gyp_files = [
        os.path.join(build_tools_dir, 'primitive_tools', 'primitive_tools.gyp'),
        os.path.join(build_tools_dir, 'build_tools.gyp')
    ]
    build_tools_targets = []
    for gyp_file in gyp_files:
      (target, _) = os.path.splitext(os.path.basename(gyp_file))
      build_tools_targets += ['%s:%s' % (gyp_file, target)]

  for build_tools_target in build_tools_targets:
    BuildMain(options, [build_tools_target], original_directory_name)


def CanonicalTargetToGypFileAndTargetName(target):
  """Parses the target string."""
  if not ':' in target:
    PrintErrorAndExit('Invalid target: %s' % target)
  (gyp_file_name, target_name) = target.split(':')
  return (gyp_file_name, target_name)


def BuildOnLinux(options, targets, unused_original_directory_name):
  """Build the targets on Linux."""
  target_names = []
  for target in targets:
    (unused_gyp_file_name, target_name) = (
        CanonicalTargetToGypFileAndTargetName(target))
    target_names.append(target_name)

  make_command = os.getenv('BUILD_COMMAND', 'make')
  # flags for building in Chrome OS chroot environment
  envvars = [
      'CFLAGS',
      'CXXFLAGS',
      'CXX',
      'CC',
      'AR',
      'AS',
      'RANLIB',
      'LD',
  ]
  for envvar in envvars:
    if envvar in os.environ:
      os.environ[envvar] = os.getenv(envvar)

  build_args = ['-j%s' % options.jobs,
                'MAKE_JOBS=%s' % options.jobs,
                'BUILDTYPE=%s' % options.configuration]
  if hasattr(options, 'android_device'):
    # Only for android testing.
    build_args.append('ANDROID_DEVICES=%s' % options.android_device)

  build_args.append('builddir_name=%s' %
                    GetBuildBaseName(options,
                                     GetMozcVersion().GetTargetPlatform()))

  RunOrDie([make_command] + build_args + target_names)


def BuildOnMac(options, targets, original_directory_name):
  """Build the targets on Mac."""
  original_directory_relpath = GetRelPath(original_directory_name, os.getcwd())
  for target in targets:
    (gyp_file_name, target_name) = CanonicalTargetToGypFileAndTargetName(target)
    gyp_file_name = os.path.join(original_directory_relpath, gyp_file_name)
    CheckFileOrDie(gyp_file_name)
    (xcode_base_name, _) = os.path.splitext(gyp_file_name)
    RunOrDie(['xcodebuild',
              '-project', '%s.xcodeproj' % xcode_base_name,
              '-configuration', options.configuration,
              '-target', target_name,
              '-parallelizeTargets',
              'BUILD_WITH_GYP=1'])


def BuildOnWindows(targets):
  """Build the target on Windows."""

  ninja = 'ninja.exe'


  for target in targets:
    tokens = target.split(':')
    target_dir = tokens[0]
    tokens.pop(0)
    RunOrDie([ninja, '-C', target_dir] + tokens)


def BuildMain(options, targets, original_directory_name):
  """The main function for the 'build' command."""
  if not targets:
    PrintErrorAndExit('No build target is specified.')

  # Add the mozc root directory to PYTHONPATH.
  original_python_path = os.environ.get('PYTHONPATH', '')
  mozc_root = os.path.abspath(GetTopLevelSourceDirectoryName())
  python_path = os.pathsep.join([original_python_path, mozc_root])
  os.environ['PYTHONPATH'] = python_path


  if IsMac():
    BuildOnMac(options, targets, original_directory_name)
  elif IsLinux():
    BuildOnLinux(options, targets, original_directory_name)
  elif IsWindows():
    BuildOnWindows(targets)
  else:
    logging.error('Unsupported platform: %s', os.name)
    return

  RunPackageVerifiers(
      os.path.join(GetBuildBaseName(options,
                                    GetMozcVersion().GetTargetPlatform()),
                   options.configuration))

  # Revert python path.
  os.environ['PYTHONPATH'] = original_python_path


def RunTests(build_base, configuration, parallel_num):
  """Run built tests actually.

  Args:
    build_base: the base directory ('out_linux', 'out_mac', etc.)
    configuration: build configuration ('Release' or 'Debug')
    parallel_num: allows specified jobs at once.

  Raises:
    RunOrDieError: One or more tests have failed.
  """
  #TODO(nona): move this function to build_tools/test_tools
  base_path = os.path.join(build_base, configuration)

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
  if IsWindows():
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
        binary_filename = os.path.basename(binary)
        tmp_xml_path = os.path.join(gtest_report_dir,
                                    '%s.xml.running' % binary_filename)
        test_options = options[:]
        test_options.extend(['--gunit_output=xml:%s' % tmp_xml_path,
                             '--gtest_output=xml:%s' % tmp_xml_path])
        RunOrDie([binary] + test_options)
        xml_path = os.path.join(gtest_report_dir, '%s.xml' % binary_filename)
        CopyFile(tmp_xml_path, xml_path)
        RemoveFile(tmp_xml_path)
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


def RunTestsOnAndroid(options, targets, build_args, original_directory_name):
  """Run a test suite for the Android version."""
  if targets:
    PrintErrorAndExit('Targets [%s] are not supported.' % ', '.join(targets))

  # Build native and java tests once.
  # The builds must be in sequence (build, wait and next build).
  # Otherwise build artifacts seem to conflict.
  android_gyp = os.path.join(SRC_DIR, 'android', 'android.gyp')
  for target in [':build_native_small_test', ':build_java_test']:
    (build_options, build_targets) = ParseBuildOptions(
        build_args + [android_gyp + target])
    BuildMain(build_options, build_targets, original_directory_name)

  try:
    emulators = []
    serialnumbers = []
    if options.android_device:
      serialnumbers.append(options.android_device)
    else:
      # Set up the environment.
      # Copy ANDROID_SDK_HOME directory.
      # Copied directory will be overwritten by StartAndroidEmulator
      # (inflating compressed file and being added emulator's temp files).
      # Thus this setup must be done only once.
      android_sdk_home_original = os.path.join(
          SRC_DIR, 'android', 'android_sdk_home')
      android_sdk_home = os.path.join(
          GetBuildBaseName(options, GetMozcVersion().GetTargetPlatform()),
          options.configuration,
          'android_sdk_home')
      android_util.SetUpTestingSdkHomeDirectory(android_sdk_home_original,
                                                android_sdk_home)
      available_ports = android_util.GetAvailableEmulatorPorts()
      emulators = []
      avd_names = android_util.GetAvdNames(android_sdk_home)
      logging.info('Launching following AVDs; %s', avd_names)
      if len(available_ports) < len(avd_names):
        PrintErrorAndExit('available_ports (%d) is smaller than avd_names (%d)'
                          % (len(available_ports), len(avd_names)))
      # Invokes all the available emulators.
      for avd_name in avd_names:
        emulator = Emulator(android_sdk_home, avd_name)
        logging.info('Creating SD card for %s', avd_name)
        emulator.CreateBlankSdCard(300 * 1024 * 1024)
        logging.info('Launching %s at port %d', avd_name, available_ports[0])
        emulator.Launch(available_ports[0])
        logging.info('Waiting for %s', avd_name)
        emulator.GetAndroidDevice().WaitForDevice()
        available_ports = available_ports[1:]
        emulators.append(emulator)
        serialnumbers.append(emulator.serial)
        logging.info('Successfully launched %s', avd_name)

    # Run native tests.
    android_gyp = os.path.join(SRC_DIR, 'android', 'android.gyp')
    (build_options, build_targets) = ParseBuildOptions(
        build_args + [android_gyp + ':run_native_small_test'])
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
    BuildMain(build_options, build_targets, original_directory_name)

    # Run the test suite in Java.
    (build_options, build_targets) = ParseBuildOptions(
        build_args + [android_gyp + ':run_test'])
    BuildMain(build_options, build_targets, original_directory_name)
  finally:
    # Terminate the emulators.
    for emulator in emulators:
      emulator.Terminate()


def RunTestsOnNaCl(targets, build_args, original_directory_name):
  """Run a test suite for the NaCl version."""
  # Currently we can only run the limited test set which is defined as
  # nacl_test_targets in nacl_extension.gyp.
  if targets:
    PrintErrorAndExit('Targets [%s] are not supported.' % ', '.join(targets))
  nacl_gyp = os.path.join(SRC_DIR, 'chrome', 'nacl', 'nacl_extension.gyp')
  (build_options, build_targets) = ParseBuildOptions(
      build_args + [nacl_gyp + ':run_nacl_test'])
  # Run the test suite in NaCl.
  BuildMain(build_options, build_targets, original_directory_name)


def RunTestsMain(options, args, original_directory_name):
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

  # configuration and build_base flags are shared among runtests options and
  # build options.
  if 'jobs' in vars(options).keys():
    build_options.extend(['-j', options.jobs])
  if options.configuration:
    build_options.extend(['-c', options.configuration])
  if options.build_base:
    build_options.extend(['--build_base', options.build_base])

  target_platform = GetMozcVersion().GetTargetPlatform()
  if target_platform == 'Android':
    return RunTestsOnAndroid(options, targets, build_options,
                             original_directory_name)
  if target_platform == 'NaCl':
    return RunTestsOnNaCl(targets, build_options, original_directory_name)

  if not targets:
    # TODO(yukawa): Change the notation rule of 'targets' to reduce the gap
    # between Ninja and make.
    if target_platform == 'Windows':
      targets.append('out/%s:unittests' % options.configuration)
    else:
      targets.append('%s/gyp/tests.gyp:unittests' % SRC_DIR)

  # Build the test targets
  (build_opts, build_args) = ParseBuildOptions(build_options + targets)
  BuildMain(build_opts, build_args, original_directory_name)

  # Run tests actually
  RunTests(GetBuildBaseName(options, target_platform), options.configuration,
           options.test_jobs)


def CleanBuildFilesAndDirectories(options, unused_args):
  """Cleans build files and directories."""
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
      for build_type in ['Debug', 'Optimize', 'Release']:
        directory_names.append(os.path.join(gyp_directory_name,
                                            build_type))
    elif IsMac():
      directory_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                                    '*.xcodeproj')))
    elif IsLinux():
      file_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                               '*.target.mk')))
      file_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                               '*/*.target.mk')))
      file_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                               '*.host.mk')))
      file_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                               '*/*.host.mk')))
      file_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                               '*.Makefile')))
      file_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                               '*/*.Makefile')))
  file_names.append('%s/mozc_version.txt' % SRC_DIR)
  file_names.append('third_party/jsoncpp/jsoncpp.gyp')
  file_names.append('third_party/zlib/v1_2_3/zlib.gyp')
  # Collect stuff in the top-level directory.
  directory_names.append('mozc_build_tools')

  target_platform = GetBuildBaseName(options,
                                     GetMozcVersion().GetTargetPlatform())
  if target_platform:
    directory_names.append(target_platform)
  if IsLinux():
    file_names.append('Makefile')
    # Remove auto-generated files.
    file_names.append(os.path.join(SRC_DIR, 'android', 'AndroidManifest.xml'))
    directory_names.append(os.path.join(SRC_DIR, 'android', 'assets'))
    # Delete files/dirs generated by Android SDK/NDK.
    android_library_projects = [
        '',
        'protobuf',
        'resources_oss',
        'tests',
        ]
    android_generated_dirs = ['bin', 'gen', 'obj', 'libs', 'gen_for_adt']
    for project in android_library_projects:
      for directory in android_generated_dirs:
        directory_names.append(
            os.path.join(SRC_DIR, 'android', project, directory))

    # In addition, remove resources/res/raw directory, which contains
    # generated .pic files.
    directory_names.append(
        os.path.join(SRC_DIR, 'android', 'resources', 'res', 'raw'))

  elif IsWindows():
    file_names.append('third_party/breakpad/breakpad.gyp')
  # Remove files.
  for file_name in file_names:
    RemoveFile(file_name)
  # Remove directories.
  for directory_name in directory_names:
    RemoveDirectoryRecursively(directory_name)


def CleanMain(options, args):
  """The main function for the 'clean' command."""
  CleanBuildFilesAndDirectories(options, args)


def ShowHelpAndExit():
  """Shows the help message."""
  print 'Usage: build_mozc.py COMMAND [ARGS]'
  print 'Commands: '
  print '  gyp          Generate project files.'
  print '  build        Build the specified target.'
  print '  build_tools  Build tools used by the build command.'
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

  # Remember the original current directory name.
  original_directory_name = os.getcwd()
  # Move to the top level source directory only once since os.chdir
  # affects functions in os.path and that causes troublesome errors.
  MoveToTopLevelSourceDirectory()

  command = sys.argv[1]
  args = sys.argv[2:]
  if command == 'gyp':
    (cmd_opts, cmd_args) = ParseGypOptions(args)
    GypMain(cmd_opts, cmd_args)
  elif command == 'build_tools':
    (cmd_opts, cmd_args) = ParseBuildOptions(args)
    BuildToolsMain(cmd_opts, cmd_args, original_directory_name)
  elif command == 'build':
    (cmd_opts, cmd_args) = ParseBuildOptions(args)
    BuildMain(cmd_opts, cmd_args, original_directory_name)
  elif command == 'runtests':
    (cmd_opts, cmd_args) = ParseRunTestsOptions(args)
    RunTestsMain(cmd_opts, cmd_args, original_directory_name)
  elif command == 'clean':
    (cmd_opts, cmd_args) = ParseCleanOptions(args)
    CleanMain(cmd_opts, cmd_args)
  else:
    logging.error('Unknown command: %s', command)
    ShowHelpAndExit()

if __name__ == '__main__':
  main()
