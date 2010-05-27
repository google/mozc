# Copyright 2010, Google Inc.
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
    'relative_dir': 'server',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'mozc_server',
      'type': 'executable',
      'sources': [
        'mozc_server.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../client/client.gyp:client',
        '../composer/composer.gyp:composer',
        '../converter/converter.gyp:converter',
        '../dictionary/dictionary.gyp:dictionary',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net',
        '../prediction/prediction.gyp:prediction',
        '../rewriter/rewriter.gyp:rewriter',
        '../session/session.gyp:session',
        '../storage/storage.gyp:storage',
        '../transliteration/transliteration.gyp:transliteration',
        '../usage_stats/usage_stats.gyp:usage_stats',
      ],
      'conditions': [
        ['OS=="mac"', {
          'product_name': 'GoogleJapaneseInputConverter',
          'sources': [
            '../mac/mozc_server_info',
          ],
          'dependencies': [
            'gen_mozc_server_info_plist',
          ],
          'mac_bundle': 1,
          'xcode_settings': {
            # Currently metadata in the Info.plist file like version
            # info go away because the generated xcodeproj don't know
            # version info.
            # TODO(mukai): write a script to expand those variables
            # and use that script instead of this INFOPLIST_FILE.
            'INFOPLIST_FILE': '<(gen_out_dir)/mozc_server_info',
          },
          'variables': {
            # This product name is used in postbuilds_mac.gypi.
            'product_name': 'GoogleJapaneseInputConverter',
          },
          'includes': [
            '../gyp/postbuilds_mac.gypi',
          ],
        }],
        ['OS=="win"', {
          'product_name': 'GoogleIMEJaConverter',
          'dependencies': [
          ],
          'includes': [
            '../gyp/postbuilds_win.gypi',
          ],
          'sources': [
            'mozc_server.rc',
            'mozc_server.exe.manifest',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'version.dll',
                'wininet.dll',
                'winmm.dll',
              ],
            },
          }
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'genproto_server',
          'type': 'none',
          'sources': [
            'win32_service_state.proto',
          ],
          'includes': [
            '../protobuf/genproto.gypi',
          ],
        },
        {
          'target_name': 'cache_service_manager',
          'type': 'static_library',
          'sources': [
            '<(proto_out_dir)/<(relative_dir)/win32_service_state.pb.cc',
            'cache_service_manager.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../protobuf/protobuf.gyp:protobuf',
            'genproto_server',
          ],
        },
        {
          'target_name': 'mozc_cache_service',
          'product_name': 'GoogleIMEJaCacheService',
          'type': 'executable',
          'includes': [
            '../gyp/postbuilds_win.gypi',
          ],
          'sources': [
            'mozc_cache_service.cc',
            'mozc_cache_service.rc',
            'mozc_cache_service.exe.manifest',
          ],
          'dependencies': [
             'cache_service_manager',
          ],
        },
      ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'gen_mozc_server_info_plist',
          'type': 'none',
          'actions': [
            {
              'action_name': 'generate',
              'inputs': [
                '../mac/mozc_server_info',
              ],
              'outputs': [
                '<(gen_out_dir)/mozc_server_info',
              ],
              'action': [
                'python', '../build_tools/tweak_info_plist.py',
                '--output', '<(gen_out_dir)/mozc_server_info',
                '--input', '../mac/mozc_server_info',
                '--version_file', '../mozc_version.txt',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
