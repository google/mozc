# renderer/

`renderer/` is the directory for the candidate window GUI process.

## Overview

The candidate window GUI process receives commands from the Mozc clients via
IPC. The commands are defined in
[`protocol/candidates.proto`](../protocol/candidates.proto).

## Locations of main function per platform

*   Linux (Qt): [`qt/qt_renderer_main.cc`](qt/qt_renderer_main.cc)
*   macOS: [`mac/mac_renderer_main.cc`](mac/mac_renderer_main.cc)
*   Windows: [`win32/win32_renderer_main.cc`](win32/win32_renderer_main.cc)
