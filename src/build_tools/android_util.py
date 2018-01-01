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

"""Utility for Android.

Internally this depends on 'aapt' and 'jarsigner' commands so if they are not
on the path an exception will raise.

TODO(matsuzakit): Split to smaller modules (APK, AndroidDevice and Emulator).
"""

__author__ = "matsuzakit"

import fcntl
import glob
import logging
import os
import re
import subprocess
import sys
import zipfile


# Directory to store lock files.
LOCK_DIR = '/tmp/.android_util'


class Error(Exception):
  """Base exception class for all android_util errors."""


class AndroidUtilIOError(Error, IOError):
  """Domain specific IOError."""


class AndroidUtilSubprocessError(Error):
  """Subrocess which is invoked by this script throws error."""


class AndroidEmulatorConditionError(Error):
  """Emulator relating error."""


def GetApkProperties(apk_path):
  """Gets the properties of an apk.

  Args:
    apk_path: The path to the apk file as a string.

  Returns:
    Following properties will be in the returned dict.
    apk_file_name (string): Full path of the apk file.
    apk_size (int): File size of the apk file.
    classdex_{raw,compressed}_size (int):
        {Raw,Compressed} file size of classes.dex.
    has_release_key (boolean): True if the apk is signed with release key.
    libmozc_{raw,compressed}_size: {Raw,Compressed} file size of libmozc.so.
    native_code (list of string): Included native code's ABIs.
    res__file_count (int): Count of res/**/* files
    res_image_file__count (int): Count of res/**/*.{png,jpg,gif} files.
    res_image_{raw,compressed}_size_total (int):
        Total {raw,compressed} file size of res/**/*.{png,jpg,gif} files.
    res_{raw,compressed}_size_total (int):
        Total {raw,compressed} file size of res/**/* files.
    res_xml_file__count (int): Count of res/**/*.xml files.
    res_xml_{raw,compressed}_size_total (int):
        Total {raw,compressed} file size of res/**/*.xml files.
    uses_permissions (array of string): List of uses_permission.
    version_name (string): Version name.
    version_code (int): Version code.

  Raises:
    AndroidUtilIOError: If apk_file does not point to a file.
  """
  if not os.path.isfile(apk_path):
    raise AndroidUtilIOError('File not found: %s' % apk_path)
  apk_path = os.path.abspath(apk_path)

  props = {}
  props['apk_file_name'] = apk_path
  props['apk_size'] = os.path.getsize(apk_path)

  zip_file = zipfile.ZipFile(apk_path)
  try:
    _CollectMultipleFileMetrics(
        zip_file, r'lib/[^/]+/libmozc.so', 'libmozc', props)
    _CollectSingleFileMetrics(
        zip_file, r'classes.dex', 'classdex', props)
    _CollectMultipleFileMetrics(
        zip_file, r'res/.*', 'res', props)
    _CollectMultipleFileMetrics(
        zip_file, r'res/.*\.(png|jpg|gif)', 'res_image', props)
    _CollectMultipleFileMetrics(
        zip_file, r'res/.*\.xml', 'res_xml', props)
  finally:
    zip_file.close()

  _CollectBadgingProperties(apk_path, props)
  _CollectSignatureProperties(apk_path, props)

  return props


def _CollectBadgingProperties(apk_path, props):
  """Collects badging properties via aapt command.

  This method fills below properties;
  'native_code': Included native code's ABIs.
  'uses_permissions': Used permissions.
  'version_code': Version code of the apk file.
  'version_name': Version name of the apk file.

  Args:
    apk_path: Absolute path to apk file.
    props: A dictionary to be updated (property name to the value).
  """
  args = ['aapt', 'dump', 'badging', apk_path]
  logging.info('Collecting badging props; %s', args)
  (output, _) = subprocess.Popen(args, stdout=subprocess.PIPE).communicate()
  logging.debug(output)
  native_code_match = re.search(r'^native-code: (.*)$', output, re.MULTILINE)
  if native_code_match:
    native_code = native_code_match.group(1)
    props['native_code'] = native_code.replace('\'', '').split(' ')
  else:
    props['native_code'] = []
  props['uses_permissions'] = re.findall(r'^uses-permission:\'(.*)\'$',
                                         output, re.MULTILINE)
  props['version_code'] = int(re.search(r'versionCode=\'(\d+)\'',
                                        output).group(1))
  props['version_name'] = re.search(
      r'versionName=\'(\d+\.\d+\.\d+\.\d+(?:-\w+)?)\'', output).group(1)


def _CollectSignatureProperties(apk_path, props):
  """Collects signature relating properties via jarsigner command.

  This method fills below property;
  'has_release_key': True if the apk is signed with release key.

  Args:
    apk_path: Absolute path to apk file.
    props: A dictionary to be updated (property name to the value).
  """
  certificate = (r'X.509, CN=Unknown, OU="Google, Inc", O="Google, Inc", '
                 r'L=Mountain View, ST=CA, C=US')
  args = ['jarsigner', '-verify', '-verbose', '-certs', apk_path]
  logging.info('Collecting signature; %s', args)
  (output, _) = subprocess.Popen(args, stdout=subprocess.PIPE).communicate()
  logging.debug(output)
  props['has_release_key'] = output.find(certificate) != -1


def _CollectSingleFileMetrics(zip_file, name_pattern, key, props):
  """Collects metrics for given file.

  This method fills below properties;
  '%s_raw_size': Given file's raw (uncompressed) size.
  '%s_compressed_size': Given file's compressed size.

  Args:
    zip_file: A ZipFile in which target file exists.
    name_pattern: A regex pattern of the file names (str object).
      This pattern must match only one file.
    key: Key name of the retuned properties.
    props: A dictionary to be updated (property name to the value).

  Raises:
    AndroidUtilIOError: Given name_pattern matches nothing or multiple files.
  """
  info_list = _GetZipInfoList(zip_file, name_pattern)
  if len(info_list) != 1:
    raise AndroidUtilIOError('Given name_pattern (%s) must match '
                             'exactly one file but matched %d files.' %
                             (name_pattern, len(info_list)))
  info = info_list[0]
  props['%s_raw_size' % key] = info.file_size
  props['%s_compressed_size' % key] = info.compress_size


def _CollectMultipleFileMetrics(zip_file, name_pattern, key, props):
  """Collects metrics for given files.

  This method fills below propertes;
  '%s_file_count': Number of matched files.
  '%s_raw_size_total': Total raw size of matched files.
  '%s_compressed_size_total': Total compressed size of matched files.

  Args:
    zip_file: A ZipFile in which target file exists.
    name_pattern: A regex pattern of the file names (str object).
    key: Key name of the retuned properties.
    props: A dictionary (property name to the value).
  """
  info_list = _GetZipInfoList(zip_file, name_pattern)
  props['%s_file_count' % key] = len(info_list)
  props['%s_raw_size_total' % key] = sum(info.file_size for info in info_list)
  props['%s_compressed_size_total' % key] = sum(
      info.compress_size for info in info_list)


def _GetZipInfoList(zip_file, name_pattern):
  """Gets ZipInfo objects from zip_file.

  Args:
    zip_file: A ZipFile to be scaned.
    name_pattern: A regex pattern of the file names (str object).
  Returns:
    ZipInfo objects.
  """
  name_regex = re.compile(name_pattern)
  return [zip_info for zip_info in zip_file.infolist()
          if name_regex.match(zip_info.filename)]


class AndroidDevice(object):
  """Represents an Android device (an emulator or a real device).

  Attributes:
    serial: A string of serial. e.g. emulator-5554
            Serial can be seen by `adb devices` command.
    android_home: A string pointing the root directory of the SDK.
  """

  def __init__(self, serial, android_home):
    """Initializes with serial string.

    Args:
      serial: A serial string, which is set to self.serial.
      android_home: A string pointing the root directory of the SDK.
    """
    self.serial = serial
    # Don't access this property dicretly.
    # Use self._GetAvdName() instead.
    self._avd_name = None
    self._android_home = android_home

  def GetLogger(self):
    """Gets logger instance.

    Logger instance must not be a instance variable as
    AndroidDevice must be able to be pickled.

    Returns:
      Logger instance.
    """
    # If we have avd_name, create own logger and return it.
    # Don't keep this as an instance field.
    # AndroidDevice must be able to be pickled.
    if self._avd_name:
      logger = logging.getLogger(self._avd_name)
      logger.setLevel(logging.INFO)
      return logger
    # Otherwise return root logger.
    # Root logger is used at _GetAvdName()'s first call.
    return logging.getLogger()

  def WaitForDevice(self):
    """Equivalent to command "adb wait-for-device".

    Raises:
      AndroidUtilSubprocessError: wait-for-device is failed.
    """
    args = [os.path.join(self._android_home, 'platform-tools', 'adb'), '-s',
            self.serial, 'wait-for-device']
    if subprocess.call(args):
      raise AndroidUtilSubprocessError(
          'wait-for-device failed for %s' % self.serial)

  def IsAcceptableAbi(self, abi):
    """Returns if given abi is executable on this device.

    Args:
      abi: ABI name. e.g. armeabi, armeabi-v7a, x86, mips
    Returns:
      True if given abi is executable on this device.
    """
    return abi in (self.GetProperty(key) for key
                   in ['ro.product.cpu.abi', 'ro.product.cpu.abi2'])

  def _RunCommand(self, *command):
    """Run a command on android via adb.

    Args:
      *command: A list of arguments which are executed by Popen.
    Returns:
      stdout output of the invoked process.
    Raises:
      AndroidUtilSubprocessError: Failed to run given command.
    """
    args = [os.path.join(self._android_home, 'platform-tools', 'adb'),
            '-s', self.serial, 'shell']
    args.extend(command)
    self.GetLogger().info('Running at %s: %s', self.serial, args)
    process = subprocess.Popen(args, stdout=subprocess.PIPE)
    output, _ = process.communicate()
    if process.returncode != 0:
      raise AndroidUtilSubprocessError(
          'Failed to run the command: ' + ' '.join(command))
    for i in output.splitlines():
      self.GetLogger().info(i)
    return output

  def _GetAvdName(self):
    if not self._avd_name:
      # [mozc.avd_name] property is set by build_mozc.py
      self._avd_name = self.GetProperty('mozc.avd_name')
      # If the property is not available (e.g. on real device or an emulator
      # instance launched not from build_mozc.py),
      # use [ro.build.display.id].
      if not self._avd_name:
        self._avd_name = self.GetProperty('ro.build.display.id')
    return self._avd_name

  def GetProperty(self, key):
    return self._RunCommand('getprop', key).rstrip()

  @staticmethod
  def GetDevices(android_home):
    """Gets all the available devices via adb.

    The devices which are accessible from adb and of which status is
    'device' or 'offline' are returned.
    'unknown' devices are not included becuase it will never be accessible.

    Args:
      android_home: A string pointing the root directory of the SDK.

    Raises:
      AndroidUtilSubprocessError: when `adb` commands fails.
    Returns:
      A list of AndroidDevice.
    """
    args = [os.path.join(android_home, 'platform-tools', 'adb'), 'devices']
    logging.info('Running: %s', args)
    process = subprocess.Popen(args, stdout=subprocess.PIPE)
    output, _ = process.communicate()
    if process.returncode != 0:
      raise AndroidUtilSubprocessError(
          'Failed to run the command: ' + ' '.join(args))
    for i in output.splitlines():
      logging.info(i)
    result = []
    # offline is used for such emulator processes which have just been launched.
    regex = re.compile(r'(.*?)\s+(?:device|offline)$')
    for line in output.splitlines():
      match = regex.match(line)
      if match:
        result.append(AndroidDevice(match.group(1), android_home))
    return result


class Emulator(object):
  """An emulator and emulator related stuff."""

  @staticmethod
  def LaunchAll(android_sdk_home, avd_configs, android_home):
    """Creates and launches Emulator instances.

    Args:
      android_sdk_home: Path to android sdk home, containing .android directory.
      avd_configs: A list of dictionaries which contains parameters
                   for emulator.
      android_home: A string pointing the root directory of the SDK.

    Raises:
      AndroidEmulatorConditionError: No availabe ports.

    Returns:
      A list of Emulator instantces.
    """
    emulators = []

    ports = GetAvailableEmulatorPorts(android_home)
    if not ports:
      raise AndroidEmulatorConditionError('No ports for ADB are available')
    for avd_config in avd_configs:
      SetUpTestingSdkHomeDirectory(android_sdk_home,
                                   android_home,
                                   avd_config)
      avd_name = avd_config['--name']
      emulator = Emulator(android_sdk_home, avd_name, android_home)
      emulator.Launch()
      emulators.append(emulator)

    for emulator in emulators:
      emulator.GetAndroidDevice(android_home).WaitForDevice()
      logging.info('Emulator %s gets ready.', emulator.serial)

    return emulators

  def __init__(self, android_sdk_home, avd_name, android_home):
    """Create an Emulator instance.

    After instantiation, an emulator process is not lauched.
    To launch the process, call Launch method.
    Args:
      android_sdk_home: Path to android sdk home, containing .android directory.
      avd_name: Name of an avd to use.
      android_home: A string pointing the root directory of the SDK.
    """
    self._android_sdk_home = android_sdk_home
    self._avd_name = avd_name
    self._android_home = android_home
    self._avd_dir = os.path.join(android_sdk_home, '.android',
                                 'avd', '%s.avd' % avd_name)
    self._avd_properties = GetAvdProperties(android_sdk_home, avd_name)
    # _process can be used for checking whether emulator process has been
    # launched or not.
    self._process = None
    self._port_number = None
    self._lock_fd = None

  def _LockPort(self, port_number):
    """Trys to lock given port_number using lock file."""
    if not os.path.exists(LOCK_DIR):
      os.makedirs(LOCK_DIR)
    if self._port_number:
      raise AndroidUtilSubprocessError('Locked twice.')
    self._lock_fd = open(os.path.join(LOCK_DIR, 'port%d.lock' % port_number),
                         'w')
    try:
      fcntl.flock(self._lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except IOError:
      logging.info('Port %d is alredy used.', port_number)
      self._lock_fd.close()
      return False
    return True

  def Launch(self):
    """Launhes an emulator process.

    Raises:
      AndroidUtilSubprocessError: If fails on launching a process.
    """
    if self._process:
      raise AndroidUtilSubprocessError(
          'Emulator process has been launched already.')

    ports = GetAvailableEmulatorPorts(self._android_home)
    for port in ports:
      if self._LockPort(port):
        self._port_number = port
        break
    if not self._port_number:
      raise AndroidUtilSubprocessError('No available port.')

    self.serial = 'emulator-%s' % self._port_number

    args = [
        os.path.join(self._android_home, 'tools', 'emulator'),
        '-avd', self._avd_name,
        '-port', str(self._port_number),
        '-no-snapshot-load',  # Should do full-boot.
        '-no-snapshot-save',
        '-no-window',  # For headless environment.
        '-prop', 'mozc.avd_name=%s' % self._avd_name,
        '-verbose',
    ]
    logging.info('Launching an emulator; %s', args)
    # ANDROID_SDK_HOME: Where .avd file are located.
    # ANDROID_SDK_ROOT: Where SDK is installed.
    env = {'ANDROID_SDK_HOME': self._android_sdk_home,
           'ANDROID_SDK_ROOT': self._android_home}
    self._process = subprocess.Popen(args, env=env)
    if self._process.poll() is not None:
      raise AndroidUtilSubprocessError('Emulator launch fails.')

  def CopyTreeToSdCard(self, host_src_dir, sd_dest_dir):
    """Copies tree from host to SD card.

    Args:
      host_src_dir: Source path (host).
      sd_dest_dir: Destination path (SD card).
    Raises:
      AndroidUtilSubprocessError: If fails on mtools command.
    """
    if self._process:
      raise AndroidUtilSubprocessError(
          'Emulator process for %s has been launched already '
          'so SD card modifaction fails.' % self.serial)
    sdcard_path = os.path.join(self._avd_dir, 'sdcard.img')
    args = ['mtools', '-c', 'mcopy', '-i', sdcard_path, '-s', host_src_dir,
            '::%s' % sd_dest_dir]
    logging.info('Copying into SD card: %s', args)
    # The size of an SD card which mksdcard creates is invalid for mtools.
    # Skip format check by MTOOLS_SKIP_CHECK variable.
    if subprocess.call(args, env={'MTOOLS_SKIP_CHECK': '1'}):
      raise AndroidUtilSubprocessError(
          'Copying %s into SD card fails' % host_src_dir)

  def GetAndroidDevice(self, android_home):
    return AndroidDevice(self.serial, android_home)

  def Terminate(self):
    """Terminate the process.

    Raises:
      AndroidUtilSubprocessError: Emulator process has not been launded.
    """
    if not self._process:
      raise AndroidUtilSubprocessError(
          'Emulator process has not been launched.')
    self._process.terminate()
    self._process = None
    fcntl.flock(self._lock_fd, fcntl.LOCK_UN)
    self._lock_fd.close()


def GetAvailableEmulatorPorts(android_home):
  """Returns a list of ports which are available for an emulator.

  Returned port can be passed to Launch method as port parameter.
  Args:
    android_home: A string pointing the root directory of the SDK.
  Returns:
    List of available ports.
  """
  args = [os.path.join(android_home, 'platform-tools', 'adb'), 'devices']
  process = subprocess.Popen(args, stdout=subprocess.PIPE)
  (devices_result, _) = process.communicate()
  used_ports = set(int(port) for port
                   in re.findall(r'emulator-(\d+)', devices_result))
  return [port for port in xrange(5554, 5586, 2) if port not in used_ports]


def SetUpTestingSdkHomeDirectory(dest_android_sdk_home,
                                 android_home,
                                 options,
                                 my_open=open):
  """Creates AVD in testing sdk home directory based on given options.

  Args:
    dest_android_sdk_home: testing sdk home
    android_home: SDK's home (e.g, /home/me/android-sdk-linux_x86)
    options: dictionary which is passed to `android create avd' command
    my_open: For testing purpose

  Raises:
    AndroidUtilSubprocessError: thrown when AVD creation fails.
  """
  # Make sure destination directory.
  avd_dir = os.path.join(dest_android_sdk_home, '.android', 'avd')
  if not os.path.exists(avd_dir):
    os.makedirs(avd_dir)

  args = [os.path.join(android_home, 'tools', 'android'),
          'create', 'avd',
          '--force',
          '--sdcard', '512M',]
  for key, value in options.iteritems():
    args.extend([key, value])
  env = {'ANDROID_SDK_HOME': os.path.abspath(dest_android_sdk_home)}
  logging.info('Creating AVD: %s', args)
  if subprocess.call(args, env=env):
    raise AndroidUtilSubprocessError('AVD creation fails.')

  # Suppress usage stats dialog.
  ddmscfg_path = os.path.join(dest_android_sdk_home, '.android', 'ddms.cfg')
  with my_open(ddmscfg_path, 'w') as f:
    f.write('pingOptIn=false\n')
    f.write('pingId=0\n')


def GetAvdNames(android_sdk_home):
  """Returns avd names which are available in android_sdk_home."""
  result = []
  ini_file_glob = os.path.join(android_sdk_home, '.android', 'avd', '*.ini')
  for ini_file_path in glob.glob(ini_file_glob):
    result.append(os.path.splitext(os.path.basename(ini_file_path))[0])
  return result


def GetAvdProperties(android_sdk_home, avd_name, my_open=open):
  """Returns a property, which is based on ${avd_name}/config.ini file."""
  # Argument 'open' is for testing.
  config_path = os.path.join(android_sdk_home, '.android', 'avd',
                             '%s.avd' % avd_name, 'config.ini')
  result = {}
  with my_open(config_path, 'r') as f:
    for line in f:
      m = re.match(r'^(.*?)=(.*)$', line)
      if m:
        result[m.group(1)] = m.group(2)
  return result


def main():
  for arg in sys.argv[1:]:
    for item in sorted(GetApkProperties(arg).items()):
      print '%s: %s' % item


if __name__ == '__main__':
  main()
