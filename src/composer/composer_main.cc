// Copyright 2010-2012, Google Inc.
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

#include <iostream>
#include "base/base.h"
#include "transliteration/transliteration.h"
#include "composer/composition_interface.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "session/commands.pb.h"

DEFINE_string(table, "system://romanji-hiragana.tsv",
              "preedit conversion table file.");

using ::mozc::commands::Request;

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::composer::Table table;
  table.LoadFromFile(FLAGS_table.c_str());
  scoped_ptr<mozc::composer::Composer> composer(
      new mozc::composer::Composer(&table, Request::default_instance()));

  string command;
  string left, focused, right;

  while (getline(cin, command)) {
    if (command == "<") {
      composer->MoveCursorLeft();
    } else if (command == "<<") {
      composer->MoveCursorToBeginning();
    } else if (command == ">") {
      composer->MoveCursorRight();
    } else if (command == ">>") {
      composer->MoveCursorToEnd();
    } else if (command == "<>") {
      composer->ToggleInputMode();
    } else if (command == ">a<") {
      composer->SetInputMode(mozc::transliteration::HALF_ASCII);
    } else if (command == ">A<") {
      composer->SetInputMode(mozc::transliteration::FULL_ASCII);
    } else if (command == ">k<") {
      composer->SetInputMode(mozc::transliteration::HALF_KATAKANA);
    } else if (command == ">K<") {
      composer->SetInputMode(mozc::transliteration::FULL_KATAKANA);
    } else if (command == ">h<" || command == ">H<") {
      composer->SetInputMode(mozc::transliteration::HIRAGANA);
    } else if (command == "!") {
      composer->Delete();
    } else if (command == "!!") {
      composer->EditErase();
    } else {
      composer->InsertCharacter(command);
    }
    composer->GetPreedit(&left, &focused, &right);
    cout << left << "[" << focused << "]" << right << endl;
  }
}
