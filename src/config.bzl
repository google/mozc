# Copyright 2010-2021, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Tips: the following git command makes git assume this file unchanged.
# % git update-index --assume-unchanged config.bzl
#
# The following command reverts it.
# % git update-index --no-assume-unchanged config.bzl

BRANDING = "Mozc"

BAZEL_TOOLS_PREFIX = "@bazel_tools"

LINUX_MOZC_BROWSER_COMMAND = "/usr/bin/xdg-open"
LINUX_MOZC_ICONS_DIR = "/usr/share/icons/mozc"
LINUX_MOZC_SERVER_DIR = "/usr/lib/mozc"
LINUX_MOZC_DOCUMENT_DIR = LINUX_MOZC_SERVER_DIR + "/documents"
IBUS_COMPONENT_DIR = "/usr/share/ibus/component"
IBUS_MOZC_INSTALL_DIR = "/usr/share/ibus-mozc"
IBUS_MOZC_ICON_PATH = IBUS_MOZC_INSTALL_DIR + "/product_icon.png"
IBUS_MOZC_PATH = "/usr/lib/ibus-mozc/ibus-engine-mozc"
EMACS_MOZC_CLIENT_DIR = "/usr/share/emacs/site-lisp/emacs-mozc"
EMACS_MOZC_HELPER_DIR = "/usr/bin"

MACOS_BUNDLE_ID_PREFIX = "org.mozc.inputmethod.Japanese"
MACOS_MIN_OS_VER = "12.0"

# identity for codesign. The value passed to the --sign option of the codesign command.
#
# pseudo identity for local development
# '-' means pseudo identity.
# https://github.com/bazelbuild/rules_apple/blob/3.5.1/apple/internal/codesigning_support.bzl#L42
# https://developer.apple.com/documentation/security/seccodesignatureflags/1397793-adhoc
MACOS_CODESIGN_IDENTITY_PSEUDO = "-"
MACOS_CODESIGN_IDENTITY = MACOS_CODESIGN_IDENTITY_PSEUDO

## Qt path for macOS
# The paths are the default paths of Qt 6.5.2 installed by "make install".
#
# If MOZC_QT_PATH env var is specified, it is used for MACOS_QT_PATH instead.
#
# For Linux, Qt paths are managed by pkg_config_repository in WORKSPACE.bazel.
MACOS_QT_PATH = "/usr/local/Qt-6.5.2"

## SHA256 of zip code files.
# When zip code files are prefetched and used via --repository_cache for offline build,
# SHA256 values should be specified.
SHA256_ZIP_CODE_KEN_ALL = None
SHA256_ZIP_CODE_JIGYOSYO = None
