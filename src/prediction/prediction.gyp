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
    'relative_dir': 'prediction',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'prediction',
      'type': 'static_library',
      'sources': [
        '<(proto_out_dir)/<(relative_dir)/user_history_predictor.pb.cc',
        'suggestion_filter.cc',
        'dictionary_predictor.cc',
        'predictor.cc',
        'user_history_predictor.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'genproto_prediction',
      ],
      'conditions': [['two_pass_build==0', {
        'dependencies': [
          'install_gen_suggestion_filter_main',
        ],
      }]],
      'actions': [
        {
          'action_name': 'gen_suggestion_filter_data',
          'variables': {
            'input_files': [
              '../data/dictionary/suggestion_filter.txt',
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'conditions': [['two_pass_build==0', {
            'inputs': [
              '<(mozc_build_tools_dir)/gen_suggestion_filter_main',
            ],
          }]],
          'outputs': [
            '<(gen_out_dir)/suggestion_filter_data.h',
          ],
          'action': [
            '<(mozc_build_tools_dir)/gen_suggestion_filter_main',
            '<@(input_files)',
            '<(gen_out_dir)/suggestion_filter_data.h',
            '--header',
            '--logtostderr',
          ],
        },
        {
          'action_name': 'gen_suggestion_feature_pos_group',
          'variables': {
            'input_files': [
              '../data/dictionary/id.def',
              '../data/rules/suggestion_feature_pos_group.def',
            ],
          },
          'inputs': [
            '../converter/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/suggestion_feature_pos_group.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/suggestion_feature_pos_group.h',
            '../converter/gen_pos_rewrite_rule.py',
            '<@(input_files)',
          ],
        },
        {
          'action_name': 'gen_svm_model',
          'variables': {
            'input_files': [
              '../data/prediction/svm_model.txt',
            ],
          },
          'inputs': [
            '../base/svmmodel2func.py',
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/svm_model.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/svm_model.h',
            '../base/svmmodel2func.py',
            '<@(input_files)',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_suggestion_filter_main',
      'type': 'executable',
      'sources': [
        'gen_suggestion_filter_main.cc',
      ],
      'dependencies': [
        '../storage/storage.gyp:storage',
      ],
    },
    {
      'target_name': 'install_gen_suggestion_filter_main',
      'type': 'none',
      'variables': {
        'bin_name': 'gen_suggestion_filter_main'
      },
      'includes' : [
        '../gyp/install_build_tool.gypi',
      ]
    },
    {
      'target_name': 'genproto_prediction',
      'type': 'none',
      'sources': [
        'user_history_predictor.proto',
      ],
      'includes': [
        '../protobuf/genproto.gypi',
      ],
    },
    {
      'target_name': 'prediction_test',
      'type': 'executable',
      'sources': [
        'suggestion_filter_test',
        'dictionary_predictor_test',
        'user_history_predictor_test',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'prediction',
      ]
    },
  ],
}
