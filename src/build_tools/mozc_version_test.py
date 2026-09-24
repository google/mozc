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

"""Tests for build_tools.mozc_version."""


import logging
import os
import re
import tempfile
from absl.testing import absltest
from build_tools import mozc_version


def _GenerateVersionFile(template, target_platform):
  try:
    _, temp_file_name = tempfile.mkstemp()
    with open(temp_file_name, 'w') as fp:
      fp.write(template)
    _, result_file_name = tempfile.mkstemp()
    version_format = ('MAJOR = @MAJOR@\n'
                      'MINOR = @MINOR@\n'
                      'BUILD = @BUILD@\n'
                      'REVISION = @REVISION@\n'
                      'TARGET_PLATFORM = @TARGET_PLATFORM@\n')
    mozc_version.GenerateVersionFileFromTemplate(temp_file_name,
                                                 result_file_name,
                                                 version_format,
                                                 target_platform)
    return result_file_name
  finally:
    try:
      os.remove(temp_file_name)
    except Exception as inst:  # pylint: disable=broad-except
      logging.warning(inst)


def _GetGeneratedVersionFile(template, target_platform):
  generated_file_name = _GenerateVersionFile(template, target_platform)
  try:
    with open(generated_file_name) as fp:
      return fp.read()
  finally:
    try:
      os.remove(generated_file_name)
    except Exception as inst:  # pylint: disable=broad-except
      logging.warning(inst)


class MozcVersionTest(absltest.TestCase):

  def testGenerateVersionFileFromTemplate(self):
    no_revision = """MAJOR = 10
      MINOR = 20
      BUILD = 30"""
    dev_revision = """MAJOR = 10
      MINOR = 20
      BUILD = 30
      REVISION = 900"""
    stable_revision = """MAJOR = 10
      MINOR = 20
      BUILD = 30
      REVISION = 9"""

    class TestData(object):
      def __init__(self, template, target_platform, expect_revision):
        self.template = template
        self.target_platform = target_platform
        self.expect_revision = expect_revision

      def __str__(self):
        return """template = %s
target_platform = %s
expect_revision = %s""" % (self.template, self.target_platform,
                           self.expect_revision)

    test_data_list = [
        # - The length of revision is equal to that of template's.
        # - The last character of revision is calaculated based on
        #     target_platform.
        # Note that no_revision is not applicable because mozc_version
        # cannot determin revision.
        TestData(dev_revision, 'Windows', '900'),
        TestData(dev_revision, 'Android', '903'),
        TestData(stable_revision, 'Windows', '0'),
        TestData(stable_revision, 'Android', '3'),
        # Test for other platforms.
        TestData(dev_revision, 'Mac', '901'),
        TestData(dev_revision, 'Linux', '902'),
        TestData(dev_revision, 'ChromeOS', '904'),
        TestData(dev_revision, 'Wasm', '907'),
    ]
    for test_data in test_data_list:
      result = _GetGeneratedVersionFile(test_data.template,
                                        test_data.target_platform)
      self.assertEqual(re.search(r'^REVISION = (.*)$',
                                 result, re.MULTILINE).group(1),
                       test_data.expect_revision,
                       test_data)
      self.assertEqual(re.search(r'^TARGET_PLATFORM = (.*)$',
                                 result, re.MULTILINE).group(1),
                       test_data.target_platform,
                       test_data)


if __name__ == '__main__':
  absltest.main()
