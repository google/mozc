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

"""Repository rule for Android build system.

If ANDROID_NDK_HOME env variable is set, android_ndk_repository is called.
Otherwise do nothing.

## Usage example in WORKSPACE.bazel
```
load("//:android_repository.bzl", "android_repository")
android_repository(name = "android_repository")
load("@android_repository//:setup.bzl", "android_ndk_setup")
android_ndk_setup()
```

## Reference
* https://github.com/google/mozc/issues/544
* https://github.com/bazelbuild/bazel/issues/14260
"""

NDK_SETUP_TEMPLATE = """
load("@rules_android_ndk//:rules.bzl", "android_ndk_repository")

def android_ndk_setup():
    {rule}
"""

def _android_repository_impl(repository_ctx):
    if repository_ctx.os.environ.get("ANDROID_NDK_HOME"):
        rule = "android_ndk_repository(name=\"androidndk\")"
    else:
        rule = "pass"
    repository_ctx.file("BUILD.bazel", "")
    repository_ctx.file("setup.bzl", NDK_SETUP_TEMPLATE.format(rule = rule))

android_repository = repository_rule(
    implementation = _android_repository_impl,
    configure = True,
    local = True,
    environ = ["ANDROID_NDK_HOME"],
)
