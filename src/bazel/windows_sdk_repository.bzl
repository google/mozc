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

"""Repository rule for Windows SDK."""

_GET_WINSDK_INCLUDE_DIRS_TEMPLATE = """
def get_winsdk_include_dirs():
    return [
        r"{include_um}",
        r"{include_shared}",
    ]
"""

def _get_path_env_var(repository_ctx, name):
    """Returns a path from an environment variable with removing quotes."""

    value = repository_ctx.getenv(name)
    if value == None:
        return None

    if value[0] == "\"" and len(value) > 1 and value[-1] != "\"":
        value = value[1:-1]
    if "/" in value:
        value = value.replace("/", "\\")
    if value[-1] == "\\":
        value = value.rstrip("\\")
    return value

def _read_reg_key(repository_ctx, reg_binary, reg_key, reg_value):
    """Reads a Win32 registory value."""

    result = repository_ctx.execute([reg_binary, "query", reg_key, "/v", reg_value])
    if result.stderr:
        return None
    for line in result.stdout.split("\n"):
        line = line.strip()
        if not line.startswith(reg_value):
            continue
        if line.find("REG_SZ") == -1:
            continue
        return line[line.find("REG_SZ") + len("REG_SZ"):].strip()
    return None

def _find_winsdk_path_and_ver(repository_ctx):
    """Find the Windows SDK path and its latest version."""

    reg_binary = _get_path_env_var(repository_ctx, "SYSTEMROOT") + r"\system32\reg.exe"
    reg_roots = [
        r"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node",
        r"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node",
        r"HKEY_LOCAL_MACHINE\SOFTWARE",
        r"HKEY_CURRENT_USER\SOFTWARE",
    ]

    for reg_root in reg_roots:
        # The following registry key has not been updated since Windows 10.
        # Hopefully we can keep using it for a while.
        reg_key = reg_root + r"\Microsoft\Microsoft SDKs\Windows\v10.0"
        winsdk_dir = _read_reg_key(repository_ctx, reg_binary, reg_key, "InstallationFolder")
        if not winsdk_dir:
            continue
        winsdk_ver_base = _read_reg_key(repository_ctx, reg_binary, reg_key, "ProductVersion")
        if not winsdk_ver_base:
            continue
        for rev in range(9, -1, -1):
            winsdk_ver = winsdk_ver_base + "." + str(rev)
            if repository_ctx.path(winsdk_dir + "/bin/" + winsdk_ver).exists:
                return (winsdk_dir, winsdk_ver)
    return (None, None)

def _windows_sdk_impl(repo_ctx):
    is_windows = repo_ctx.os.name.lower().startswith("win")
    if not is_windows:
        repo_ctx.file("BUILD.bazel", "")
        repo_ctx.template(
            "windows_sdk_rules.bzl",
            repo_ctx.path(Label("@//bazel:windows_sdk_rules.noop.template.bzl")),
            executable = False,
        )
        return

    winsdk_dir, winsdk_ver = _find_winsdk_path_and_ver(repo_ctx)
    if winsdk_dir == None or winsdk_ver == None:
        repo_ctx.file("BUILD.bazel", "")
        repo_ctx.template(
            "windows_sdk_rules.bzl",
            repo_ctx.path(Label("@//bazel:windows_sdk_rules.noop.template.bzl")),
            executable = False,
        )
        return

    # Hopefully "x86" and "arm64" should just work, but we need to check later.
    arch = repo_ctx.os.arch
    if arch == "amd64" or arch == "x86_64":
        arch = "x64"

    winsdk_path = repo_ctx.path(winsdk_dir)
    repo_ctx.symlink(winsdk_path.get_child("bin/" + winsdk_ver + "/" + arch), "bin")
    repo_ctx.symlink(winsdk_path.get_child("include/" + winsdk_ver), "include")
    repo_ctx.symlink(winsdk_path.get_child("lib/" + winsdk_ver), "lib")
    repo_ctx.template(
        "BUILD.bazel",
        repo_ctx.path(Label("@//bazel:BUILD.winsdk.bazel")),
        executable = False,
    )

    get_winsdk_include_dirs = _GET_WINSDK_INCLUDE_DIRS_TEMPLATE.format(
        include_um = repo_ctx.path("include/um"),
        include_shared = repo_ctx.path("include/shared"),
    )
    repo_ctx.file("get_winsdk_include_dirs.bzl", get_winsdk_include_dirs)

    repo_ctx.template(
        "windows_sdk_rules.bzl",
        repo_ctx.path(Label("@//bazel:windows_sdk_rules.win32.template.bzl")),
        executable = False,
    )

windows_sdk_repository = repository_rule(
    implementation = _windows_sdk_impl,
    configure = True,
    local = True,
    environ = ["SYSTEMROOT"],
)
