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

{
  'targets': [
    {
      'target_name': 'testing',
      'type': 'static_library',
      'sources': [
        '<(DEPTH)/third_party/gtest/src/gtest.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-death-test.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-filepath.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-port.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-printers.cc',
        '<(DEPTH)/third_party/gtest/src/gtest-test-part.cc',
      ],
    },
    {
      'target_name': 'gtest_main',
      'type': 'static_library',
      'sources': [
        'base/internal/googletest.cc',
        'base/internal/gtest_main.cc',
       ],
      'conditions': [
        ['OS!="linux"', {
          'dependencies': [
            'testing',
          ],
        }, {  # OS=="linux"
          'conditions': [
            ['use_libgtest==1', {
              'link_settings': {
                'libraries': [
                  # NOTE(komatsu): Nice to use pkg-config when it is available.
                  '-lgtest',
                ],
              },
            }, {  # OS=="linux" and use_libgtest!="YES"
              'dependencies': [
                'testing',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
