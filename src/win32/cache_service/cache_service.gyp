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

{
  'variables': {
    'relative_dir': 'win32/cache_service',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'cache_service_product_name_win': 'GoogleIMEJaCacheService',
      }, {  # else
        'cache_service_product_name_win': 'mozc_cache_service',
      }],
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'genproto_service_state',
          'type': 'none',
          'sources': [
            'win32_service_state.proto',
          ],
          'includes': [
            '../../protobuf/genproto.gypi',
          ],
        },
        {
          'target_name': 'service_state_orotocol',
          'type': 'static_library',
          'hard_dependency': 1,
          'sources': [
            '<(proto_out_dir)/<(relative_dir)/win32_service_state.pb.cc',
          ],
          'dependencies': [
            '<(mozc_oss_src_dir)/protobuf/protobuf.gyp:protobuf',
            'genproto_service_state',
          ],
          'export_dependent_settings': [
            'genproto_service_state',
          ],
        },
        {
          'target_name': 'gen_mozc_cache_service_resource_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_cache_service',
            'gen_main_resource_path': 'win32/cache_service/mozc_cache_service.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/mozc_cache_service_autogen.rc',
          },
          'includes': [
            '../../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'cache_service_manager',
          'type': 'static_library',
          'sources': [
            'cache_service_manager.cc',
          ],
          'dependencies': [
            '<(mozc_src_dir)/base/base.gyp:base',
            '<(mozc_src_dir)/base/base.gyp:encryptor',
            'service_state_orotocol',
          ],
        },
        {
          'target_name': 'mozc_cache_service',
          'product_name': '<(cache_service_product_name_win)',
          'type': 'executable',
          'sources': [
            'mozc_cache_service.cc',
            '<(gen_out_dir)/mozc_cache_service_autogen.rc',
          ],
          'dependencies': [
            'cache_service_manager',
            'gen_mozc_cache_service_resource_header',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': 'mozc_cache_service.exe.manifest',
              'EmbedManifest': 'true',
            },
            'VCLinkerTool': {
              'AdditionalDependencies': [
                'crypt32.lib',  # used in 'mozc_cache_service.cc'
              ],
            },
          },
        },
      ],
    }],
  ],
}
