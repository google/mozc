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

{
  'targets': [
    {
      'target_name': 'unittests',
      'type': 'none',
      'dependencies': [
        "../base/base_test.gyp:base_all_test",
        "../client/client_test.gyp:client_all_test",
        "../config/config_test.gyp:config_all_test",
        "../composer/composer.gyp:composer_all_test",
        "../converter/converter_test.gyp:converter_all_test",
        "../dictionary/dictionary_test.gyp:dictionary_all_test",
        # "../gui/gui.gyp:gui_all_test",
        "../ipc/ipc.gyp:ipc_all_test",
        # "../net/net.gyp:net_all_test",
        "../prediction/prediction.gyp:prediction_all_test",
        "../rewriter/rewriter_test.gyp:rewriter_all_test",
        # "../server/server.gyp:server_all_test",
        "../session/session_test.gyp:session_all_test",
        "../storage/storage.gyp:storage_all_test",
        "../transliteration/transliteration.gyp:transliteration_all_test",
        "../usage_stats/usage_stats.gyp:usage_stats_all_test",
      ],
    },
  ],
}
