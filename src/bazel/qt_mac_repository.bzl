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

"""Repository rule for Qt for macOS."""

def _qt_mac_repository_impl(repo_ctx):
    is_mac = repo_ctx.os.name.lower().startswith("mac")
    if not is_mac:
        repo_ctx.file("BUILD.bazel", "")
        return

    qt_path = repo_ctx.path(repo_ctx.os.environ.get("MOZC_QT_PATH", repo_ctx.attr.default_path))
    repo_ctx.symlink(qt_path.get_child("bin"), "bin")
    repo_ctx.symlink(qt_path.get_child("lib"), "lib")
    repo_ctx.symlink(qt_path.get_child("plugins"), "plugins")
    repo_ctx.template("BUILD.bazel", repo_ctx.path(Label("@//bazel:BUILD.qt.bazel")))

qt_mac_repository = repository_rule(
    implementation = _qt_mac_repository_impl,
    environ = ["MOZC_QT_PATH"],
    attrs = {
        "default_path": attr.string(),
    },
)
