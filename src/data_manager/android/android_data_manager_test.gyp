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
    'relative_dir': 'data_manager/android',
    'relative_mozc_dir': '',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'gen_out_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_mozc_dir)',
  },
  'targets': [
    {
      'target_name': 'android_data_manager_test',
      'type': 'executable',
      'sources': [
        'android_data_manager_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        '../data_manager.gyp:data_manager_test_base',
        'android_data_manager.gyp:android_data_manager',
        'android_data_manager.gyp:gen_android_segmenter_inl_header#host',
      ],
      'variables': {
        'test_size': 'small',
      },
      'copies': [
        {
          'destination': '<(mozc_data_dir)/data/dictionary_android/',
          'files': [ '../../data/dictionary_android/connection.txt',
                     '../../data/dictionary_android/dictionary00.txt',
                     '../../data/dictionary_android/dictionary01.txt',
                     '../../data/dictionary_android/dictionary02.txt',
                     '../../data/dictionary_android/dictionary03.txt',
                     '../../data/dictionary_android/dictionary04.txt',
                     '../../data/dictionary_android/dictionary05.txt',
                     '../../data/dictionary_android/dictionary06.txt',
                     '../../data/dictionary_android/dictionary07.txt',
                     '../../data/dictionary_android/dictionary08.txt',
                     '../../data/dictionary_android/dictionary09.txt',
                     '../../data/dictionary_android/suggestion_filter.txt' ],
        },
      ],
    },
  ],
}
