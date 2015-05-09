[Mozc - a Japanese Input Method Editor designed for multi-platform](http://github.com/google/mozc)
===================================

Copyright 2010-2015, Google Inc.

Mozc is a Japanese Input Method Editor (IME) designed for multi-platform such as
Android OS, Apple OS X, Chromium OS, GNU/Linux and Microsoft Windows.  This
open-source project originates from
[Google Japanese Input](http://www.google.com/intl/ja/ime/).

Build Status
------------

|         |Android |Windows |OS X |Linux |NaCl |
|---------|:------:|:------:|:---:|:----:|:---:|
|**Build**|N/A     |[![Build status](https://ci.appveyor.com/api/projects/status/qm7q355lenq5ogp6/branch/master?svg=true)](https://ci.appveyor.com/project/google/mozc/branch/master) |[![Build Status](https://travis-ci.org/google/mozc.svg?branch=master)](https://travis-ci.org/google/mozc) |N/A |N/A |


What's Mozc?
------------
For historical reasons, the project name *Mozc* has two different meanings:

1. Internal code name of Google Japanese Input that is still commonly used
   inside Google.
2. Project name to release a subset of Google Japanese Input in the form of
   source code under OSS license without any warranty nor user support.

In this repository, *Mozc* means the second definition unless otherwise noted.

License
-------

All Mozc code written by Google is released under
[The BSD 3-Clause License](http://opensource.org/licenses/BSD-3-Clause).
For thrid party code under [src/third_party](src/third_party) directory,
see each sub directory to find the copyright notice.  Note also that
outside [src/third_party](src/third_party) following directories contain
thrid party code.

###[src/data/dictionary_oss/](src/data/dictionary_oss)
Mixed.
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

###[src/data/test/dictionary/](src/data/test/dictionary)
The same to [src/data/dictionary_oss/](src/data/dictionary_oss).
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

###[src/data/test/stress_test/](src/data/test/stress_test)
Public Domain.  See the comment in
[src/data/test/stress_test/sentences.txt](src/data/test/stress_test/sentences.txt)

###[src/data/unicode/](src/data/unicode)
UNICODE, INC. LICENSE AGREEMENT.
See each file header for details.
