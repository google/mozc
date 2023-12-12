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
    'relative_dir': 'config',
  },
  'targets': [
    {
      'target_name': 'config_handler',
      'type': 'static_library',
      'hard_dependency': 1,
      'sources': [
        'config_handler.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_synchronization',
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../base/base.gyp:hash',
        '../base/base.gyp:version',
        '../protocol/protocol.gyp:config_proto',
      ],
    },
    {
      'target_name': 'stats_config_util',
      'type': 'static_library',
      'sources': [
        'stats_config_util.cc',
      ],
      'dependencies': [
        '../base/absl.gyp:absl_synchronization',
        'config_handler',
      ],
    },
    {
      'target_name': 'character_form_manager',
      'type': 'static_library',
      'sources': [
        'character_form_manager.cc',
      ],
      'dependencies': [
        'config_handler',
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../base/base.gyp:japanese_util',
        '../protocol/protocol.gyp:config_proto',
        # storage.gyp:storage is depended by character_form_manager.
        # TODO(komatsu): delete this line.
        '<(mozc_oss_src_dir)/storage/storage.gyp:storage',
      ]
    },
  ],
}
