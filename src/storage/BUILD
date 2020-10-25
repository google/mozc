# -*- coding: utf-8 -*-
# Copyright 2010-2020, Google Inc.
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

# Visibility:  please choose a more appropriate default for the package,
# and update any rules that should be different.

load(
    "//:build_defs.bzl",
    "cc_library_mozc",
    "cc_test_mozc",
)

package(default_visibility = ["//:__subpackages__"])

test_suite(
    name = "nacl_test",
)

cc_library_mozc(
    name = "lru_storage",
    srcs = ["lru_storage.cc"],
    hdrs = ["lru_storage.h"],
    deps = [
        "//base",
        "//base:clock",
        "//base:file_stream",
        "//base:file_util",
        "//base:hash",
        "//base:logging",
        "//base:mmap",
        "//base:mozc_hash_map",
        "//base:mozc_hash_set",
        "//base:port",
        "//base:util",
        "@com_google_absl//absl/strings",
    ],
)

##    Commented out on 2011-03-09, because no other rule depends on it.
## cc_binary_mozc(name = "lru_storage_main",
##           srcs = [ "lru_storage_main.cc" ],
##           deps = [ ":lru_storage",
##                    "//base:base",])

cc_library_mozc(
    name = "lru_cache",
    hdrs = ["lru_cache.h"],
    deps = [
        "//base:logging",
        "//base:port",
    ],
)

##    Commented out on 2011-03-09, because no other rule depends on it.
## cc_binary_mozc(name = "lru_cache_main",
##           srcs = ["lru_cache_main.cc", ],
##           deps = [ "//base:base" ]);

cc_library_mozc(
    name = "existence_filter",
    srcs = ["existence_filter.cc"],
    hdrs = ["existence_filter.h"],
    deps = [
        "//base",
        "//base:logging",
        "//base:port",
    ],
)

##    Commented out on 2011-03-09, because no other rule depends on it.
## cc_binary_mozc(name = "existence_filter_main",
##           srcs = [ "existence_filter_main.cc" ],
##           deps = [ ":existence_filter",
##                    "//base:base",])

cc_test_mozc(
    name = "lru_storage_test",
    size = "small",
    srcs = ["lru_storage_test.cc"],
    deps = [
        ":lru_cache",
        ":lru_storage",
        "//base",
        "//base:clock_mock",
        "//base:file_stream",
        "//base:file_util",
        "//base:logging",
        "//base:port",
        "//base:util",
        "//testing:gunit_main",
    ],
)

cc_test_mozc(
    name = "existence_filter_test",
    size = "small",
    srcs = ["existence_filter_test.cc"],
    deps = [
        ":existence_filter",
        "//base",
        "//base:hash",
        "//base:logging",
        "//base:port",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "storage_interaface",
    hdrs = ["storage_interface.h"],
    deps = ["//base:port"],
)

cc_library_mozc(
    name = "tiny_storage",
    srcs = ["tiny_storage.cc"],
    hdrs = ["tiny_storage.h"],
    deps = [
        ":storage_interaface",
        "//base",
        "//base:file_stream",
        "//base:file_util",
        "//base:logging",
        "//base:mmap",
        "//base:port",
    ],
)

cc_test_mozc(
    name = "tiny_storage_test",
    size = "small",
    srcs = ["tiny_storage_test.cc"],
    deps = [
        ":storage_interaface",
        ":tiny_storage",
        "//base",
        "//base:file_util",
        "//base:port",
        "//testing:gunit_main",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library_mozc(
    name = "memory_storage",
    srcs = ["memory_storage.cc"],
    hdrs = ["memory_storage.h"],
    deps = [
        ":storage_interaface",
        "//base",
        "//base:logging",
    ],
)

cc_test_mozc(
    name = "memory_storage_test",
    size = "small",
    srcs = ["memory_storage_test.cc"],
    deps = [
        ":memory_storage",
        "//base",
        "//base:port",
        "//testing:gunit_main",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library_mozc(
    name = "registry",
    srcs = ["registry.cc"],
    hdrs = ["registry.h"],
    deps = [
        ":storage_interaface",
        ":tiny_storage",
        "//base",
        "//base:file_util",
        "//base:logging",
        "//base:mutex",
        "//base:port",
        "//base:singleton",
        "//base:system_util",
    ],
)

cc_test_mozc(
    name = "registry_test",
    size = "small",
    srcs = ["registry_test.cc"],
    deps = [
        ":registry",
        "//base",
        "//base:system_util",
        "//testing:gunit_main",
    ],
)

cc_library_mozc(
    name = "encrypted_string_storage",
    srcs = ["encrypted_string_storage.cc"],
    hdrs = ["encrypted_string_storage.h"],
    deps = [
        "//base",
        "//base:encryptor",
        "//base:file_stream",
        "//base:file_util",
        "//base:logging",
        "//base:mmap",
        "//base:port",
        "//base:util",
    ],
)

cc_test_mozc(
    name = "encrypted_string_storage_test",
    size = "small",
    srcs = ["encrypted_string_storage_test.cc"],
    deps = [
        ":encrypted_string_storage",
        "//base:file_stream",
        "//base:file_util",
        "//base:logging",
        "//base:system_util",
        "//testing:gunit_main",
    ],
)
