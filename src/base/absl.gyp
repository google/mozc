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
    'gen_absl_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/abseil-cpp/absl',
    'glob_absl': '<(glob) --notest --base <(absl_srcdir) --subdir',
  },
  'targets': [
    {
      'target_name': 'absl_base',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) base "**/*.cc"' +
        ' --exclude "**/*_benchmark.cc" "**/*_test*.cc")',
        '<(absl_srcdir)/profiling/internal/exponential_biased.cc',
      ],
      'dependencies': [
        'absl_hash_internal',
      ],
      'msvs_disabled_warnings': [
        # 'type' : forcing value to bool 'true' or 'false'
        # (performance warning)
        # http://msdn.microsoft.com/en-us/library/b6801kcy.aspx
        '4800',
      ],
    },
    {
      'target_name': 'absl_crc',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) crc "**/*.cc")',
      ],
    },
    {
      'target_name': 'absl_debugging',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) debugging "**/*.cc")',
      ],
      'dependencies': [
        'absl_base',
      ],
    },
    {
      'target_name': 'absl_flags',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) flags "*.cc")',
      ],
      'dependencies': [
        'absl_flags_internal',
        'absl_hash_internal',
        'absl_synchronization',
      ],
    },
    {
      'target_name': 'absl_flags_internal',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) flags/internal "*.cc" --exclude size_tester.cc)',
      ],
      'dependencies': [
        'absl_strings',
      ],
    },
    {
      'target_name': 'absl_container_internal',
      'toolsets': ['host', 'target'],
      'type': 'static_library',
      'sources': [
        '<!@(<(glob_absl) container/internal "*.cc")',
      ],
    },
    {
      'target_name': 'absl_hash_internal',
      'toolsets': ['host', 'target'],
      'type': 'static_library',
      'sources': [
        '<(absl_srcdir)/hash/internal/city.cc',
        '<(absl_srcdir)/hash/internal/hash.cc',
        '<(absl_srcdir)/hash/internal/low_level_hash.cc',
      ],
      'dependencies': [
          'absl_container_internal',
      ],
    },
    {
      'target_name': 'absl_hash_testing',
      'toolsets': ['host', 'target'],
      'type': 'static_library',
      'sources': [],
      'dependencies': [
          'absl_strings',
          'absl_types',
      ],
    },
    {
      'target_name': 'absl_log',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) log "*.cc")',
      ],
      'sources!': [
        '<(absl_srcdir)/log/scoped_mock_log.cc',
      ],
      'dependencies': [
        'absl_base',
        'absl_debugging',
        'absl_log_internal',
      ],
    },
    {
      'target_name': 'absl_log_internal',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) log/internal "*.cc")',
      ],
      'dependencies': [
        'absl_base',
        'absl_debugging',
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
      'target_name': 'absl_random',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) random "**/*.cc"' +
        ' --exclude "**/*benchmark*.cc" "**/*generator*.cc")',
      ],
      'sources!': [
        '<(absl_srcdir)/random/internal/gaussian_distribution_gentables.cc',
      ],
      'dependencies': [
        'absl_base',
      ],
    },
    {
      'target_name': 'absl_status',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) status "*.cc")',
        '<!@(<(glob_absl) status/internal "*.cc")',
      ],
      'include_dirs': [
        '<(gmock_dir)/include',
        '<(gtest_dir)/include',
      ],
      'dependencies': [
        'absl_base',
        'absl_strings',
      ],
    },
    {
      'target_name': 'absl_strings_internal',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) strings/internal "**/*.cc")',
      ],
      'sources!': [
        '<(absl_srcdir)/strings/internal/str_format/benchmarks.cc',
        '<(absl_srcdir)/strings/internal/str_format/extension_test_class.cc',
        '<(absl_srcdir)/strings/internal/str_format/parser_fuzzer.cc',
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
        '<!@(<(glob_absl) strings "*.cc")',
      ],
      'dependencies': [
        'absl_base',
        'absl_crc',
        'absl_numeric',
        'absl_strings_internal',
      ],
    },
    {
      'target_name': 'absl_synchronization',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) synchronization "**/*.cc")',
      ],
      'cflags': [
        '-Wno-error',
      ],
      'dependencies': [
        'absl_base',
        'absl_debugging',
        'absl_time',
        'absl_numeric'
      ],
    },
    {
      'target_name': 'absl_time',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) time "*.cc")',
        '<!@(<(glob_absl) time/internal/cctz/src "*.cc"' +
        ' --exclude time_tool.cc)',
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
    {
      'target_name': 'absl_types',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<!@(<(glob_absl) types "**/*.cc")',
      ],
      'cflags': [
        '-Wno-error',
      ],
    },
  ],
}
