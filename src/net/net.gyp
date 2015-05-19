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

{
  'targets': [
    {
      'target_name': 'net',
      'type': 'static_library',
      'sources': [
        'http_client.cc',
        'proxy_manager.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'conditions': [
        ['OS=="mac"', {
          'sources': [
            'http_client_mac.mm',
          ],
        }],
      ],
    },
    {
      'target_name': 'http_client_mock',
      'type': 'static_library',
      'sources': [
        'http_client_mock.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'net',
      ],
    },
    {
      'target_name': 'http_client_main',
      'type': 'executable',
      'sources': [
        'http_client_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'net',
      ],
    },
    {
      'target_name': 'http_client_mock_test',
      'type': 'executable',
      'sources': [
        'http_client_mock_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'http_client_mock',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'jsonpath',
      'type': 'static_library',
      'sources': [
        'jsonpath.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
      ],
    },
    {
      'target_name': 'jsonpath_test',
      'type': 'executable',
      'sources': [
        'jsonpath_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'jsonpath',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'net_all_test',
      'type': 'none',
      'dependencies': [
        'http_client_mock_test',
        'jsonpath_test',
      ],
    },
  ],
}
