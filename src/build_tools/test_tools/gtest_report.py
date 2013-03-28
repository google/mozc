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

"""Converting gtest result xml to python object.

The gtest can output test result as xml file. Following class can parse it and
store as python object.
"""

__author__ = "nona"

import cStringIO as StringIO
import logging
from xml.etree import cElementTree as ElementTree


class Failure(object):
  """Corresponding to failure element in test output xml."""

  def __init__(self, message, contents):
    self.message = message
    self.contents = contents

  @classmethod
  def CreateFromXMLElement(cls, element):
    return cls(element.get('message'), element.text)


class TestCase(object):
  """Corresponding to testcase element in test output xml."""

  def __init__(self, name, status, time, classname, failures):
    self.name = name
    self.status = status
    self.time = time
    self.classname = classname
    self.failures = failures

  @classmethod
  def CreateFromXMLElement(cls, element):
    failures = [Failure.CreateFromXMLElement(failure) for failure in element]
    return cls(element.get('name'), element.get('status'),
               element.get('time'), element.get('classname'), failures)


class TestSuite(object):
  """Corresponding to testsuite element in test output xml."""

  def __init__(self, name, total, fail_num, disabled_num, error_num, time,
               testcases):
    self.name = name
    self.total = int(total)
    self.fail_num = int(fail_num)
    self.disabled_num = int(disabled_num)
    self.error_num = int(error_num)
    self.time = time
    self.testcases = testcases

  def GetErrorSummary(self):
    """Returns summarized error report text."""
    if self.fail_num == 0:
      return ''
    output = StringIO.StringIO()
    for testcase in self.testcases:
      if not testcase.failures:
        continue
      print >>output, '%s.%s:' % (self.name, testcase.name)
      for failure in testcase.failures:
        print >>output, failure.contents.encode('utf-8')
    return output.getvalue()

  @classmethod
  def CreateFromXMLElement(cls, element):
    testcases = [TestCase.CreateFromXMLElement(testcase) for
                 testcase in element]
    return cls(element.get('name'), element.get('tests'),
               element.get('failures'), element.get('disabled'),
               element.get('errors'), element.get('time'), testcases)


class TestSuites(object):
  """Corresponding to testsuites element in output xml."""

  def __init__(self, name, total, fail_num, disabled_num, error_num, time,
               timestamp, testsuites):
    self.name = name
    self.total = int(total)
    self.fail_num = int(fail_num)
    self.disabled_num = int(disabled_num)
    self.error_num = int(error_num)
    self.timestamp = timestamp
    self.time = time
    self.testsuites = testsuites

  def GetErrorSummary(self):
    """Returns summarized unit test errors."""
    return '\n'.join(suite.GetErrorSummary() for suite in self.testsuites if
                     suite.fail_num != 0)

  @classmethod
  def CreateFromXMLElement(cls, element):
    testsuites = [TestSuite.CreateFromXMLElement(testsuite) for
                  testsuite in element]
    return cls(element.get('name'), element.get('tests'),
               element.get('failures'), element.get('disabled'),
               element.get('errors'), element.get('time'),
               element.get('timestamp'), testsuites)


def GetFromXMLFile(xml_fname):
  """Create summary object from specified xml file."""
  try:
    tree = ElementTree.parse(xml_fname)
  except SyntaxError as reason:
    logging.critical('XML Parse Failed: %s', reason)
    return None
  return TestSuites.CreateFromXMLElement(tree.getroot())
