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

# dictionary_base.gyp defines targets for lower layers to link to the dictionary
# modules, so modules in lower layers do not depend on ones in higher layers,
# avoiding circular dependencies.
{
  'variables': {
    'relative_dir': 'dictionary',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'text_dictionary_loader',
      'type': 'static_library',
      'sources': [
        'dictionary_token.h',
        'text_dictionary_loader.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'gen_pos_matcher',
      ],
    },
    {
      'target_name': 'gen_pos_matcher',
      'type': 'none',
      'actions': [
        {
          'action_name': 'gen_pos_matcher',
          'variables': {
            'input_files': [
              '../data/dictionary/id.def',
              '../data/rules/special_pos.def',
              '../data/rules/pos_matcher_rule.def',
            ],
          },
          'inputs': [
            'gen_pos_matcher_code.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/pos_matcher.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/pos_matcher.h',
            'gen_pos_matcher_code.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/pos_matcher.h.',
        },
      ],
    },
    {
      'target_name': 'install_dictionary_test_data',
      'type': 'none',
      'variables': {
        'test_data_subdir': 'data/dictionary',
        'test_data': [
          '../<(test_data_subdir)/suggestion_filter.txt',
          '../<(test_data_subdir)/dictionary00.txt',
          '../<(test_data_subdir)/dictionary01.txt',
          '../<(test_data_subdir)/dictionary02.txt',
          '../<(test_data_subdir)/dictionary03.txt',
          '../<(test_data_subdir)/dictionary04.txt',
          '../<(test_data_subdir)/dictionary05.txt',
          '../<(test_data_subdir)/dictionary06.txt',
          '../<(test_data_subdir)/dictionary07.txt',
          '../<(test_data_subdir)/dictionary08.txt',
          '../<(test_data_subdir)/dictionary09.txt',
        ],
      },
      'includes': [ '../gyp/install_testdata.gypi' ],
    },
  ],
}
