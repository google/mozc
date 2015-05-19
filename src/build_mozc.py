#!/usr/bin/env python
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

"""Script building Mozc.

Typical usage:

  % python build_mozc.py gyp
  % python build_mozc.py build_tools -c Release
  % python build_mozc.py build base/base.gyp:base
"""

__author__ = "komatsu"

import glob
import inspect
import logging
import optparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading

from build_tools import mozc_version
from build_tools.test_tools import task_scheduler

from build_tools.util import CheckFileOrDie
from build_tools.util import CopyFile
from build_tools.util import GetNumberOfProcessors
from build_tools.util import GetRelPath
from build_tools.util import IsLinux
from build_tools.util import IsMac
from build_tools.util import IsWindows
from build_tools.util import MakeFileWritableRecursively
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
TERMINAL_COLORS = {
    'CLEAR': '\x1b[0m',
    'BLACK': '\x1b[30m',
    'RED': '\x1b[31m',
    'GREEN': '\x1b[32m',
    'YELLOW': '\x1b[33m',
    'BLUE': '\x1b[34m',
    'MAGENTA': '\x1b[35m',
    'CYAN': '\x1b[36m',
    'WHITE': '\x1b[37m',
}

sys.path.append(SRC_DIR)

def GetMozcVersion():
  """Returns MozcVersion instance."""
  # TODO(matsuzakit): Caching might be better.
  return mozc_version.MozcVersion('%s/mozc_version.txt' % SRC_DIR)


def GetColoredText(text, color):
  """Gets colored text for terminal."""
  # TODO(hsumita): Considers dumb terminal.
  # Disables on windows because cmd.exe doesn't support ANSI color escape
  # sequences.
  # TODO(team): Considers to use ctypes.windll.kernel32.SetConsoleTextAttribute
  #             on Windows. b/6260694
  if not IsWindows() and sys.stdout.isatty() and sys.stderr.isatty():
    return TERMINAL_COLORS[color] + text + TERMINAL_COLORS['CLEAR']
  return text


def GenerateMessageForLogging(message, tag, color, with_line_number=False,
                              stack_frame_number=2):
  """Generates a logging message."""
  output = [GetColoredText(tag, color), str(message)]
  if with_line_number:
    stack_frame = inspect.stack(stack_frame_number)[stack_frame_number]
    line_number_index = 2
    output.append('[build_mozc.py:%d]' % stack_frame[line_number_index])
  return ' '.join(output)


def PrintError(message, *args, **kwargs):
  """Prints a error message."""
  output = GenerateMessageForLogging(message, 'ERROR:', 'RED', True)
  logging.error(output, *args, **kwargs)


def PrintErrorAndExit(message, *args, **kwargs):
  """Prints the error message and exists."""
  output = GenerateMessageForLogging(message, 'ERROR:', 'RED', True)
  logging.error('==========')
  logging.error(output, *args, **kwargs)
  logging.error('==========')

  sys.exit(1)


def PrintWarning(message, *args, **kwargs):
  """Prints a warning message."""
  output = GenerateMessageForLogging(message, 'WARNING:', 'YELLOW', True)
  logging.warning(output, *args, **kwargs)


def PrintInfo(message, *args, **kwargs):
  """Prints a information message."""
  output = GenerateMessageForLogging(message, 'INFO:', 'CYAN')
  logging.info(output, *args, **kwargs)


def PrintTestSuccess(message, *args):
  """Prints the results of a successed test."""
  output = GenerateMessageForLogging(message, '[  PASSED  ]', 'GREEN')
  print output % args


def PrintTestFailure(message, *args):
  """Prints the results of a failed test."""
  output = GenerateMessageForLogging(message, '[  FAILED  ]', 'RED')
  print output % args


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

  if target_platform == 'Windows':
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
    PrintError('Unsupported platform: %s', target_platform)

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
    return 'msvs'
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
    PrintInfo('%s failed' % ' '.join(command_line))
  return False


def GenerateVersionFile(version_template_path, version_path, target_platform,
                        is_channel_dev):
  """Reads the version template file and stores it into version_path.

  This doesn't update the "version_path" if nothing will be changed to
  reduce unnecessary build caused by file timestamp.

  Args:
    version_template_path: a file name which contains the template of version.
    version_path: a file name to be stored the official version.
    target_platform: target platform name. c.f. --target_platform option
    is_channel_dev: True if dev channel. False if stable channel.
      None if you want to use template file's configuration.
  """
  version_format = ('MAJOR=@MAJOR@\n'
                    'MINOR=@MINOR@\n'
                    'BUILD=@BUILD@\n'
                    'REVISION=@REVISION@\n'
                    'ANDROID_VERSION_CODE=@ANDROID_VERSION_CODE@\n'
                    'FLAG=@FLAG@\n'
                    'TARGET_PLATFORM=@TARGET_PLATFORM@\n')
  mozc_version.GenerateVersionFileFromTemplate(version_template_path,
                                               version_path,
                                               version_format,
                                               target_platform=target_platform,
                                               is_channel_dev=is_channel_dev)




def GetGypFileNames(options):
  """Gets the list of gyp file names."""
  gyp_file_names = []
  mozc_top_level_names = glob.glob('%s/*' % SRC_DIR)

  if not options.language == 'japanese':
    language_path = '%s/languages/%s' % (SRC_DIR, options.language)
    if not os.path.exists(language_path):
      PrintErrorAndExit('Can not find language directory: %s ', language_path)
    mozc_top_level_names.append(language_path)
  # Exclude the gyp directory where we have special gyp files like
  # breakpad.gyp that we should exclude.
  mozc_top_level_names = [x for x in mozc_top_level_names if
                          os.path.basename(x) != 'gyp']
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
  if IsWindows():
    gyp_file_names.extend(glob.glob('%s/win32/*/*.gyp' % SRC_DIR))
    gyp_file_names.extend(glob.glob('third_party/breakpad/*.gyp'))
  elif IsLinux():
    gyp_file_names.extend(glob.glob('%s/unix/*/*.gyp' % SRC_DIR))
    # Add ibus.gyp if ibus is installed.
    # Ubuntu 8.04 (Hardy) does not contain ibus package.
    if not PkgExists('ibus-1.0'):
      PrintInfo('removing ibus.gyp.')
      gyp_file_names.remove('%s/unix/ibus/ibus.gyp' % SRC_DIR)
    # Check if Qt libraries are installed.
    qt_found = PkgExists('QtCore', 'QtGui')
    if options.ensure_value('qtdir', None):
      qt_found = True
    if not qt_found:
      # Fall back to --noqt mode.  Ensure options.noqt is available and
      # force it to be True.
      options.ensure_value('noqt', True)
      options.noqt = True
    # Add scim.gyp if SCIM is installed.
    if not PkgExists('scim'):
      PrintInfo('removing scim.gyp.')
      gyp_file_names.remove('%s/unix/scim/scim.gyp' % SRC_DIR)
  if options.target_platform == 'NaCl':
    # Add chrome skk gyp scripts.
    gyp_file_names.append('%s/chrome/skk/skk.gyp' % SRC_DIR)
    gyp_file_names.append('%s/chrome/skk/skk_dict.gyp' % SRC_DIR)
    gyp_file_names.append('%s/chrome/skk/skk_util_test.gyp' % SRC_DIR)
  gyp_file_names.extend(glob.glob('third_party/rx/*.gyp'))
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


def RunPackageVerifiers(build_dir):
  """Runs script to verify created packages/binaries.

  Args:
    build_dir: the directory where build results are.
  """
  binary_size_checker = os.path.join('build_tools', 'binary_size_checker.py')
  RunOrDie([binary_size_checker, '--target_directory', build_dir])


def AddCommonOptions(parser):
  """Adds the options common among the commands."""
  parser.add_option('--build_base', dest='build_base',
                    help='Specifies the base directory of the built binaries.')
  parser.add_option('--language', dest='language', default='japanese',
                    help='Specify the target language to build.')
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
  parser.add_option('--gyp_generator', dest='gyp_generator',
                    help='Specifies the generator for GYP.')
  parser.add_option('--branding', dest='branding', default='Mozc')
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

  parser.add_option('--nacl_sdk_root', dest='nacl_sdk_root', default='',
                    help='A path to the root directory of Native Client SDK. '
                    'This is used when NaCl module build.')

  # On Linux, you should not set this flag if you want to use "dlopen" to
  # load Mozc's modules. See
  # - http://code.google.com/p/mozc/issues/detail?id=14
  # - http://code.google.com/p/protobuf/issues/detail?id=128
  # - http://code.google.com/p/protobuf/issues/detail?id=370
  # for the background information.
  parser.add_option('--use_libprotobuf', action='store_true',
                    dest='use_libprotobuf', default=False,
                    help='Use libprotobuf on GNU/Linux. On other platforms, '
                    'this flag will simply be ignored and Mozc always links '
                    'to protobuf statically.')

  use_dynamically_linked_qt_default = True
  parser.add_option('--use_dynamically_linked_qt',
                    dest='use_dynamically_linked_qt',
                    default=use_dynamically_linked_qt_default,
                    help='Use dynamically linked version of Qt. '
                    'Currently this flag is used only on Windows builds.')

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
  AddFeatureOption(parser, feature_name='gtk renderer',
                   macro_name='ENABLE_GTK_RENDERER',
                   option_name='gtk_renderer')
  AddFeatureOption(parser, feature_name='cloud sync',
                   macro_name='ENABLE_CLOUD_SYNC',
                   option_name='cloud_sync')
  AddFeatureOption(parser, feature_name='cloud handwriting',
                   macro_name='ENABLE_CLOUD_HANDWRITING',
                   option_name='cloud_handwriting')
  AddFeatureOption(parser, feature_name='http client',
                   macro_name='MOZC_ENABLE_HTTP_CLIENT',
                   option_name='http_client')

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
    # for Visual Studio 2008, which is a default compiler for Mozc.  However,
    # you can specify the target version explicitly via 'msvs_version' option
    # as follows.
    parser.add_option('--msvs_version', dest='msvs_version', default='2008',
                      help='Specifies the target MSVS version.')

  return parser.parse_args(args, values)


def ExpandMetaTarget(meta_target_name):
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
    targets = ['%s/android/android.gyp:apk']
  elif target_platform == 'Linux':
    targets = ['%s/unix/ibus/ibus.gyp:ibus_mozc',
               '%s/server/server.gyp:mozc_server',
               '%s/gui/gui.gyp:mozc_tool']
  elif target_platform == 'Mac':
    targets = ['%s/mac/mac.gyp:DiskImage']
  elif target_platform == 'Windows':
    targets = ['%s/win32/build32/build32.gyp:',
               '%s/win32/build64/build64.gyp:']
    targets += ['%s/win32/installer/installer.gyp:']
  return [(target % SRC_DIR) for target in targets]


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
    targets.extend(ExpandMetaTarget(arg))
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
  #TODO(nona): enable parallel testing on continuous build
  parser.add_option('--test_jobs', dest='test_jobs', type='int',
                    default=1,
                    metavar='N', help='run test jobs in parallel')
  parser.add_option('--test_size', dest='test_size', default='small')
  parser.add_option('--configuration', '-c', dest='configuration',
                    default='Debug', help='specify the build configuration.')

  return parser.parse_args(args, values)


def ParseCleanOptions(args=None, values=None):
  """Parses command line options for the clean command."""
  parser = optparse.OptionParser(
      usage='Usage: %prog clean [-- build options]')
  AddCommonOptions(parser)
  AddTargetPlatformOption(parser)

  return parser.parse_args(args, values)


def GypMain(options, unused_args):
  """The main function for the 'gyp' command."""
  # Generate a version definition file.
  # The version file is used in this method to check if this is dev channel.
  PrintInfo('Generating version definition file...')
  template_path = '%s/%s' % (SRC_DIR, options.version_file)
  version_path = '%s/mozc_version.txt' % SRC_DIR
  GenerateVersionFile(template_path, version_path, options.target_platform,
                      options.channel_dev)
  version = GetMozcVersion()
  PrintInfo('Version string is %s' % version.GetVersionString())

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
    mozc_dir = os.path.abspath(GetTopLevelSourceDirectoryName())
    # Following script tests if 'import gyp' is now available and its
    # location is expected one. By using this script, make sure
    # 'import gyp' actually loads 'third_party/gyp/pylib/gyp'.
    gyp_check_script = os.path.join(
        mozc_dir, 'build_tools', 'ensure_gyp_module_path.py')
    expected_gyp_module_path = os.path.join(gyp_dir, 'pylib', 'gyp')
    RunOrDie([sys.executable, gyp_check_script,
              '--expected=%s' % expected_gyp_module_path])

  # Copy rx.gyp and jsoncpp.gyp to the third party directory.
  CopyFile('%s/gyp/rx.gyp' % SRC_DIR,
           'third_party/rx/rx.gyp')
  CopyFile('%s/gyp/jsoncpp.gyp' % SRC_DIR,
           'third_party/jsoncpp/jsoncpp.gyp')
  # Copy breakpad.gyp to the third party directory, if necessary.
  if IsWindows():
    CopyFile('%s/gyp/breakpad.gyp' % SRC_DIR,
             'third_party/breakpad/breakpad.gyp')

  # Determine the generator name.
  generator = GetGeneratorName(options.gyp_generator)
  os.environ['GYP_GENERATORS'] = generator
  PrintInfo('Build tool: %s', generator)

  # Get and show the list of .gyp file names.
  gyp_file_names = GetGypFileNames(options)
  PrintInfo('GYP files:')
  for file_name in gyp_file_names:
    PrintInfo('- %s', file_name)

  # Build GYP command line.
  PrintInfo('Building GYP command line...')
  command_line = [sys.executable, gyp_command,
                  '--depth=.',
                  '--include=%s/gyp/common.gypi' % SRC_DIR]
  command_line.extend(['-D', 'python_executable=%s' % sys.executable])

  command_line.extend(gyp_file_names)

  if options.branding:
    command_line.extend(['-D', 'branding=%s' % options.branding])
  if options.noqt:
    command_line.extend(['-D', 'use_qt=NO'])
  else:
    command_line.extend(['-D', 'use_qt=YES'])
  if options.qtdir:
    command_line.extend(['-D', 'qt_dir=%s' % os.path.abspath(options.qtdir)])
  else:
    command_line.extend(['-D', 'qt_dir='])

  if IsWindows() and options.wix_dir:
    command_line.extend(['-D', 'use_wix=YES'])
    command_line.extend(['-D', 'wix_dir=%s' % options.wix_dir])
  else:
    command_line.extend(['-D', 'use_wix=NO'])

  command_line.extend(['-D', 'build_base=%s' %
                       GetBuildBaseName(options, version.GetTargetPlatform())])

  disable_unittest_if_not_available = True
  if disable_unittest_if_not_available:
    required_modules = ['gmock', 'gtest']
    for module in required_modules:
      module_dir = os.path.abspath(
          os.path.join(GetTopLevelSourceDirectoryName(), 'third_party', module))
      if not os.path.exists(module_dir):
        PrintWarning('%s not found. Disabling unittest.', module)
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

  enable_gtk_renderer_by_default = False
  SetCommandLineForFeature(option_name='gtk_renderer',
                           linux=enable_gtk_renderer_by_default)
  SetCommandLineForFeature(option_name='webservice_infolist')
  SetCommandLineForFeature(option_name='cloud_sync',
                           linux=is_official_dev,
                           windows=is_official_dev,
                           mac=is_official_dev)
  SetCommandLineForFeature(option_name='cloud_handwriting',
                           linux=is_official_dev,
                           windows=is_official_dev,
                           mac=is_official_dev)
  enable_http_client_by_default = is_official
  SetCommandLineForFeature(option_name='http_client',
                           linux=enable_http_client_by_default,
                           windows=enable_http_client_by_default,
                           mac=enable_http_client_by_default,
                           chromeos=False,  # not supported.
                           android=enable_http_client_by_default)

  command_line.extend(['-D', 'target_platform=%s' % options.target_platform])

  if IsWindows():
    command_line.extend(['-G', 'msvs_version=%s' % options.msvs_version])

  if options.use_dynamically_linked_qt:
    command_line.extend(['-D', 'use_dynamically_linked_qt=YES'])
  else:
    command_line.extend(['-D', 'use_dynamically_linked_qt=NO'])

  if options.use_zinnia:
    command_line.extend(['-D', 'use_zinnia=YES'])
  else:
    command_line.extend(['-D', 'use_zinnia=NO'])

  if IsLinux():
    if '%s/unix/ibus/ibus.gyp' % SRC_DIR in gyp_file_names:
      command_line.extend(['-D', 'use_libibus=1'])
    if '%s/unix/scim/scim.gyp' % SRC_DIR in gyp_file_names:
      command_line.extend(['-D', 'use_libscim=1'])
  if options.use_libprotobuf:
    command_line.extend(['-D', 'use_libprotobuf=1'])


  # Dictionary configuration
  if options.target_platform == 'ChromeOS':
    # Note that the OSS version of mozc ignores the dictionary variable.
    command_line.extend(['-D', 'dictionary=small'])
    command_line.extend(['-D', 'use_separate_connection_data=0'])
    command_line.extend(['-D', 'use_separate_dictionary=0'])
  else:
    command_line.extend(['-D', 'dictionary=desktop'])
    command_line.extend(['-D', 'use_separate_connection_data=0'])
    command_line.extend(['-D', 'use_separate_dictionary=0'])

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
    elif options.nacl_sdk_root:
      PrintErrorAndExit('The directory specified with --nacl_sdk_root does not '
                      'exist: %s', options.nacl_sdk_root)
    command_line.extend(['-D', 'nacl_sdk_root=%s' % nacl_sdk_root])

  command_line.extend(['-D', 'language=%s' % options.language])

  command_line.extend([
      '-D', 'server_dir=%s' % os.path.abspath(options.server_dir)])

  # Run GYP.
  PrintInfo('Running GYP...')
  RunOrDie(command_line)

  # Restore PYTHONPATH just in case.
  if not original_python_path:
    os.environ['PYTHONPATH'] = original_python_path


  # Done!
  PrintInfo('Done')


def BuildToolsMain(options, unused_args, original_directory_name):
  """The main function for 'build_tools' command."""
  build_tools_dir = os.path.join(GetRelPath(os.getcwd(),
                                            original_directory_name),
                                 SRC_DIR, 'build_tools')
  # Build targets in this order.
  gyp_files = [
      os.path.join(build_tools_dir, 'primitive_tools', 'primitive_tools.gyp'),
      os.path.join(build_tools_dir, 'build_tools.gyp')
      ]

  for gyp_file in gyp_files:
    (target, _) = os.path.splitext(os.path.basename(gyp_file))
    BuildMain(options, ['%s:%s' % (gyp_file, target)], original_directory_name)


def CanonicalTargetToGypFileAndTargetName(target):
  """Parses the target string."""
  if not ':' in target:
    PrintErrorAndExit('Invalid target: %s', target)
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


def LocateMSBuildDir():
  """Locate the directory where msbuild.exe exists.

  Returns:
    A string of absolute directory path where msbuild.exe exists.
  """
  if not IsWindows():
    PrintErrorAndExit('msbuild.exe is not supported on this platform')

  msbuild_path = os.path.join(os.getenv('windir'), 'Microsoft.NET',
                              'Framework', 'v4.0.30319')

  if os.path.exists(os.path.join(msbuild_path, 'msbuild.exe')):
    return os.path.abspath(msbuild_path)

  PrintErrorAndExit('Failed to locate msbuild.exe')


def LocateVCBuildDir():
  """Locate the directory where vcbuild.exe exists.

  Returns:
    A string of absolute directory path where vcbuild.exe exists.
  """
  if not IsWindows():
    PrintErrorAndExit('vcbuild.exe is not supported on this platform')


  program_files_x86 = ''
  if os.getenv('ProgramFiles(x86)'):
    program_files_x86 = os.getenv('ProgramFiles(x86)')
  elif os.getenv('ProgramFiles'):
    program_files_x86 = os.getenv('ProgramFiles')

  if not program_files_x86:
    PrintErrorAndExit('Failed to locate vcbuild.exe')

  vcbuild_path = os.path.join(program_files_x86, 'Microsoft Visual Studio 9.0',
                              'VC', 'vcpackages')

  if os.path.exists(os.path.join(vcbuild_path, 'vcbuild.exe')):
    return os.path.abspath(vcbuild_path)

  PrintErrorAndExit('Failed to locate vcbuild.exe')




def BuildOnWindowsVS2008(abs_solution_path, platform, configuration):
  """Build the targets on Windows with VS2008."""
  abs_command_dir = LocateVCBuildDir()
  command = 'vcbuild.exe'

  CheckFileOrDie(os.path.join(abs_command_dir, command))

  original_path = os.getenv('PATH')

  if original_path:
    os.environ['PATH'] = os.pathsep.join([abs_command_dir, original_path])
  else:
    os.environ['PATH'] = abs_command_dir

  build_concurrency = GetNumberOfProcessors()

  RunOrDie(['vcbuild',
            '/M%d' % build_concurrency,  # Use concurrent build
            '/time',    # Show build time
            '/platform:%s' % platform,
            abs_solution_path,
            '%s|%s' % (configuration, platform)])

  os.environ['PATH'] = original_path


def BuildOnWindowsVS2010(abs_solution_path, platform, configuration):
  """Build the targets on Windows with VS2010."""
  abs_command_dir = LocateMSBuildDir()
  command = 'msbuild.exe'

  CheckFileOrDie(os.path.join(abs_command_dir, command))

  original_path = os.getenv('PATH')

  if original_path:
    os.environ['PATH'] = os.pathsep.join([abs_command_dir, original_path])
  else:
    os.environ['PATH'] = abs_command_dir

  RunOrDie(['msbuild',
            '/m',  # enable concurrent build
            '/property:Platform=%s' % platform,
            '/property:Configuration=%s' % configuration,
            abs_solution_path])

  os.environ['PATH'] = original_path


def BuildOnWindows(options, targets, original_directory_name):
  """Build the target on Windows."""

  for target in targets:
    # TODO(yukawa): target name is currently ignored.
    (gyp_file_name, _) = CanonicalTargetToGypFileAndTargetName(target)
    gyp_file_name = os.path.join(original_directory_name, gyp_file_name)
    CheckFileOrDie(gyp_file_name)
    (base_sln_path, _) = os.path.splitext(gyp_file_name)
    abs_sln_path = os.path.abspath('%s.sln' % base_sln_path)
    base_sln_file_name = os.path.basename(abs_sln_path)

    platform = 'Win32'
    # We are using very ad-hoc way to determine which target platform
    # should be needed for the given target.  If and only if the target
    # solution file name is 'build64.sln', invoke vcbuild to build x64
    # binaries.
    if base_sln_file_name.lower() == 'build64.sln':
      platform = 'x64'

    line = open(abs_sln_path).readline().rstrip('\n')
    if line.endswith('Format Version 11.00'):
      BuildOnWindowsVS2010(abs_sln_path, platform, options.configuration)
    elif line.endswith('Format Version 10.00'):
      BuildOnWindowsVS2008(abs_sln_path, platform, options.configuration)


def BuildMain(options, targets, original_directory_name):
  """The main function for the 'build' command."""
  if not targets:
    PrintErrorAndExit('No build target is specified.')

  # Add the mozc root directory to PYTHONPATH.
  original_python_path = os.environ.get('PYTHONPATH', '')
  mozc_root = os.path.abspath(GetTopLevelSourceDirectoryName())
  python_path = os.pathsep.join([original_python_path, mozc_root])
  os.environ['PYTHONPATH'] = python_path

  target_platform = GetMozcVersion().GetTargetPlatform()

  if IsMac():
    BuildOnMac(options, targets, original_directory_name)
  elif IsLinux():
    BuildOnLinux(options, targets, original_directory_name)
  elif IsWindows():
    BuildOnWindows(options, targets, original_directory_name)
  else:
    PrintError('Unsupported platform: %s', os.name)
    return

  RunPackageVerifiers(
      os.path.join(GetBuildBaseName(options, target_platform),
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

  failed_tests = []
  if parallel_num == 1:
    for binary in test_binaries:
      PrintInfo('running %s...', binary)
      try:
        RunOrDie([binary] + options)
      except RunOrDieError, e:
        PrintError(e)
        failed_tests.append(binary)
  else:
    scheduler = task_scheduler.TaskScheduler()
    lock = threading.RLock()

    def OnFail(cmd, msg, unused_return_code, unused_user_data):
      # TODO(nona): using xml reports
      with lock:
        failed_tests.append(' '.join(cmd))
        # Prints bare output of test.
        print msg
        PrintTestFailure(' '.join(cmd))

    def OnSuccess(cmd, msg, user_data):
      # Even return code represents suceess, if there is no xml output file, it
      # suppose to exit unexpected way.
      with lock:
        if not os.path.isfile(user_data['xmlpath']):
          failed_tests.append(' '.join(cmd))
          # Prints bare output of test.
          print msg
          PrintError('XML file does not exists. test process might crash.')
          PrintTestFailure(' '.join(cmd))
          return
        PrintTestSuccess(cmd[0])

    def OnFinish(unused_cmd, unused_msg, user_data):
      try:
        shutil.rmtree(user_data['tmp_dir'])
      except OSError:
        PrintWarning('Failed to clean up temporary directory.')

    for binary in test_binaries:
      tmp_dir = tempfile.mkdtemp()
      xml_path = os.path.join(tmp_dir, '%s.xml' % binary)
      additional_options = ['--test_tmpdir=%s' % tmp_dir,
                            '--gunit_output=xml:%s' % xml_path,
                            '--gtest_output=xml:%s' % xml_path]
      scheduler.AddTask([binary] + options + additional_options, OnSuccess,
                        OnFail, OnFinish,
                        {'xmlpath': xml_path, 'tmp_dir': tmp_dir})
    scheduler.Execute(parallel_num)
  if failed_tests:
    raise RunOrDieError('\n'.join(['following tests failed'] + failed_tests))




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

  if not targets:
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
      for pattern in ['*.ncb', '*.rules', '*.sln', '*.suo', '*.vcproj',
                      '*.vcproj.*.user', '*.vcxproj', '*.vcxproj.filters',
                      '*.vcxproj.user']:
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
  file_names.append('third_party/rx/rx.gyp')
  file_names.append('third_party/jsoncpp/jsoncpp.gyp')
  # Collect stuff in the top-level directory.
  directory_names.append('mozc_build_tools')

  target_platform = GetBuildBaseName(options,
                                     GetMozcVersion().GetTargetPlatform())
  if target_platform:
    directory_names.append(target_platform)
  if IsLinux():
    file_names.append('Makefile')
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
  logging.basicConfig(level=logging.INFO, format='%(message)s')

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
    PrintError('Unknown command: %s', command)
    ShowHelpAndExit()

if __name__ == '__main__':
  main()
