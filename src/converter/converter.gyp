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
  'variables': {
    'relative_mozc_dir': '',
    'gen_out_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_mozc_dir)',
  },
  'targets': [
    {
      'target_name': 'converter',
      'type': 'static_library',
      'sources': [
        '<(gen_out_mozc_dir)/dictionary/pos_matcher.h',
        'converter.cc',
        'converter_mock.cc',
      ],
      'dependencies': [
        '../composer/composer.gyp:composer',
        '../data_manager/data_manager.gyp:user_pos_manager',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../prediction/prediction.gyp:prediction',
        '../prediction/prediction.gyp:prediction_protocol',
        '../rewriter/rewriter.gyp:rewriter',
        '../session/session_base.gyp:session_protocol',
        'converter_base.gyp:immutable_converter',
        'converter_base.gyp:segments',
      ],
    },
    {
      'target_name': 'converter_main',
      'type': 'executable',
      'sources': [
        'converter_main.cc',
       ],
      'dependencies': [
        '../composer/composer.gyp:composer',
        'converter',
        'converter_base.gyp:segments',
      ],
    },
    {
      'target_name': 'connection_data_injected_environment',
      'type': 'static_library',
      'sources': [
        'connection_data_injected_environment.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base_core',
        '../testing/testing.gyp:testing',
        'converter_base.gyp:connector',
      ],
    },
  ],
}
