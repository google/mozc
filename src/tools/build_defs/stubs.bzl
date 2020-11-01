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

"""Stub build rules."""

def bzl_library(**kwargs):
    # Do nothing for OSS.
    _ignore = kwargs
    pass

def cc_embed_data(**kwargs):
    # Do nothing for OSS.
    _ignore = kwargs
    pass

def jspb_proto_library(**kwargs):
    # Do nothing for OSS.
    _ignore = kwargs
    pass

def portable_proto_library(name, proto_deps, deps = [], **kwargs):
    _ignore = [kwargs]
    native.cc_proto_library(name = name, deps = deps + proto_deps)

def py2and3_test(**kwargs):
    native.py_test(**kwargs)
    pass

def pytype_strict_library(**kwargs):
    native.py_library(**kwargs)
    pass

def pytype_strict_binary(test_lib = True, **kwargs):
    native.py_binary(**kwargs)
    pass
