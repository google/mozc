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
        '<(absl_srcdir)/base/internal/strerror.cc',
        '<(absl_srcdir)/base/internal/sysinfo.cc',
        '<(absl_srcdir)/base/internal/thread_identity.cc',
        '<(absl_srcdir)/base/internal/throw_delegate.cc',
        '<(absl_srcdir)/base/internal/unscaledcycleclock.cc',
        '<(absl_srcdir)/base/log_severity.cc',
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
      'target_name': 'absl_debugging',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(absl_srcdir)/debugging/stacktrace.cc',
        '<(absl_srcdir)/debugging/symbolize.cc',
        '<(absl_srcdir)/debugging/internal/address_is_readable.cc',
        '<(absl_srcdir)/debugging/internal/demangle.cc',
        '<(absl_srcdir)/debugging/internal/elf_mem_image.cc',
        '<(absl_srcdir)/debugging/internal/vdso_support.cc',
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
        '<(absl_srcdir)/flags/commandlineflag.cc',
        '<(absl_srcdir)/flags/commandlineflag.h',
        '<(absl_srcdir)/flags/usage.cc',
        '<(absl_srcdir)/flags/usage.h',
        '<(absl_srcdir)/flags/flag.cc',
        '<(absl_srcdir)/flags/flag.h',
        '<(absl_srcdir)/flags/config.h',
        '<(absl_srcdir)/flags/declare.h',
        '<(absl_srcdir)/flags/marshalling.cc',
        '<(absl_srcdir)/flags/marshalling.h',
        '<(absl_srcdir)/flags/parse.cc',
        '<(absl_srcdir)/flags/parse.h',
        '<(absl_srcdir)/flags/reflection.cc',
        '<(absl_srcdir)/flags/reflection.h',
        '<(absl_srcdir)/flags/usage_config.cc',
        '<(absl_srcdir)/flags/usage_config.h',
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
        '<(absl_srcdir)/flags/internal/commandlineflag.cc',
        '<(absl_srcdir)/flags/internal/commandlineflag.h',
        '<(absl_srcdir)/flags/internal/flag.cc',
        '<(absl_srcdir)/flags/internal/flag.h',
        '<(absl_srcdir)/flags/internal/parse.h',
        '<(absl_srcdir)/flags/internal/path_util.h',
        '<(absl_srcdir)/flags/internal/private_handle_accessor.cc',
        '<(absl_srcdir)/flags/internal/private_handle_accessor.h',
        '<(absl_srcdir)/flags/internal/program_name.cc',
        '<(absl_srcdir)/flags/internal/program_name.h',
        '<(absl_srcdir)/flags/internal/registry.h',
        '<(absl_srcdir)/flags/internal/usage.cc',
        '<(absl_srcdir)/flags/internal/usage.h',
      ],
      'dependencies': [
        'absl_strings',
      ],
    },
    {
      'target_name': 'absl_hash_internal',
      'toolsets': ['host', 'target'],
      'type': 'static_library',
      'sources': [
        '<(absl_srcdir)/container/internal/raw_hash_set.cc',
        '<(absl_srcdir)/hash/internal/city.cc',
        '<(absl_srcdir)/hash/internal/hash.cc',
        '<(absl_srcdir)/hash/internal/low_level_hash.cc',
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
        '<(absl_srcdir)/random/discrete_distribution.cc',
        '<(absl_srcdir)/random/gaussian_distribution.cc',
        '<(absl_srcdir)/random/internal/chi_square.cc',
        '<(absl_srcdir)/random/internal/pool_urbg.cc',
        '<(absl_srcdir)/random/internal/randen.cc',
        '<(absl_srcdir)/random/internal/randen_detect.cc',
        '<(absl_srcdir)/random/internal/randen_hwaes.cc',
        '<(absl_srcdir)/random/internal/randen_round_keys.cc',
        '<(absl_srcdir)/random/internal/randen_slow.cc',
        '<(absl_srcdir)/random/internal/seed_material.cc',
        '<(absl_srcdir)/random/seed_gen_exception.cc',
        '<(absl_srcdir)/random/seed_sequences.cc',
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
        '<(absl_srcdir)/strings/internal/cord_internal.cc',
        '<(absl_srcdir)/strings/internal/cord_rep_btree.cc',
        '<(absl_srcdir)/strings/internal/cord_rep_btree_navigator.cc',
        '<(absl_srcdir)/strings/internal/cord_rep_btree_reader.cc',
        '<(absl_srcdir)/strings/internal/cord_rep_consume.cc',
        '<(absl_srcdir)/strings/internal/cord_rep_crc.cc',
        '<(absl_srcdir)/strings/internal/cord_rep_ring.cc',
        '<(absl_srcdir)/strings/internal/cordz_functions.cc',
        '<(absl_srcdir)/strings/internal/cordz_handle.cc',
        '<(absl_srcdir)/strings/internal/cordz_info.cc',
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
        '<(absl_srcdir)/strings/cord.cc',
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
      'target_name': 'absl_synchronization',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(absl_srcdir)/synchronization/barrier.cc',
        '<(absl_srcdir)/synchronization/blocking_counter.cc',
        '<(absl_srcdir)/synchronization/blocking_counter.cc',
        '<(absl_srcdir)/synchronization/internal/create_thread_identity.cc',
        '<(absl_srcdir)/synchronization/internal/create_thread_identity.cc',
        '<(absl_srcdir)/synchronization/internal/graphcycles.cc',
        '<(absl_srcdir)/synchronization/internal/graphcycles.cc',
        '<(absl_srcdir)/synchronization/internal/per_thread_sem.cc',
        '<(absl_srcdir)/synchronization/internal/waiter.cc',
        '<(absl_srcdir)/synchronization/mutex.cc',
        '<(absl_srcdir)/synchronization/notification.cc',
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
    {
      'target_name': 'absl_status',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        '<(absl_srcdir)/status/status.cc',
        '<(absl_srcdir)/status/status_payload_printer.cc',
        '<(absl_srcdir)/status/statusor.cc',
      ],
      'dependencies': [
        'absl_base',
        'absl_strings',
      ],
    },
  ],
}
