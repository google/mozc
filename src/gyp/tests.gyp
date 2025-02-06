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
  'targets': [
    {
      'target_name': 'unittests',
      'type': 'none',
      'dependencies': [
        '<(mozc_oss_src_dir)/base/base_test.gyp:base_all_test',
        '<(mozc_oss_src_dir)/client/client_test.gyp:client_all_test',
        '<(mozc_oss_src_dir)/config/config_test.gyp:config_all_test',
        '<(mozc_oss_src_dir)/composer/composer_test.gyp:composer_all_test',
        '<(mozc_oss_src_dir)/converter/converter_test.gyp:converter_all_test',
        '<(mozc_oss_src_dir)/dictionary/dictionary_test.gyp:dictionary_all_test',
        '<(mozc_oss_src_dir)/dictionary/file/dictionary_file_test.gyp:dictionary_file_all_test',
        '<(mozc_oss_src_dir)/dictionary/system/system_dictionary_test.gyp:system_dictionary_all_test',
        '<(mozc_oss_src_dir)/engine/engine_test.gyp:engine_all_test',
        '<(mozc_oss_src_dir)/gui/gui.gyp:gui_all_test',
        '<(mozc_oss_src_dir)/ipc/ipc.gyp:ipc_all_test',
        '<(mozc_oss_src_dir)/prediction/prediction_test.gyp:prediction_all_test',
        '<(mozc_oss_src_dir)/renderer/renderer.gyp:renderer_all_test',
        '<(mozc_oss_src_dir)/rewriter/rewriter_test.gyp:rewriter_all_test',
        # Currently 'server_all_test' does not exist.
        # '<(mozc_oss_src_dir)/server/server.gyp:server_all_test',
        '<(mozc_oss_src_dir)/session/session_test.gyp:session_all_test',
        '<(mozc_oss_src_dir)/storage/louds/louds_test.gyp:storage_louds_all_test',
        '<(mozc_oss_src_dir)/storage/storage_test.gyp:storage_all_test',
        '<(mozc_oss_src_dir)/transliteration/transliteration_test.gyp:transliteration_all_test',
      ],
      'conditions': [
        ['target_platform=="Windows"', {
          'dependencies': [
            '<(mozc_oss_src_dir)/base/win32/base_win32.gyp:base_win32_all_test',
            '<(mozc_oss_src_dir)/win32/base/win32_base.gyp:win32_base_all_test',
            '<(mozc_oss_src_dir)/win32/tip/tip.gyp:tip_all_test',
          ],
        }],
        ['target_platform=="Mac"', {
          'dependencies': [
            '<(mozc_oss_src_dir)/mac/mac.gyp:mac_all_test',
          ],
        }],
        ['target_platform=="Linux"', {
          'dependencies': [
            '<(mozc_oss_src_dir)/unix/emacs/emacs.gyp:emacs_all_test',
          ],
        }],
      ],
    },
  ],
}
