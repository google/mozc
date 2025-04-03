// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_GUI_BASE_UTIL_H_
#define MOZC_GUI_BASE_UTIL_H_

#include <QApplication>
#include <QLabel>
#include <QString>
#include <memory>

#include "absl/strings/string_view.h"

namespace mozc {
namespace gui {

class GuiUtil {
 public:
  // Initializes the common Qt configuration such as High DPI, font, and theme.
  // The type of argc is a reference.
  static std::unique_ptr<QApplication> InitQt(int &argc, char *argv[]);

  // Installs the translation message.
  // Resource name is the prefix of Qt resource.
  // For instance, |resource_name| == "foo", foo_ja.qm or foo_en.qm is loaded.
  static void InstallTranslator(absl::string_view resource_name);

  // Returns the product name.
  static QString ProductName();

  // Replace placeholders in all labels under the widget.
  static void ReplaceWidgetLabels(QWidget *widget);

  // Replace placeholders in the label.
  static void ReplaceLabel(QLabel *label);

  // Replace placeholders in the widget.
  static void ReplaceTitle(QWidget *widget);

  // Replace placeholders in the string.
  static QString ReplaceString(const QString &str);

 private:
  GuiUtil() = delete;
  virtual ~GuiUtil() = delete;
};
}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_BASE_UTIL_H_
