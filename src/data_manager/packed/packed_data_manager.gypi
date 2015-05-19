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
  'targets': [
    {
      'target_name': 'gen_gen_packed_data_main_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_gen_packed_data_main_<(dataset_tag)',
          'inputs': [
            'gen_packed_data_main_template.cc',
          ],
          'outputs': [
            '<(gen_out_dir)/gen_packed_data_main_<(dataset_tag).cc',
          ],
          'action': [
            'python', '../../build_tools/replace_macros.py',
            '--input=gen_packed_data_main_template.cc',
            '--output=<(gen_out_dir)/gen_packed_data_main_<(dataset_tag).cc',
            '--define=dir=<(dataset_dir)',
          ],
        },
      ],
    },
    {
      'target_name': 'gen_packed_data_main_<(dataset_tag)',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        '<(gen_out_dir)/gen_packed_data_main_<(dataset_tag).cc',
      ],
      'dependencies': [
        'gen_gen_packed_data_main_<(dataset_tag)',
        'packed_data_manager_base.gyp:system_dictionary_data_packer',
        'packed_data_manager_base.gyp:system_dictionary_data_protocol',
        '../../base/base.gyp:base',
        '../../dictionary/dictionary_base.gyp:pos_matcher',
        '../<(dataset_dir)/<(dataset_tag)_data_manager.gyp:gen_<(dataset_tag)_embedded_data',
      ],
    },
    {
      'target_name': 'gen_packed_data_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_packed_data_<(dataset_tag)',
          'inputs': [
            '<(PRODUCT_DIR)/gen_packed_data_main_<(dataset_tag)<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(gen_out_dir)/packed_data_<(dataset_tag)',
          ],
          'action': [
            '<(PRODUCT_DIR)/gen_packed_data_main_<(dataset_tag)<(EXECUTABLE_SUFFIX)',
            '--output=<(gen_out_dir)/packed_data_<(dataset_tag)',
            '--logtostderr',
          ],
        },
      ],
      'dependencies': [
        'gen_packed_data_main_<(dataset_tag)',
      ],
    },
    {
      'target_name': 'gen_packed_data_header_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_packed_data_<(dataset_tag)',
          'inputs': [
            '<(PRODUCT_DIR)/gen_packed_data_main_<(dataset_tag)<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(gen_out_dir)/packed_data_<(dataset_tag).h',
          ],
          'action': [
            '<(PRODUCT_DIR)/gen_packed_data_main_<(dataset_tag)<(EXECUTABLE_SUFFIX)',
            '--output=<(gen_out_dir)/packed_data_<(dataset_tag).h',
            '--logtostderr',
            '--make_header',
          ],
        },
      ],
      'dependencies': [
        'gen_packed_data_main_<(dataset_tag)',
      ],
    },
    {
      'target_name': 'gen_zipped_data_<(dataset_tag)',
      'type': 'none',
      'toolsets': ['host'],
      'actions': [
        {
          'action_name': 'gen_packed_data_<(dataset_tag)',
          'inputs': [
            '<(PRODUCT_DIR)/gen_packed_data_main_<(dataset_tag)<(EXECUTABLE_SUFFIX)',
          ],
          'outputs': [
            '<(gen_out_dir)/zipped_data_<(dataset_tag)',
          ],
          'action': [
            '<(PRODUCT_DIR)/gen_packed_data_main_<(dataset_tag)<(EXECUTABLE_SUFFIX)',
            '--output=<(gen_out_dir)/zipped_data_<(dataset_tag)',
            '--logtostderr',
            '--use_gzip',
          ],
        },
      ],
      'dependencies': [
        'gen_packed_data_main_<(dataset_tag)',
      ],
    },
  ],
}
