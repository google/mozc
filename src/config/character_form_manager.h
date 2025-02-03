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

#ifndef MOZC_CONFIG_CHARACTER_FORM_MANAGER_H_
#define MOZC_CONFIG_CHARACTER_FORM_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "base/number_util.h"
#include "base/singleton.h"
#include "protocol/config.pb.h"

namespace mozc {

namespace config {

// TODO(hidehiko): Move some methods which don't depend on "config" to the
//   mozc::Util class.
class CharacterFormManager {
 public:
  struct NumberFormStyle {
    Config::CharacterForm form;
    NumberUtil::NumberString::Style style;
  };

  CharacterFormManager(const CharacterFormManager &) = delete;
  CharacterFormManager &operator=(const CharacterFormManager &) = delete;

  // Returns the preference of character form.
  // This method cannot return the preference,
  // if str has two set of string groups having different preferences.
  // e.g., GetPreeditCharacterForm("グーグル012") returns NO_CONVERSION
  Config::CharacterForm GetPreeditCharacterForm(absl::string_view input) const;
  Config::CharacterForm GetConversionCharacterForm(
      absl::string_view input) const;

  // Converts string according to the config rules.
  void ConvertPreeditString(absl::string_view input, std::string *output) const;
  void ConvertConversionString(absl::string_view input,
                               std::string *output) const;

  std::string ConvertPreeditString(absl::string_view input) const {
    std::string output;
    ConvertPreeditString(input, &output);
    return output;
  }
  std::string ConvertConversionString(absl::string_view input) const {
    std::string output;
    ConvertConversionString(input, &output);
    return output;
  }

  // Converts string according to the config rules.
  // if alternate output, which should be shown next to
  // the output, is defined, it is stored into alternative_output.
  // If alternative_output is not required, you can pass nullptr.
  // e.g., output = "@" alternative_output = "＠"
  // return true if both output and alternative_output are defined.
  bool ConvertPreeditStringWithAlternative(
      absl::string_view input, std::string *output,
      std::string *alternative_output) const;
  bool ConvertConversionStringWithAlternative(
      absl::string_view input, std::string *output,
      std::string *alternative_output) const;

  // Calls this method after user fixed the final result.
  // If a character has "LAST_FORM" preference in the config,
  // SetCharacterForm() stores the last character form into local file.
  // Next time user calls GetPreeditCharacterForm() or
  // GetConversionCharacterForm(), the preference is restored.
  void SetCharacterForm(absl::string_view input, Config::CharacterForm form);

  // Guesses the character form of str and call
  // SetCharacterForm with this result.
  // Call this method after user fixed the final result.
  // It is more useful to call GuessAndSetCharacterForm(), as
  // you don't need to pass the form.
  // You can just pass the final final conversion string to this method.
  void GuessAndSetCharacterForm(absl::string_view input);

  void SetLastNumberStyle(const NumberFormStyle &form_style);
  std::optional<const NumberFormStyle> GetLastNumberStyle() const;

  // Clears history data. This method does not clear config data.
  void ClearHistory();

  // Clears internal rules.
  void Clear();

  // Adds a rule.
  // Example:
  // AddPreeditRule("[]{}()", config::Config::LAST_FORM);
  // AddPreeditRule("+=", config::Config::HALF_WIDTH);
  // The all characters in str are treated as the same group.
  void AddPreeditRule(absl::string_view input, Config::CharacterForm form);

  void AddConversionRule(absl::string_view input, Config::CharacterForm form);

  // Loads Default rules.
  void SetDefaultRule();

  // Reload config explicitly.
  void ReloadConfig(const Config &config);

  // Utility function: pass character form.
  static std::string ConvertWidth(std::string input,
                                  Config::CharacterForm form);

  // Returns the singleton instance.
  static CharacterFormManager *GetCharacterFormManager();

 private:
  class Data;

  // TODO(team): Get rid of Singleton.
  friend class Singleton<CharacterFormManager>;

  CharacterFormManager();
  ~CharacterFormManager() = default;

  std::unique_ptr<Data> data_;
};

}  // namespace config
}  // namespace mozc

#endif  // MOZC_CONFIG_CHARACTER_FORM_MANAGER_H_
