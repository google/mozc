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

"""Repository rule for Linux libraries configured by `pkg-config`.

Note, this rule supports only necessary functionaries of pkg-config for Mozc.
Generated `BUILD.bazel` is available at `bazel-src/external/<repository_name>`.

## Example of usage
```:WORKSPACE.bazel
pkg_config_repository(
  name = "ibus",
  packages = ["glib-2.0", "gobject-2.0", "ibus-1.0"],
)
```

```:BUILD.bazel
cc_library(
    name = "ibus_client",
    deps = [
        "@ibus//:ibus",
        ...
    ],
    ...
)
```
"""

BUILD_TEMPLATE = """
package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "{name}",
    hdrs = glob([
        {hdrs}
    ]),
    copts = [
        {copts}
    ],
    includes = [
        {includes}
    ],
    linkopts = [
        {linkopts}
    ],
)
"""

EXPORTS_FILES_TEMPLATE = """
exports_files(glob(["libexec/*"], allow_empty=True))
"""

def _exec_pkg_config(repo_ctx, flags):
    binary = repo_ctx.which("pkg-config")
    if not binary:
        # Using print is not recommended, but this will be a clue to debug build errors in
        # the case of pkg-config is not found.
        print("pkg-config is not found")  # buildifier: disable=print
        return []
    result = repo_ctx.execute([binary] + flags + repo_ctx.attr.packages)
    items = result.stdout.strip().split(" ")
    uniq_items = sorted({key: None for key in items}.keys())
    return uniq_items

def _get_possible_pc_files(repo_ctx):
    """
    Get all possible .pc file paths for the given packages in pkg-config search
    paths.
    """
    binary = repo_ctx.which("pkg-config")
    if not binary:
        return []

    pc_path_sep = ":"
    dir_sep = "/"
    if repo_ctx.os.name.lower().startswith("win"):
        pc_path_sep = ";"
        dir_sep = "\\"

    pc_file_search_paths = []

    pc_file_search_paths_env = repo_ctx.getenv("PKG_CONFIG_PATH")
    if pc_file_search_paths_env:
        pc_file_search_paths.extend(pc_file_search_paths_env.split(pc_path_sep))

    result = repo_ctx.execute([binary, "--variable=pc_path", "pkg-config"])
    if result.return_code == 0:
        pc_path = result.stdout.strip()
        pc_file_search_paths.extend(pc_path.split(pc_path_sep))

    pc_files = []
    for path in pc_file_search_paths:
        if not path:
            continue
        for package in repo_ctx.attr.packages:
            pc_file = path + dir_sep + package + ".pc"
            pc_files.append(pc_file)
    return pc_files

def _make_strlist(list):
    return "\"" + "\",\n        \"".join(list) + "\""

def _symlinks(repo_ctx, paths):
    for path in paths:
        if repo_ctx.path(path).exists:
            continue
        repo_ctx.symlink("/" + path, path)

def _pkg_config_repository_impl(repo_ctx):
    # Register all possible .pc files for watching to trigger repository
    # reevaluation.
    # This includes files that don't exist yet but might be created later
    # (e.g., when a new package is installed).
    pc_files = _get_possible_pc_files(repo_ctx)
    for pc_file in pc_files:
        repo_ctx.watch(pc_file)

    includes = _exec_pkg_config(repo_ctx, ["--cflags-only-I"])

    # If includes is empty, pkg-config will be re-executed with
    # the --keep-system-cflags option added. Typically, -I/usr/include is
    # returned, enabling bazel to recognize packages as valid even when
    # pkg-config does not output cflags with standard options.
    if includes[0] == "":
        includes = _exec_pkg_config(repo_ctx, ["--cflags-only-I", "--keep-system-cflags"])
    includes = [item[len("-I/"):] for item in includes]
    _symlinks(repo_ctx, includes)
    data = {
        # In bzlmod, repo_ctx.attr.name has a prefix like "_main~_repo_rules~ibus".
        # Note also that Bazel 8.0+ uses "+" instead of "~".
        # https://github.com/bazelbuild/bazel/issues/23127
        "name": repo_ctx.attr.name.replace("~", "+").split("+")[-1],
        "hdrs": _make_strlist([item + "/**" for item in includes]),
        "copts": _make_strlist(_exec_pkg_config(repo_ctx, ["--cflags-only-other"])),
        "includes": _make_strlist(includes),
        "linkopts": _make_strlist(_exec_pkg_config(repo_ctx, ["--libs-only-l"])),
    }
    build_file_data = BUILD_TEMPLATE.format(**data)

    libexecdir = _exec_pkg_config(repo_ctx, ["--variable=libexecdir"])
    if len(libexecdir) == 1:
        repo_ctx.symlink(libexecdir[0], "libexec")
        build_file_data += EXPORTS_FILES_TEMPLATE

    repo_ctx.file("BUILD.bazel", build_file_data)

pkg_config_repository = repository_rule(
    implementation = _pkg_config_repository_impl,
    configure = True,
    local = True,
    attrs = {
        "packages": attr.string_list(),
    },
)
