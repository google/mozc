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
  'variables': {
    'absl_srcdir': '<(DEPTH)/third_party/abseil-cpp/absl',
    'gen_absl_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/abseil-cpp/absl',
  },
  'targets': [
    {
      'target_name': 'absl_base',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(absl_srcdir)/base/internal/cycleclock.cc',
        '<(absl_srcdir)/base/internal/low_level_alloc.cc',
        '<(absl_srcdir)/base/internal/raw_logging.cc',
        '<(absl_srcdir)/base/internal/spinlock.cc',
        '<(absl_srcdir)/base/internal/spinlock_wait.cc',
        '<(absl_srcdir)/base/internal/sysinfo.cc',
        '<(absl_srcdir)/base/internal/thread_identity.cc',
        '<(absl_srcdir)/base/internal/throw_delegate.cc',
        '<(absl_srcdir)/base/internal/unscaledcycleclock.cc',
        '<(absl_srcdir)/container/internal/raw_hash_set.cc',
        '<(absl_srcdir)/hash/internal/city.cc',
        '<(absl_srcdir)/hash/internal/hash.cc',
      ],
      'msvs_disabled_warnings': [
        # 'type' : forcing value to bool 'true' or 'false'
        # (performance warning)
        # http://msdn.microsoft.com/en-us/library/b6801kcy.aspx
        '4800',
      ],
    },
    {
      'target_name': 'absl_numeric',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(absl_srcdir)/numeric/int128.cc',
      ],
      'dependencies': [
        'absl_base',
      ],
    },
    {
      'target_name': 'absl_strings_internal',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(absl_srcdir)/strings/internal/charconv_bigint.cc',
        '<(absl_srcdir)/strings/internal/charconv_parse.cc',
        '<(absl_srcdir)/strings/internal/escaping.cc',
        '<(absl_srcdir)/strings/internal/memutil.cc',
        '<(absl_srcdir)/strings/internal/str_format/arg.cc',
        '<(absl_srcdir)/strings/internal/str_format/bind.cc',
        '<(absl_srcdir)/strings/internal/str_format/extension.cc',
        '<(absl_srcdir)/strings/internal/str_format/float_conversion.cc',
        '<(absl_srcdir)/strings/internal/str_format/output.cc',
        '<(absl_srcdir)/strings/internal/str_format/parser.cc',
        '<(absl_srcdir)/strings/internal/utf8.cc',
      ],
      'dependencies': [
        'absl_base',
        'absl_numeric',
      ],
    },
    {
      'target_name': 'absl_strings',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(absl_srcdir)/strings/ascii.cc',
        '<(absl_srcdir)/strings/charconv.cc',
        '<(absl_srcdir)/strings/escaping.cc',
        '<(absl_srcdir)/strings/match.cc',
        '<(absl_srcdir)/strings/numbers.cc',
        '<(absl_srcdir)/strings/str_cat.cc',
        '<(absl_srcdir)/strings/str_replace.cc',
        '<(absl_srcdir)/strings/str_split.cc',
        '<(absl_srcdir)/strings/string_view.cc',
        '<(absl_srcdir)/strings/substitute.cc',
      ],
      'dependencies': [
        'absl_base',
        'absl_numeric',
        'absl_strings_internal',
      ],
    },
    {
      'target_name': 'absl_time',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(absl_srcdir)/time/civil_time.cc',
        '<(absl_srcdir)/time/clock.cc',
        '<(absl_srcdir)/time/duration.cc',
        '<(absl_srcdir)/time/format.cc',
        '<(absl_srcdir)/time/time.cc',
        '<(absl_srcdir)/time/internal/cctz/src/civil_time_detail.cc',
        '<(absl_srcdir)/time/internal/cctz/src/time_zone_fixed.cc',
        '<(absl_srcdir)/time/internal/cctz/src/time_zone_format.cc',
        '<(absl_srcdir)/time/internal/cctz/src/time_zone_if.cc',
        '<(absl_srcdir)/time/internal/cctz/src/time_zone_impl.cc',
        '<(absl_srcdir)/time/internal/cctz/src/time_zone_info.cc',
        '<(absl_srcdir)/time/internal/cctz/src/time_zone_libc.cc',
        '<(absl_srcdir)/time/internal/cctz/src/time_zone_lookup.cc',
        '<(absl_srcdir)/time/internal/cctz/src/time_zone_posix.cc',
        '<(absl_srcdir)/time/internal/cctz/src/zone_info_source.cc',
      ],
      'cflags': [
        '-Wno-error',
      ],
      'dependencies': [
        'absl_base',
        'absl_numeric',
        'absl_strings_internal',
      ],
    },
  ],
}
