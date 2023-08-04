# Contributing Guide

## Before you begin

### Sign our Contributor License Agreement

Contributions to this project must be accompanied by a
[Contributor License Agreement](https://cla.developers.google.com/about) (CLA).
You (or your employer) retain the copyright to your contribution; this simply
gives us permission to use and redistribute your contributions as part of the
project.

If you or your current employer have already signed the Google CLA (even if it
was for a different project), you probably don't need to do it again.

Visit <https://cla.developers.google.com/> to see your current agreements or to
sign a new one.

### Check locations of files and directories you plan to edit

**Pull requests to the Mozc project are limited to the specific directories.**

Files and directories we may accept pull requests:
* files in the [top directory](https://github.com/google/mozc/tree/master/)
* [.github/](https://github.com/google/mozc/tree/master/.github/)
* [docker/](https://github.com/google/mozc/tree/master/docker/)
* [docs/](https://github.com/google/mozc/tree/master/docs/)
* [src/.bazelrc](https://github.com/google/mozc/tree/master/src/.bazelrc)
* [src/bazel/](https://github.com/google/mozc/tree/master/src/bazel/)
* [src/data/oss/](https://github.com/google/mozc/tree/master/src/data/oss/)
* [src/data/test/quality_regression_test/](https://github.com/google/mozc/tree/master/src/data/test/quality_regression_test/)
* [src/renderer/qt/](https://github.com/google/mozc/tree/master/src/renderer/qt/)
* [src/unix/](https://github.com/google/mozc/tree/master/src/unix/)
* [src/WORKSPACE.bazel](https://github.com/google/mozc/tree/master/src/WORKSPAE.bazel)

#### Why is there such a limitation?

The limitation is due to the Google corporate policy that requires OSS code to
be place under the `third_party` directory in the Google internal source
repository [1]. For Mozc's case, files and directories that accept pull requests
need to be placed under `third_party/mozc` in the internal repository, which is
different from where the internal version of Mozc has been developed.

Unfortunately just moving files and directories becomes a quite complicated
project if it involves multiple build dependencies. The above files and
directories are where the migrations have been completed so far, and the Mozc
team is still working on other files and directories. This is why the above list
exists and keeps growing in an incremental manner.

[1]: [Accepting Contributions](https://opensource.google/documentation/reference/releasing/contributions#thirdparty)
> ### Patched code is third_party code
> Once external patches to a project have been accepted, that code is no longer
> exclusively copyrighted by Google. That means that, like any other open source
> software we use, it is subject to our
> [go/thirdparty](https://opensource.google/documentation/reference/thirdparty)
> policies and must be moved to a third_party directory. This means
> //third_party if stored in Piper.
