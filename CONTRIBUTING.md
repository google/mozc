Contributing Guide
==================

## Pull Requests
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
* [src/unix/emacs/mozc.el](https://github.com/google/mozc/tree/master/src/unix/emacs/mozc.el)
* [src/unix/ibus/](https://github.com/google/mozc/tree/master/src/unix/ibus/)
* [src/WORKSPACE.bazel](https://github.com/google/mozc/tree/master/src/WORKSPAE.bazel)

### Why is there such a limitation?

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

[1]: [Open Source at Google - Linuxcon 2016](http://events.linuxfoundation.org/sites/events/files/slides/OSS_at_Google.pdf#page=30)
> ### License Compliance
> - We store all external open source code in a third_party hierarchy,
> along with the licenses for each project. We only allow the use of OSS
> under licenses we can comply with.
> - Use of external open source is not allowed unless it is put in that
> third_party tree.
> - This makes it easier to ensure we are only using software with
licenses that we can abide.
> - This also allows us to generate a list of all licenses used by any
project we build when they are released externally.
