# Copyright 2010-2014, Google Inc.
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
  'variables': {
    'relative_dir': 'net',
  },
  'targets': [
    {
      'target_name': 'http_client',
      'type': 'static_library',
      'sources': [
        'http_client.cc',
        'proxy_manager.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'conditions': [
        ['enable_http_client==1', {
          'conditions': [
            ['OS=="mac"', {
              'sources': [
                'http_client_mac.mm',
              ],
            }],
            ['OS=="win"', {
              'link_settings': {
                'msvs_settings': {
                  'VCLinkerTool': {
                    'AdditionalDependencies': [
                      'wininet.lib',
                    ],
                  },
                },
              },
            }],
            ['target_platform=="Linux"', {
              # Enable libcurl
              'cflags': [
                '<!@(<(pkg_config_command) --cflags libcurl)',
              ],
              'defines': [
                'HAVE_CURL=1',
              ],
              'link_settings': {
                'ldflags': [
                  '<!@(<(pkg_config_command) --libs-only-L libcurl)',
                ],
                'libraries': [
                  '<!@(<(pkg_config_command) --libs-only-l libcurl)',
                ],
              },
            }],
            ['target_platform=="NaCl" and _toolset=="target"', {
              'sources': [
                'http_client_pepper.cc',
              ],
            }],
            ['target_platform=="Android"', {
              'dependencies': [
                '../base/base.gyp:jni_proxy'
              ],
            }],
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
        'http_client',
      ],
    },
    {
      'target_name': 'jsonpath',
      'type': 'static_library',
      'sources': [
        'jsonpath.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'jsoncpp.gyp:jsoncpp',
      ],
    },
    {
      'target_name': 'json_util',
      'type': 'static_library',
      'sources': [
        'json_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../protobuf/protobuf.gyp:protobuf',
        'jsoncpp.gyp:jsoncpp',
      ],
    },
    {
      'target_name': 'http_task_manager',
      'type': 'static_library',
      'sources': [
        'http_task_manager.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:task',
        'http_client',
      ],
    },
  ],
}
