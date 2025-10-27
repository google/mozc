# How to build Mozc for Linux desktop

<!-- disableFinding(LINK_RELATIVE_G3DOC) -->

[![Linux](https://github.com/google/mozc/actions/workflows/linux.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/linux.yaml)

## Summary

If you are not sure what the following commands do, please check the
descriptions below and make sure the operations before running them.

```
git clone https://github.com/google/mozc.git --recursive
cd mozc/src

bazelisk build package --config oss_linux --config release_build
```

`bazel-bin/unix/mozc.zip` contains built files.

## System Requirements

Due to the diverse nature of Linux desktop ecosystem, continuous builds on
GitHub Actions are the best example on how Mozc executables for Linux desktop
can be built and tested against existing test cases.

*   [`.github/workflows/linux.yaml`](../.github/workflows/linux.yaml)
*   [CI for Linux](https://github.com/google/mozc/actions/workflows/linux.yaml)

The following sections describe relevant software components that are necessary
to build Mozc for Linux desktop.

### Bazelisk

[Bazelisk](https://github.com/bazelbuild/bazelisk) is a wrapper of
[Bazel](https://bazel.build) to use the specific version of Bazel.

The Bazel version specified in [`src/.bazeliskrc`](../src/.bazeliskrc) is what
continuous builds are testing against.

See the following document for detail on how Bazelisk determines the Bazel
version.

*   [How does Bazelisk know which Bazel version to run?](https://github.com/bazelbuild/bazelisk/blob/master/README.md#how-does-bazelisk-know-which-bazel-version-to-run)

⚠️ Bazel version mismatch is a major source of build failures. If you manually
install Bazel then use it instead of Bazelisk, pay extra attention to which
version of Bazel you are using.

### C++ toolchain

GCC or Clang is needed to build Mozc.

While Linux continuous builds currently use GCC, Mozc's C++ code is designed to
be compatible with Clang (for macOS, Windows, Android, and Google internal use)
and Visual C++ 2022 (for Windows GYP build, which is going to be deprecated) as
well.

💡 See [`.github/workflows/linux.yaml`](../.github/workflows/linux.yaml) on which
version of GCC is tested against.

💡 Like many other Bazel-based C++ projects, Mozc relies on
[`rules_cc`](https://github.com/bazelbuild/rules_cc/) specified in
[`MODULE.bazel`](../src/MODULE.bazel) to automatically detect C++ toolchains in
the host environment.

### Packages

Development packages referenced in `pkg_config_repository` at
[`src/MODULE.bazel`](../src/MODULE.bazel) need to be installed beforehand.

```
# iBus
pkg_config_repository(
    name = "ibus",
    packages = [
        "glib-2.0",
        "gobject-2.0",
        "ibus-1.0",
    ],
)
```

```
# Qt for Linux
pkg_config_repository(
    name = "qt_linux",
    packages = [
        "Qt6Core",
        "Qt6Gui",
        "Qt6Widgets",
    ],
)
```

💡 `pkg_config_repository` is not a bazel standard functionality. It is a custom
macro defined in
[`src/bazel/pkg_config_repository.bzl`](../src/bazel/pkg_config_repository.bzl).

## Build instructions

### Get the Code

You can download Mozc source code as follows.

```
git clone https://github.com/google/mozc.git --recursive
cd mozc/src
```

Hereafter you can do all the operations without changing directory.

### Build Mozc

You should be able to build Mozc for Linux desktop as follows, assuming
`bazelisk` is in your `$PATH`.

```
bazelisk build package --config oss_linux --config release_build
```

`package` is an alias to build Mozc executables and archive them into
`mozc.zip`.

### Clean Bazel's build cache

💡 You may have some build errors when you update the build environment or
configurations. Try the following command to
[clean Bazel's build cache](https://bazel.build/docs/user-manual#clean).

```
bazelisk clean --expunge
```

### How to customize installation locations

Here is a table of contents in `mozc.zip` and their actual build target names.

build target                     | installation location
-------------------------------- | ---------------------
`//server:mozc_server`           | `/usr/lib/mozc/mozc_server`
`//gui/tool:mozc_tool`           | `/usr/lib/mozc/mozc_tool`
`//renderer:mozc_renderer`       | `/usr/lib/mozc/mozc_renderer`
`//unix/ibus/ibus_mozc`          | `/usr/lib/ibus-mozc/ibus-engine-mozc`
`//unix/ibus:gen_mozc_xml`       | `/usr/share/ibus/component/mozc.xml`
`//unix:icons`                   | `/usr/share/ibus-mozc/...`
`//unix:icons`                   | `/usr/share/icons/mozc/...`
`//unix/emacs:mozc.el`           | `/usr/share/emacs/site-lisp/emacs-mozc/mozc.el`
`//unix/emacs:mozc_emacs_helper` | `/usr/bin/mozc_emacs_helper`

To customize above installation locations, modify
[`src/config.bzl`](../src/config.bzl).

💡 The following command makes the specified file untracked by Git.

```
git update-index --assume-unchanged src/config.bzl
```

💡 This command reverts the above change.

```
git update-index --no-assume-unchanged src/config.bzl
```

## Bazel command examples

### Bazel User Guide

*   [Build programs with Bazel](https://bazel.build/run/build)
*   [Commands and Options](https://bazel.build/docs/user-manual)
*   [Write bazelrc configuration files](https://bazel.build/run/bazelrc)

### Run all tests

```
bazelisk test ... --config oss_linux --build_tests_only -c dbg
```

*   `...` means all targets under the current and subdirectories.

### Run tests under the specific directories

```
bazelisk test base/... composer/... --config oss_linux --build_tests_only -c dbg
```

*   `<dir>/...` means all targets under the `<dir>/` directory.

### Run tests without the specific directories

```
bazelisk test ... --config oss_linux --build_tests_only -c dbg -- -base/...
```

*   `--` means the end of the flags which start from `-`.
*   `-<dir>/...` means exclusion of all targets under the `dir`.

### Run the specific test

```
bazelisk test base:util_test --config oss_linux -c dbg
```

*   `util_test` is defined in `base/BUILD.bazel`.

### Output logs to stderr

```
bazelisk test base:util_test --config oss_linux --test_arg=--stderrthreshold=0 --test_output=all
```

*   The `--test_arg=--stderrthreshold=0 --test_output=all` flags shows the
    output of unitests to stderr.

--------------------------------------------------------------------------------

## Build Mozc for Linux Desktop with GYP (deprecated):

⚠️ The GYP build is deprecated and no longer supported.

Please check the previous version for more information.
https://github.com/google/mozc/blob/2.29.5374.102/docs/build_mozc_in_docker.md#build-mozc-for-linux-desktop-with-gyp-maintenance-mode
