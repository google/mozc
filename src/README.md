# src/

Directory of the source code.

## Overview of components and directories

```
+------------+            +------------+            +------------+
| Server     | <--------> | Client     | <--------> | UI         |
+------------+  Protocol  +------------+  Protocol  +------------+
| Session    |            | Platform   |
+------------+            +------------+
| Converter  |
+------------+
| Dictionary |
+------------+
```

Note: Platform is used for various components for platform specific
requirements.

### Platform

*   [android/](android/)
    *   Android specific implementation.
    *   Note: Mozc does not contain the client UI implementation for Android.
*   [unix/](unix/)
    *   Linux specific implementation.
*   [win32/](win32/)
    *   Windows specific implementation.
*   [mac/](mac/)
    *   macOS specific implementation.
*   [ios/](ios/)
    *   iOS specific implementation.
    *   Note: Mozc does not officially support iOS build.

### Build

*   [build_tools/](build_tools/)
    *   Utilities used for build (e.g. code generation).
*   [bazel/](bazel/)
    *   Build rules and tools for Bazel build.
*   [gyp/](gyp/)
    *   Build rules and tools for GYP.
*   [protobuf/](protobuf/)
    *   Build rules and tools of GYP for Protocol Buffers.

### Base

*   [base/](base/)
    *   Fundamental libraries for generic purposes.
*   [data/](data/)
    *   Directory and subdirectories of data files.
*   [third_party/](third_party/)
    *   Third party libraries (e.g. Abseil).
*   [testing/](testing/)
    *   Libraries for testing.
*   [usage_stats/](usage_stats/)
    *   Libraries for usage statistics.
    *   Note: Mozc does not use usage statistics.

### Protocol

*   [protocol/](protocol/)
    *   Protocol definitions of function API (e.g. API between client and
        server).
*   [config/](config/)
    *   Libraries to manage user configurations.
*   [request/](request/)
    *   Libraries of request used for text conversion.

### Client / Server

*   [client/](client/)
    *   Libraries of client code.
    *   Note: The main function of the client is in each platform directory.
*   [server/](server/)
    *   Libraries of server code.
    *   The main function is in this directory.
*   [ipc/](ipc/)
    *   Libraries of IPC to communicate between client, server and other
        processes.

### Session

*   [session/](session/)
    *   Libraries of state management between user interactions and text
        conversion.
*   [composer/](composer/)
    *   Libraries of text composing (e.g. Romaji-Hiragana conversion).
*   [transliteration/](transliteration/)
    *   Libraries of script type transliteration (e.g. half-width Katakana,
        full-width Ascii).

### Converter

*   [engine/](engine/)
    *   Libraries to manage converter libraries.
*   [data_manager/](data_manager/)
    *   Libraries to manage converter data.
*   [converter/](converter/)
    *   Libraries of text conversion (e.g. Hiragana to Kanji exact match
        conversion).
*   [rewriter/](rewriter/)
    *   Libraries for additional conversions (e.g. date input, symbol input, zip
        code input).
*   [prediction/](prediction/)
    *   Libraries of text prediction (e.g. Hiragana to Kanji prefix match
        conversion).

### Dictionary

*   [dictionary/](dictionary/)
    *   Libraries of dictionary lookup (e.g. Hiragana key to possible words).
*   [storage/](storage/)
    *   Libraries of dictionary data storage (e.g. LOUDS, etc.).

### UI

*   [renderer/](renderer/)
    *   UI application of candidate words.
*   [gui/](gui/)
    *   GUI applications for configurations.
