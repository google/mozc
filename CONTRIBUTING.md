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
* [src/BUILD.(library).bazel](https://github.com/google/mozc/tree/master/src/)
* [src/data/oss/](https://github.com/google/mozc/tree/master/src/data/oss/)
* [src/data/test/quality_regression_test/](https://github.com/google/mozc/tree/master/src/data/test/quality_regression_test/)
* [src/renderer/qt/](https://github.com/google/mozc/tree/master/src/renderer/qt/)
* [src/unix/emacs/mozc.el](https://github.com/google/mozc/tree/master/src/unix/emacs/mozc.el)
* [src/WORKSPACE.bazel](https://github.com/google/mozc/tree/master/src/WORKSPAE.bazel)

Although Google company policy certainly allows Mozc team to accept pull
requests, to do so Mozc team needs to move all Mozc source files into
`third_party` directory in the Google internal source repository [1].
Doing that without breaking any Google internal project that depends on
Mozc source code requires non-trivial amount of time and engineering
resources that Mozc team cannot afford right now.

Mozc team continues to seek opportunities to address this limitation,
but we are still not ready to accept any pull request due to the above
reason.

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
