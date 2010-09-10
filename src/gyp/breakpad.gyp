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

# WARNING: This file will be copied from gyp to
# third_party/breakpad. Don't edit the copied version.
{
  'targets': [
    {
      'target_name': 'breakpad',
      'type': 'static_library',
      'conditions': [
        ['OS=="win"', {
          'include_dirs': [
            # Use the local glog configured for Windows.
            # See b/2954681 for details.
            'src/third_party/glog/glog/src/windows',
          ],
          'sources': [
            'src/client/windows/crash_generation/client_info.cc',
            'src/client/windows/crash_generation/crash_generation_client.cc',
            'src/client/windows/crash_generation/crash_generation_server.cc',
            'src/client/windows/crash_generation/minidump_generator.cc',
            'src/client/windows/handler/exception_handler.cc',
            'src/client/windows/sender/crash_report_sender.cc',
            'src/common/windows/guid_string.cc',
            'src/common/windows/http_upload.cc'
          ],
        }],
      ],
    },
  ],
}
