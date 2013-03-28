# Copyright 2010-2013, Google Inc.
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
    'relative_dir': 'data_manager/oss',
    'relative_mozc_dir': '',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'gen_out_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_mozc_dir)',
    # The following variables are passed to ../data_manager.gypi.
    'current_dir': '.',
    'mozc_dir': '../..',
    'common_data_dir': '<(mozc_dir)/data',
    'platform_data_dir': '<(mozc_dir)/data/dictionary_oss',
    'boundary_def': '<(mozc_dir)/data/rules/boundary.def',
    'dataset_tag': 'oss',
    'dictionary_files': [
      '<(platform_data_dir)/dictionary00.txt',
      '<(platform_data_dir)/dictionary01.txt',
      '<(platform_data_dir)/dictionary02.txt',
      '<(platform_data_dir)/dictionary03.txt',
      '<(platform_data_dir)/dictionary04.txt',
      '<(platform_data_dir)/dictionary05.txt',
      '<(platform_data_dir)/dictionary06.txt',
      '<(platform_data_dir)/dictionary07.txt',
      '<(platform_data_dir)/dictionary08.txt',
      '<(platform_data_dir)/dictionary09.txt',
      '<(platform_data_dir)/reading_correction.tsv',
    ],
  },
  # This 'includes' defines the following targets:
  #   - oss_data_manager  (type: static_library)
  #   - gen_separate_dictionary_data_for_oss (type: none)
  #   - gen_separate_connection_data_for_oss (type: none)
  #   - gen_oss_embedded_data  (type: none)
  'includes': [ '../data_manager.gypi' ],
}
