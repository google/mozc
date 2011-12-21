#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright 2010-2011, Google Inc.
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
import shutil
import stat
import subprocess
import sys

from build_tools import mozc_version


SRC_DIR = '.'

sys.path.append(SRC_DIR)


def IsWindows():
  """Returns true if the platform is Windows."""
  return os.name == 'nt'


def IsMac():
  """Returns true if the platform is Mac."""
  return os.name == 'posix' and os.uname()[0] == 'Darwin'


def IsLinux():
  """Returns true if the platform is Linux."""
  return os.name == 'posix' and os.uname()[0] == 'Linux'


# TODO(yukawa): Move this function to util.py (b/2715400)
def GetNumberOfProcessors():
  """Returns the number of processors available.

  Returns:
    An integer corresponding to the number of processors available in
    Windows, Mac, and Linux.  In other platforms, returns 1.
  """
  if IsWindows():
    return int(os.environ['NUMBER_OF_PROCESSORS'])
  elif IsMac():
    commands = ['sysctl', '-n', 'hw.ncpu']
    process = subprocess.Popen(commands, stdout=subprocess.PIPE)
    return int(process.communicate()[0])
  elif IsLinux():
    # Count the number of 'vendor_id' in /proc/cpuinfo, assuming that
    # each line corresponds to one logical CPU.
    cpuinfo = open('/proc/cpuinfo', 'r')
    count = len([line for line in cpuinfo if line.find('vendor_id') != -1])
    cpuinfo.close()
    return count
  else:
    return 1


def GetBuildBaseName(options):
  """Returns the build base directory."""
  if options.build_base:
    return options.build_base
  # For some reason, xcodebuild does not accept absolute path names for
  # the -project parameter. Convert the original_directory_name to a
  # relative path from the build top level directory.
  build_base = ''
  if IsMac():
    build_base = os.path.join(os.getcwd(), 'out_mac')
  elif IsWindows():
    build_base = os.path.join(os.getcwd(), 'out_win')
  elif IsLinux():
    # On Linux, seems there is no way to specify the build_base directory
    # inside common.gypi
    build_base = 'out_linux'
  else:
    logging.error('Unsupported platform: %s', os.name)

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


def GenerateVersionFile(version_template_path, version_path):
  """Reads the version template file and stores it into version_path.

  This doesn't update the "version_path" if nothing will be changed to
  reduce unnecessary build caused by file timestamp.

  Args:
    version_template_path: a file name which contains the template of version.
    version_path: a file name to be stored the official version.
  """
  version = mozc_version.MozcVersion(version_template_path, expand_daily=True)
  version_definition = version.GetVersionInFormat(
      'MAJOR=@MAJOR@\n'
      'MINOR=@MINOR@\n'
      'BUILD=@BUILD@\n'
      'REVISION=@REVISION@\n'
      'FLAG=@FLAG@\n')
  old_content = ''
  if os.path.exists(version_path):
    # if the target file already exists, need to check the necessity of update.
    old_content = open(version_path).read()

  if version_definition != old_content:
    open(version_path, 'w').write(version_definition)




def GetVersionFileNames(options):
  """Gets the (template of version file, version file) pair."""
  template_path = '%s/%s' % (SRC_DIR, options.version_file)
  version_path = '%s/mozc_version.txt' % SRC_DIR
  return (template_path, version_path)


def GetGypFileNames(options):
  """Gets the list of gyp file names."""
  gyp_file_names = []
  mozc_top_level_names = glob.glob('%s/*' % SRC_DIR)

  if not options.language == 'japanese':
    language_path = '%s/languages/%s' % (SRC_DIR, options.language)
    if not os.path.exists(language_path):
      print >>sys.stderr, 'Can not find language directory: %s ' % language_path
      sys.exit(1)
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
  gyp_file_names.append(
      '%s/dictionary/file/dictionary_file.gyp' % SRC_DIR)
  gyp_file_names.append(
      '%s/dictionary/system/system_dictionary.gyp' % SRC_DIR)
  # Include subdirectory of win32 and breakpad for Windows
  if IsWindows():
    gyp_file_names.extend(glob.glob('%s/win32/*/*.gyp' % SRC_DIR))
    gyp_file_names.extend(glob.glob('third_party/breakpad/*.gyp'))
  elif IsLinux():
    gyp_file_names.extend(glob.glob('%s/unix/*/*.gyp' % SRC_DIR))
    # Add ibus.gyp if ibus is installed.
    # Ubuntu 8.04 (Hardy) does not contain ibus package.
    try:
      RunOrDie([GetPkgConfigCommand(), '--exists', 'ibus-1.0'])
    except RunOrDieError:
      print 'ibus-1.0 was not found with pkg-config.'
      gyp_file_names.remove('%s/unix/ibus/ibus.gyp' % SRC_DIR)
    # Check if Qt libraries are installed.
    try:
      RunOrDie([GetPkgConfigCommand(), '--exists', 'QtCore', 'QtGui'])
      qt_found = True
    except RunOrDieError:
      print 'QtCore or QtGui was not found with pkg-config.'
      qt_found = False
    if options.ensure_value('qtdir', None):
      qt_found = True
    if not qt_found:
      # Fall back to --noqt mode.  Ensure options.noqt is available and
      # force it to be True.
      options.ensure_value('noqt', True)
      options.noqt = True
    # Add scim.gyp if SCIM is installed.
    try:
      RunOrDie([GetPkgConfigCommand(), '--exists', 'scim'])
    except RunOrDieError:
      print 'scim was not found with pkg-config.'
      gyp_file_names.remove('%s/unix/scim/scim.gyp' % SRC_DIR)
    # Add chrome skk gyp scripts.
    gyp_file_names.append('%s/chrome/skk/skk.gyp' % SRC_DIR)
    gyp_file_names.append('%s/chrome/skk/skk_dict.gyp' % SRC_DIR)
    gyp_file_names.append('%s/chrome/skk/skk_util_test.gyp' % SRC_DIR)
  gyp_file_names.extend(glob.glob('third_party/rx/*.gyp'))
  gyp_file_names.extend(glob.glob('third_party/jsoncpp/*.gyp'))
  gyp_file_names.sort()
  return gyp_file_names


def RemoveFile(file_name):
  """Removes the specified file."""
  if not os.path.isfile(file_name):
    return  # Do nothing if not exist.
  if IsWindows():
    # Read-only files cannot be deleted on Windows.
    os.chmod(file_name, 0700)
  print 'Removing file: %s' % file_name
  os.unlink(file_name)


def CopyFile(source, destination):
  """Copies a file to the destination. Remove an old version if needed."""
  if os.path.isfile(destination):  # Remove the old one if exists.
    RemoveFile(destination)
  dest_dirname = os.path.dirname(destination)
  if not os.path.isdir(dest_dirname):
    os.makedirs(dest_dirname)
  print 'Copying file to: %s' % destination
  shutil.copy(source, destination)


def RemoveDirectoryRecursively(directory):
  """Removes the specified directory recursively."""
  if os.path.isdir(directory):
    print 'Removing directory: %s' % directory
    if IsWindows():
      # Use RD because shutil.rmtree fails when the directory is readonly.
      RunOrDie(['CMD.exe', '/C', 'RD', '/S', '/Q',
                os.path.normpath(directory)])
    else:
      shutil.rmtree(directory, ignore_errors=True)


def MakeFileWritableRecursively(path):
  """Make files (including directories) writable recursively."""
  for root, dirs, files in os.walk(path):
    for name in dirs + files:
      path = os.path.join(root, name)
      os.chmod(path, os.lstat(path).st_mode | stat.S_IWRITE)


def GetTopLevelSourceDirectoryName():
  """Gets the top level source directory name."""
  if SRC_DIR == '.':
    return SRC_DIR
  script_file_directory_name = os.path.dirname(sys.argv[0])
  num_components = len(SRC_DIR.split('/'))
  return os.path.join(script_file_directory_name, *(['..'] * num_components))


def MoveToTopLevelSourceDirectory():
  """Moves to the build top level directory."""
  os.chdir(GetTopLevelSourceDirectoryName())


def GetGypSvnUrl(deps_file_name):
  """Get the GYP SVN URL from DEPS file."""
  contents = file(deps_file_name).read()
  match = re.search(r'"(http://gyp\.googlecode\.com.*?)@', contents)
  if match:
    base_url = match.group(1)
    match = re.search(r'"gyp_revision":\s+"(\d+)"', contents)
    if match:
      revision = match.group(1)
      return '%s@%s' % (base_url, revision)
  else:
    PrintErrorAndExit('GYP URL not found in %s:' % deps_file_name)


class RunOrDieError(StandardError):
  """The exception class for RunOrDie."""

  def __init__(self, message):
    StandardError.__init__(self, message)


def RunOrDie(argv):
  """Run the command, or die if it failed."""
  # Rest are the target program name and the parameters, but we special
  # case if the target program name ends with '.py'
  if argv[0].endswith('.py'):
    argv.insert(0, sys.executable)  # Inject the python interpreter path.
  # We don't capture stdout and stderr from Popen. The output will just
  # be emitted to a terminal or console.
  print 'Running: ' + ' '.join(argv)
  process = subprocess.Popen(argv)

  if process.wait() != 0:
    raise RunOrDieError('\n'.join(['',
                                   '==========',
                                   ' ERROR: %s' % ' '.join(argv),
                                   '==========']))


def PrintErrorAndExit(error_message):
  """Prints the error message and exists."""
  print '\n==========\n ERROR: %s\n==========\n' % error_message
  sys.exit(1)


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
  # Linux environment can build both for Linux and ChromeOS.
  # This option enable this script to know which build (Linux or ChromeOS)
  # should be done. If you want ChromeOS build, specify "ChromeOS".
  parser.add_option('--target_platform', dest='target_platform', default='',
                    help='If you want ChromeOS build, specify "ChromeOS"')
  parser.add_option('--language', dest='language', default='japanese',
                    help='Specify the target language to build.')
  return parser


def GetDefaultWixPath():
  """Returns the default Wix directory.."""
  abs_path = ''
  return abs_path


def ParseGypOptions(args=None, values=None):
  """Parses command line options for the gyp command."""
  parser = optparse.OptionParser(usage='Usage: %prog gyp [options]')
  AddCommonOptions(parser)
  # DEPS file should exist in the same directory of the script.
  default_deps_file = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]),
                                                   'DEPS'))
  parser.add_option('--deps_file', dest='deps_file', default=default_deps_file,
                    help='Specifies the DEPS file.')
  parser.add_option('--gyp_generator', dest='gyp_generator',
                    help='Specifies the generator for GYP.')
  parser.add_option('--deprecated_onepass', '-1', dest='onepass',
                    action='store_true',
                    default=False,
                    help='Not supported. Builds mozc in one pass.')
  parser.add_option('--branding', dest='branding', default='Mozc')
  parser.add_option('--gypdir', dest='gypdir', default='third_party/gyp')
  parser.add_option('--noqt', action='store_true', dest='noqt', default=False)
  parser.add_option('--qtdir', dest='qtdir',
                    default=os.getenv('QTDIR', None),
                    help='Qt base directory to be used.')
  parser.add_option('--coverage', action='store_true', dest='coverage',
                    help='use code coverage analysis build options',
                    default=False)
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

  parser.add_option('--mac_dir', dest='mac_dir',
                    help='A path to the root directory of third party '
                    'libraries for Mac build which will be passed to gyp '
                    'files.')

  parser.add_option('--nacl_sdk_root', dest='nacl_sdk_root', default='',
                    help='A path to the root directory of Native Client SDK. '
                    'This is used when NaCl module build.')

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


def ExpandMetaTarget(meta_target_name, target_platform):
  """Returns a list of build targets with expanding meta target name.

  If the specified name is 'package', returns a list of build targets for
  building production package.

  If the specified name is not a meta target name, returns it as a list.

  Args:
    meta_target_name: meta target name to be expanded.
    target_platform: The target platform.

  Returns:
    A list of build targets with meta target names expanded.
  """
  if meta_target_name != 'package':
    return [meta_target_name]

  if target_platform == 'ChromeOS':
    targets = ['%s/unix/ibus/ibus.gyp:ibus_mozc',
               '%s/server/server.gyp:mozc_server',
               '%s/gui/gui.gyp:mozc_tool']
  elif IsLinux():
    targets = ['%s/unix/ibus/ibus.gyp:ibus_mozc',
               '%s/server/server.gyp:mozc_server',
               '%s/gui/gui.gyp:mozc_tool']
  elif IsMac():
    targets = ['%s/mac/mac.gyp:DiskImage']
  elif IsWindows():
    targets = ['%s/win32/build32/build32.gyp:',
               '%s/win32/build64/build64.gyp:']
    targets += ['%s/win32/installer/installer.gyp:']
  return [(target % SRC_DIR) for target in targets]


def ParseBuildOptions(args=None, values=None):
  """Parses command line options for the build command."""
  parser = optparse.OptionParser(usage='Usage: %prog build [options]')
  AddCommonOptions(parser)
  parser.add_option('--jobs', '-j', dest='jobs', default='4', metavar='N',
                    help='run jobs in parallel')
  parser.add_option('--configuration', '-c', dest='configuration',
                    default='Debug', help='specify the build configuration.')
  parser.add_option('--version_file', dest='version_file',
                    help='use the specified version template file',
                    default='mozc_version_template.txt')

  (options, args) = parser.parse_args(args, values)
  targets = []
  for arg in args:
    targets.extend(ExpandMetaTarget(arg, options.target_platform))
  return (options, targets)


def ParseRunTestsOptions(args=None, values=None):
  """Parses command line options for the runtests command."""
  parser = optparse.OptionParser(
      usage='Usage: %prog runtests [options] [test_targets] [-- build options]')
  AddCommonOptions(parser)
  parser.add_option('--test_size', dest='test_size', default='small')
  parser.add_option('--calculate_coverage', dest='calculate_coverage',
                    default=False, action='store_true',
                    help='specify if you want to calculate test coverage.')
  parser.add_option('--configuration', '-c', dest='configuration',
                    default='Debug', help='specify the build configuration.')

  return parser.parse_args(args, values)


def ParseCleanOptions(args=None, values=None):
  """Parses command line options for the clean command."""
  parser = optparse.OptionParser(
      usage='Usage: %prog clean [-- build options]')
  AddCommonOptions(parser)

  return parser.parse_args(args, values)


def GypMain(options, unused_args):
  """The main function for the 'gyp' command."""
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
  print 'Build tool: %s' % generator

  # Get and show the list of .gyp file names.
  gyp_file_names = GetGypFileNames(options)
  print 'GYP files:'
  for file_name in gyp_file_names:
    print '- %s' % file_name
  # We use the one in third_party/gyp
  gyp_script = os.path.join(options.gypdir, 'gyp')
  # If we don't have a copy of gyp, download it.
  if not os.path.isfile(gyp_script):
    # SVN creates mozc_build_tools directory if it's not present.
    gyp_svn_url = GetGypSvnUrl(options.deps_file)
    RunOrDie(['svn', 'checkout', gyp_svn_url, options.gypdir])
  # Run GYP.
  print 'Running GYP...'
  command_line = [sys.executable, gyp_script,
                  '--depth=.',
                  '--include=%s/gyp/common.gypi' % SRC_DIR]
  command_line.extend(['-D', 'python_executable=%s' % sys.executable])

  if options.onepass:
    command_line.extend(['-D', 'two_pass_build=0'])
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
  if options.coverage:
    command_line.extend(['-D', 'coverage=1'])

  if IsWindows() and options.wix_dir:
    command_line.extend(['-D', 'use_wix=YES'])
    command_line.extend(['-D', 'wix_dir=%s' % options.wix_dir])
  else:
    command_line.extend(['-D', 'use_wix=NO'])

  command_line.extend(['-D', 'build_base=%s' % GetBuildBaseName(options)])



  mac_dir = options.mac_dir or '../mac'
  if not os.path.isabs(mac_dir):
    mac_dir = os.path.join('<(DEPTH)', mac_dir)
  command_line.extend(['-D', 'mac_dir=%s' % mac_dir])

  # Check the version and determine if the building version is dev-channel or
  # not. Note that if --channel_dev is explicitly set, we don't check the
  # version number.
  (template_path, unused_version_path) = GetVersionFileNames(options)
  version = mozc_version.MozcVersion(template_path, expand_daily=False)
  if options.channel_dev is None:
    options.channel_dev = version.IsDevChannel()
  if options.channel_dev:
    command_line.extend(['-D', 'channel_dev=1'])

  command_line.extend(['-D', 'target_platform=%s' % options.target_platform])

  if IsWindows():
    command_line.extend(['-G', 'msvs_version=%s' % options.msvs_version])


  # Dictionary configuration
  if options.target_platform == 'ChromeOS':
    # Note that the OSS version of mozc ignores the dictionary variable.
    command_line.extend(['-D', 'dictionary=small'])
  else:
    command_line.extend(['-D', 'dictionary=desktop'])

  # Specifying pkg-config command.  Some environment (such like
  # cross-platform ChromeOS build) requires us to call a different
  # command for pkg-config.  Here we catch the environment variable
  # and use the specified command instead of actual pkg-config
  # command.
  if IsLinux():
    command_line.extend(['-D', 'pkg_config_command=%s' % GetPkgConfigCommand()])
  else:
    command_line.extend(['-D', 'pkg_config_command='])

  if os.path.isdir(options.nacl_sdk_root):
    nacl_sdk_root = os.path.abspath(options.nacl_sdk_root)
  elif options.nacl_sdk_root:
    PrintErrorAndExit('The directory specified with --nacl_sdk_root does not '
                      'exist: %s' % options.nacl_sdk_root)
  else:
    nacl_sdk_root = ''
  command_line.extend(['-D', 'nacl_sdk_root=%s' % nacl_sdk_root])

  command_line.extend(['-D', 'language=%s' % options.language])
  command_line.extend([
      '-D', 'language_define=LANGUAGE_%s' % options.language.upper()])

  # Add options.gypdir/pylib to PYTHONPATH so gyp uses its own library modules,
  # otherwise gyp can use ones of a different version.
  original_python_path = os.environ.get('PYTHONPATH', '')
  os.environ['PYTHONPATH'] = (os.path.abspath(os.path.join(options.gypdir,
                                                           'pylib')) +
                              os.pathsep + original_python_path)
  RunOrDie(command_line)
  os.environ['PYTHONPATH'] = original_python_path


  # Done!
  print 'Done'


def GetRelPath(path, start):
  """Return a relative path to |path| from |start|."""
  # NOTE: Python 2.6 provides os.path.relpath, which has almost the same
  # functionality as this function. Since Python 2.6 is not the internal
  # official version, we reimplement it.
  path_list = os.path.abspath(os.path.normpath(path)).split(os.sep)
  start_list = os.path.abspath(os.path.normpath(start)).split(os.sep)

  common_prefix_count = 0
  for i in range(0, min(len(path_list), len(start_list))):
    if path_list[i] != start_list[i]:
      break
    common_prefix_count += 1

  return os.sep.join(['..'] * (len(start_list) - common_prefix_count) +
                     path_list[common_prefix_count:])


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
    PrintErrorAndExit('Invalid target: ' + target)
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
  build_args.append('builddir_name=%s' % GetBuildBaseName(options))

  RunOrDie([make_command] + build_args + target_names)


def CheckFileOrDie(file_name):
  """Check the file exists or dies if not."""
  if not os.path.isfile(file_name):
    PrintErrorAndExit('No such file: ' + file_name)


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
  """Locate the directory where vcbuild.exe exists.

  Returns:
    A string of absolute directory path where vcbuild.exe exists.
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

  # TODO(yukawa): Support Visual C++ 2010
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

  build_concurrency = GetNumberOfProcessors()

  RunOrDie(['msbuild',
            '/m:%d' % build_concurrency,  # Use concurrent build
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

  # Generate a version definition file.
  print 'Generating version definition file...'
  (template_path, version_path) = GetVersionFileNames(options)
  GenerateVersionFile(template_path, version_path)


  if IsMac():
    BuildOnMac(options, targets, original_directory_name)
  elif IsLinux():
    BuildOnLinux(options, targets, original_directory_name)
  elif IsWindows():
    BuildOnWindows(options, targets, original_directory_name)
  else:
    print 'Unsupported platform: ', os.name
    return

  RunPackageVerifiers(
      os.path.join(GetBuildBaseName(options), options.configuration))


def RunTests(build_base, configuration, unused_calculate_coverage):
  """Run built tests actually.

  Args:
    build_base: the base directory ('out_linux', 'out_mac', etc.)
    configuration: build configuration ('Release' or 'Debug')
    unused_calculate_coverage: True if runtests calculates the test coverage.

  Raises:
    RunOrDieError: One or more tests have failed.
  """
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
  for binary in test_binaries:
    logging.info('running %s...', binary)
    try:
      RunOrDie([binary] + options)
    except RunOrDieError, e:
      print e
      failed_tests.append(binary)
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
  if options.configuration:
    build_options.extend(['-c', options.configuration])
  if options.build_base:
    build_options.extend(['--build_base', options.build_base])
  if options.target_platform:
    build_options.extend(['--target_platform', options.target_platform])


  if not targets:
    targets.append('%s/gyp/tests.gyp:unittests' % SRC_DIR)

  # Build the test targets
  (build_opts, build_args) = ParseBuildOptions(build_options + targets)
  BuildMain(build_opts, build_args, original_directory_name)

  # Run tests actually
  RunTests(GetBuildBaseName(options), options.configuration,
           options.calculate_coverage)


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
                                               '*.Makefile')))
      file_names.extend(glob.glob(os.path.join(gyp_directory_name,
                                               '*/*.Makefile')))
  file_names.append('%s/mozc_version.txt' % SRC_DIR)
  file_names.append('third_party/rx/rx.gyp')
  file_names.append('third_party/jsoncpp/jsoncpp.gyp')
  # Collect stuff in the top-level directory.
  directory_names.append('mozc_build_tools')

  directory_names.append(GetBuildBaseName(options))
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
    print 'Unknown command: ' + command
    ShowHelpAndExit()

if __name__ == '__main__':
  main()
