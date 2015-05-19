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

"""A script to run unittests on android.

This file searches all tests under --test_bin_dir (the default is the current
directory), and run them on android (emulator or actual device).
You need to run an android emulator or to connect a dvice, before running this
script.
"""

__author__ = "hidehiko"

from build_tools.test_tools import gtest_report
from build_tools import android_util
import errno
import logging
import multiprocessing
import optparse
import os
import subprocess
import sys
import time
from xml.etree import cElementTree as ElementTree


RESULT_FILE_NAME = 'result.xml'


def VerifyResultXMLFile(xml_path):
  test_suites = gtest_report.GetFromXMLFile(xml_path)
  if not test_suites:
    error_message = '[FAIL] Result XML file is invalid; %s' % xml_path
  if test_suites.fail_num:
    print '[FAIL] Test failures are found:'
    error_message = test_suites.GetErrorSummary()
  else:
    print '[ OK ] All tests succeeded.'
    error_message = None
  if error_message:
    print error_message
  return error_message


def FindTestBinaries(test_dir):
  """Returns the file names of tests."""
  logging.info('Gathering binaries in %s.', os.path.abspath(test_dir))
  result = {}

  for abi in os.listdir(test_dir):
    dir_path = os.path.join(test_dir, abi)
    if not os.path.isdir(dir_path):
      continue
    logging.info('ABI found : %s', abi)
    binaries_for_abi = []
    for f in os.listdir(dir_path):
      path = os.path.join(dir_path, f)
      # Test binaries are "executable file" and "its name ends with _test."
      if (os.access(path, os.R_OK | os.X_OK) and
          os.path.isfile(path) and
          f.endswith('_test')):
        binaries_for_abi.append(f)
    result[abi] = binaries_for_abi
  return result


def ParseArgs():
  """Parses command line options and returns them."""
  parser = optparse.OptionParser()
  # Assume the parent directory is the Mozc root directory.
  program_dir = os.path.dirname(sys.argv[0])
  parent_dir = os.path.join(*os.path.split(program_dir)[:-1]) or '..'
  parser.add_option('--run_java_test', dest='run_java_test', default=False,
                    action='store_true',
                    help='Runs JUnit tests. [JAVA] options are used.')
  parser.add_option('--android_devices', dest='android_devices', default='',
                    help='[JAVA][NATIVE] Comma separated serial numbers '
                    'on which devices the tests run. '
                    'If not specified all the available devices are used.')
  parser.add_option('--run_native_test', dest='run_native_test', default=False,
                    action='store_true',
                    help='Runs native tests. [NATIVE] options are used.')
  parser.add_option('--configuration', dest='configuration',
                    default='Debug_Android',
                    help='[JAVA] Build configuration. Used to build correct '
                    'testee binary.')
  parser.add_option('--app_package_name', dest='app_package_name',
                    default='org.mozc.android.inputmethod.japanese',
                    help='[JAVA] Testee project\'s package name. '
                    'This is used to find test reporting XML file.')
  parser.add_option('--mozc_root_dir', dest='mozc_root_dir', default=parent_dir,
                    help='[NATIVE] The root directory of Mozc project')
  parser.add_option('--remote_dir', dest='remote_dir',
                    default='/sdcard/mozctest',
                    help='[NATIVE] Working directory on '
                    'a SD card on a Android device')
  parser.add_option('--remote_device', dest='remote_device',
                    default='/dev/block/vold/179:0',
                    help='[NATIVE] Path to the device to be (re)mounted.')
  parser.add_option('--remote_mount_point', dest='remote_mount_point',
                    default='/sdcard',
                    help='[NATIVE] Mount point for the --remote_device.')
  parser.add_option('--test_bin_dir', dest='test_bin_dir', default='.',
                    help='[NATIVE] The directory which contains test binaries')
  parser.add_option('--test_case', dest='testcase', default='',
                    help='[NATIVE] Limits testcases to run.')
  parser.add_option('--mozc_dictionary_data_file',
                    dest='mozc_dictionary_data_file', default=None,
                    help='[NATIVE] The relative path from mozc_root_dir to '
                    'the system.dictionary file.')
  parser.add_option('--mozc_connection_data_file',
                    dest='mozc_connection_data_file', default=None,
                    help='[NATIVE] The relative path from mozc_root_dir to '
                    'the connection.data file.')
  parser.add_option('--mozc_test_connection_data_file',
                    dest='mozc_test_connection_data_file', default=None,
                    help='[NATIVE] The relative path from mozc_root_dir to '
                    'the test_connection.data file.')
  parser.add_option('--mozc_data_dir',
                    dest='mozc_data_dir', default=None,
                    help='[NATIVE] The relative path from mozc_root_dir to '
                    'the data directory.')
  parser.add_option('--output_report_dir', dest='output_report_dir',
                    default=None,
                    help='[JAVA][NATIVE] Path to output gtest '
                    'reporting XML files')
  return parser.parse_args()[0]


def AppendPrefixToSuiteName(in_file_name, out_file_name, prefix):
  """Appends prefix to <testsuite> element's name attribute."""

  def AppendPrefix(elem):
    name = elem.attrib['name']
    elem.attrib['name'] = prefix + name
  tree = ElementTree.parse(in_file_name)
  root = tree.getroot()
  # Root element isn't returned by any traversing methods.
  if root.tag == 'testsuite':
    AppendPrefix(root)
  for elem in root.findall('testsuite'):
    AppendPrefix(elem)
  with open(out_file_name, 'w') as f:
    # Note that ElementTree of 2.6 doesn't write XML declaration.
    f.write('<?xml version="1.0" encoding="utf-8"?>\n')
    f.write(ElementTree.tostring(root, 'utf-8'))


class AndroidDevice(android_util.AndroidDevice):
  def __init__(self, serial):
    android_util.AndroidDevice.__init__(self, serial)

  def WaitForMount(self):
    """Wait until SD card is mounted."""
    retry = 5
    sleep = 30
    for _ in xrange(retry):
      if self._RunCommand('mount').find('/sdcard') != -1:
        logging.info('SD card has been mounted.')
        return
      logging.info(
          'SD card has not been mounted. Wait and retry. '
          'Don\'t worry. This is typically expected behavior.')
      time.sleep(sleep)
    raise IOError('No mounted SD card found. Something went wrong or the '
                  'emulator is too slow.')

  def CopyFile(self, host_path, remote_path, operation):
    """Copy a file between host and remote.

    Args:
      host_path: path in host side.
      remote_path: path in remote side.
      operation: 'push' or 'pull'.
        If 'push', 'adb push host_path remote_path' is executed.
        If 'pull', 'adb pull remote_path host_path' is executed.
    """
    if operation == 'push':
      command_args = ['adb', '-s', self.serial, 'push', host_path, remote_path]
    elif operation == 'pull':
      command_args = ['adb', '-s', self.serial, 'pull', remote_path, host_path]
    else:
      raise ValueError('operation parameter must be push or pull but '
                       '%s is given.' % operation)
    logging.info('Copying at %s: %s', self.serial, command_args)
    process = subprocess.Popen(command_args)
    if process.wait() == 0:
      return
    raise IOError(
        'Failed to copy a file: %s to %s' % (host_path, remote_path))

  def _CopyAndVerifyResult(
      self, test_name, remote_result_path, host_result_path):
    try:
      self.CopyFile(host_path=host_result_path,
                    remote_path=remote_result_path,
                    operation='pull')
      print '[ OK ] The process terminated with no crash.'
      error_message = VerifyResultXMLFile(host_result_path)
    except IOError:
      error_message = ('[FAIL] Result file does not exist. '
                       'The process might crash: %s' % test_name)
    if error_message:
      logging.critical(error_message)
    return error_message

  def RunOneTest(self, test_bin_dir, abi, test_name, remote_dir,
                 output_report_dir):
    """Execute each test."""
    prefix = '%s::%s::' % (self._GetAvdName(), abi)
    host_path = os.path.join(test_bin_dir, abi, test_name)
    remote_path = os.path.join(remote_dir, test_name)
    output_report_file_name = '%s%s.xml' % (prefix, test_name)
    if output_report_dir:
      output_report_path = os.path.join(output_report_dir,
                                        output_report_file_name)
    else:
      output_report_path = os.tmpnam()

    # Create default test report file.
    CreateDefaultReportFile(output_report_path, test_name, prefix)

    # Copy the binary file, run the test, and clean up the executable.
    self.CopyFile(host_path=host_path,
                  remote_path=remote_path,
                  operation='push')
    remote_report_path = os.path.abspath(os.path.join(remote_dir,
                                                      RESULT_FILE_NAME))
    # We should append colored_log=false to suppress escape sequences on
    # continuous build.
    command = ['cd', remote_dir, ';',
               './' + test_name, '--test_srcdir=.', '--logtostderr',
               '--gunit_output=xml:%s' % remote_report_path,
               '--colored_log=false']
    self._RunCommand(*command)
    temporal_report_path = os.tmpnam()
    error_message = self._CopyAndVerifyResult(
        test_name, remote_report_path, temporal_report_path)
    # Successfully the result file is pulled.
    if not error_message:
      # Append prefix to testsuite name.
      # Otherwise duplicate testsuites name will be generated finally.
      AppendPrefixToSuiteName(temporal_report_path, output_report_path,
                              prefix)
      self._RunCommand('rm', remote_path)
      self._RunCommand('rm', remote_report_path)
    return error_message

  def SetUpTest(self, device, mount_point, host_dir, remote_dir,
                dictionary_data, connection_data, test_connection_data,
                mozc_data_dir):
    """Set up the android to run tests."""
    self.WaitForMount()
    # Now, the binary size of unittests are getting bigger and actually,
    # some of them exceed the /data/data/... quota.
    # So we'll use sdcard, instead. Unfortunately, sdcard has noexec
    # attribute
    # by default, so we remove it by remounting.
    self._RunCommand('mount', '-o', 'remount', device, mount_point)
    # Some tests depend on dictionary data. In the product, the
    # data is set at jni loading time, but it is necessary to somehow
    # set the data in native tests. So, copy the dictionary data to the
    # emulator.
    self.CopyFile(host_path=os.path.join(host_dir, dictionary_data),
                  remote_path=os.path.join(remote_dir,
                                           'embedded_data', 'dictionary_data'),
                  operation='push')
    self.CopyFile(host_path=os.path.join(host_dir, connection_data),
                  remote_path=os.path.join(remote_dir,
                                           'embedded_data', 'connection_data'),
                  operation='push')
    self.CopyFile(host_path=os.path.join(host_dir, test_connection_data),
                  remote_path=os.path.join(remote_dir,
                                           'converter',
                                           'test_connection_data.data'),
                  operation='push')
    # mozc_data_dir contains both generated .h files and test data.
    # We want only test data and they are in mozc_data_dir/data.
    # TODO(matsuzakit): Split generated .h files and test data
    #                   into separate directories.
    self.CopyFile(host_path=os.path.join(host_dir, mozc_data_dir, 'data'),
                  remote_path=os.path.join(remote_dir, 'data'),
                  operation='push')

  def TearDownTest(self, remote_dir):
    self._RunCommand('rm', '-r', remote_dir)

  def RunNativeTests(self, options, binaries_for_abis):
    logging.info('[NATIVE] Testing device=%s', self.serial)
    error_messages = []
    self.SetUpTest(options.remote_device, options.remote_mount_point,
                   options.mozc_root_dir, options.remote_dir,
                   options.mozc_dictionary_data_file,
                   options.mozc_connection_data_file,
                   options.mozc_test_connection_data_file,
                   options.mozc_data_dir)
    if options.testcase:
      test_list = options.testcase.split(',')
    for abi in binaries_for_abis.iterkeys():
      if not self.IsAcceptableAbi(abi):
        continue
      logging.info('[NATIVE] Testing device=%s, abi=%s', self.serial, abi)
      for test in binaries_for_abis[abi]:
        if test_list and test not in test_list:
          continue
        logging.info('[NATIVE] Testing device=%s, abi=%s, test=%s',
                     self.serial, abi, test)
        error_message = self.RunOneTest(options.test_bin_dir, abi, test,
                                        options.remote_dir,
                                        options.output_report_dir)
        if error_message:
          error_messages.append(error_message)
    self.TearDownTest(options.remote_dir)
    return error_messages

  def RunJavaTests(self, output_report_dir, configuration, app_package_name):
    logging.info('[JAVA] Testing device=%s', self.serial)
    prefix = '%s::' % self._GetAvdName()
    report_file_name_remote = 'gtest-report.xml'
    report_file_name_host = prefix + 'java_layer.xml'
    output_report_path = os.path.join(output_report_dir,
                                      report_file_name_host)
    CreateDefaultReportFile(output_report_path, 'JUnit all tests', prefix)
    # Run 'run-tests' target on specified device.
    # This target does build (testee and tester), install (both) and run.
    args = ['ant', 'run-tests',
            '-Dadb.device.arg=-s %s' % self.serial,
            '-Dgyp.build_type=%s' % configuration]
    process = subprocess.Popen(args, cwd='tests')
    process.wait()
    if process.wait() != 0:
      return '[FAIL] [JAVA] run_test target fails'

    remote_report_path = os.path.join('data', 'data',
                                      app_package_name, report_file_name_remote)
    try:
      temporal_report_path = os.tmpnam()
      self.CopyFile(host_path=temporal_report_path,
                    remote_path=remote_report_path,
                    operation='pull')
      print '[ OK ] The process terminated with no crash.'
      # Append prefix to testsuite name.
      # Otherwise duplicate testsuites name will be generated finally.
      AppendPrefixToSuiteName(temporal_report_path, output_report_path,
                              prefix)
      # XML file verification is omitted because
      # - Java's report XML doesn't fit verifier's expectation.
    except IOError:
      return '[FAIL] Result file does not exist. The process might crash.'

    # TODO(matsuzakit): Verify reporting XML file here.
    # gtest_report.GetFromXMLFile() is not applicable becuase of
    # format difference.
    return None

  @staticmethod
  def GetDevices():
    return [AndroidDevice(super_device.serial)
            for super_device in android_util.AndroidDevice.GetDevices()]


def CreateDefaultReportFile(output_report_path, test_name, prefix):
  # Create default test report file.
  # It will be used as the result of the test if the test crashes.
  with open(output_report_path, 'w') as f:
    f.write("""<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="%s%s" tests="1" errors="1">
  <testcase name="No reporting XML">
    <error message="No reporting XML has been generated. Process crash?" />
  </testcase>
</testsuite>""" % (prefix, test_name))


def RunTest(device, options, binaries_for_abis):
  error_messages = []
  if options.run_native_test:
    native_error_messages = device.RunNativeTests(options, binaries_for_abis)
    if native_error_messages:
      error_messages.extend(native_error_messages)
  if options.run_java_test:
    error_message = device.RunJavaTests(options.output_report_dir,
                                        options.configuration,
                                        options.app_package_name)
    if error_message:
      error_messages.append(error_message)
  return '\n'.join(error_messages)


def main():
  # Enable logging.info.
  logging.getLogger().setLevel(logging.INFO)

  options = ParseArgs()

  if options.run_native_test:
    binaries_for_abis = FindTestBinaries(options.test_bin_dir)
    logging.info(binaries_for_abis)
  else:
    binaries_for_abis = None

  # Prepare reporting directory.
  if options.output_report_dir:
    options.output_report_dir = os.path.abspath(options.output_report_dir)
    try:
      os.makedirs(options.output_report_dir)
      logging.info('Made directory; %s', options.output_report_dir)
    except OSError as e:
      if e.errno == errno.EEXIST:
        logging.info('%s has existed already.', options.output_report_dir)
      else:
        raise

  devices = AndroidDevice.GetDevices()
  logging.info('All the activated devices are %s',
               [device.serial for device in devices])
  if options.android_devices:
    using_devices = options.android_devices.split(',')
    devices = [device for device in devices if device.serial in using_devices]
    logging.info('Filtered to %s',
                 [device.serial for device in devices])

  if not devices:
    logging.info('No devices are specified. Do nothing.')
    return

  # Maximum # of devices is 10 so running them in the same time is possible.
  pool = multiprocessing.Pool(len(devices))
  results = [pool.apply_async(RunTest, [device, options, binaries_for_abis])
             for device in devices]
  pool.close()
  # result.get() blocks until the test terminates.
  error_messages = [result.get() for result in results if result.get()]
  if error_messages:
    print '[FAIL] Native tests result : Test failures are found;'
    for message in error_messages:
      print message
  else:
    print '[ OK ] Native tests result : Tests scceeded.'


if __name__ == '__main__':
  main()
