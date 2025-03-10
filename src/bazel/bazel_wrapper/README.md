# bazel_wrapper

This directory can be used to place special wrapper scripts for bazelisk.

*   [tools/bazel - Bazelisk](https://github.com/bazelbuild/bazelisk/blob/master/README.md#toolsbazel)

By default `bazel` and `bazelisk` uses `tools/` as the bazel wrapper directory,
but `bazelisk` allows us to customize it with `BAZELISK_WRAPPER_DIRECTORY` in
[`src/.bazeliskrc`](../../.bazeliskrc) since bazelisk v1.21.0.

*   [Allow overriding tools/bazel path with BAZELISK_WRAPPER_DIRECTORY](https://github.com/bazelbuild/bazelisk/pull/567)

## Contents

*   [bazel.bat](./bazel.bat)
