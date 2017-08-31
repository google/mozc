#!/bin/sh
find unix/fcitx5  -name '*.h' -o -name '*.cc'  | xargs clang-format -style=Google -i
