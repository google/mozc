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
  'conditions': [
    ['OS!="win"', {
      'targets': [
        {
          'target_name': 'mozc_win32_build32',
          'type': 'none',
        },
      ],
    }],
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'mozc_win32_build32',
          'type': 'none',
          'dependencies': [
            '../../renderer/renderer.gyp:mozc_renderer',
            '../../server/server.gyp:mozc_server',
            '../broker/broker.gyp:mozc_broker',
            '../cache_service/cache_service.gyp:mozc_cache_service',
            '../tip/tip.gyp:mozc_tip32',
          ],
          'conditions': [
            ['branding=="GoogleJapaneseInput"', {
              'dependencies': [
                '../custom_action/custom_action.gyp:mozc_custom_action32',
               ],
            }],
            ['use_qt!="YES"', {
              'dependencies': [
                '../../gui/gui.gyp:mozc_tool',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
