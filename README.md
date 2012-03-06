Mozc Fcitx
==========

We use this repo to add fcitx support to mozc, due to release schedule and
some other reason, we decide not to make it upstream.

master branch is the origin mozc, and will sync mozc trunk with git svn.

All development will be under fcitx branch.

So git diff origin/master origin/fcitx will generate patch for distribution
packager.

There is some helper script for compile/install it yourself under scripts.
Please run those scripts under src/.
