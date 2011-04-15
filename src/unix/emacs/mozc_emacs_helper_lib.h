// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_UNIX_EMACS_MOZC_EMACS_HELPER_LIB_H_
#define MOZC_UNIX_EMACS_MOZC_EMACS_HELPER_LIB_H_

#include <string>
#include <vector>

#include "base/port.h"
#include "base/protobuf/protobuf.h"

namespace mozc {
namespace commands {
class Input;
}  // namespace commands

namespace emacs {

// Error symbols used to call ErrorExit()
// These symbols are taken from error symbols of GNU Emacs
// except for ipc-error.
const char kErrScanError[] = "scan-error";
const char kErrWrongNumberOfArguments[] = "wrong-number-of-arguments";
const char kErrWrongTypeArgument[] = "wrong-type-argument";
const char kErrVoidFunction[] = "void-function";
const char kErrSessionError[] = "session-error";


// Parses a line, which must be a single complete command in form of:
//     '(' EVENT_ID COMMAND [ARGUMENT]... ')'
// where EVENT_ID is an arbitrary integer used to identify the response
// according to the command (see 'emacs-event-id' in a response).
// Normally it's just a sequence number of transactions.
// COMMAND is one of 'CreateSession', 'DeleteSession' and 'SendKey'.
// ARGUMENTs depend on a command.
// An input line must be surrounded by a pair of parentheses,
// like a S-expression.
void ParseInputLine(
    const string &line, uint32 *event_id, uint32 *session_id,
    mozc::commands::Input *input);


// Prints the content of a protocol buffer in S-expression.
// - 'message' and 'group' are mapped to alist (associative list)
// - 'repeated' is expressed as a list
// - other types are expressed as is
//
// Input parameter 'message' is a protocol buffer to be output.
// 'output' is a text buffer to output 'message'.
//
// This function never outputs newlines except for ones in strings.
void PrintMessage(const mozc::protobuf::Message &message,
                  vector<string>* output);


// Utilities

// Normalizes a symbol with the following rule:
// - all alphabets are converted to lowercase
// - underscore('_') is converted to dash('-')
string NormalizeSymbol(const string &symbol);

// Returns a quoted string as a string literal in S-expression.
// - double-quote is converted to backslash + double-quote
// - backslash is converted to backslash + backslash
//
// Control characters, including newline('\n'), in a given string remain as is.
string QuoteString(const string &str);

// Unquotes and unescapes a double-quoted string.
// The input string must begin and end with double quotes.
bool UnquoteString(const string &input, string *output);

// Tokenizes the given string as S expression.  Returns true if success.
//
// This function implements very simple tokenization and is NOT conforming to
// the definition of S expression.  For example, this function does not return
// an error for the input "\'".
bool TokenizeSExpr(const string &input, vector<string> *output);

// Prints an error message in S-expression and terminates with status code 1.
void ErrorExit(const string &error, const string &message);

}  // namespace emacs
}  // namespace mozc

#endif  // MOZC_UNIX_EMACS_MOZC_EMACS_HELPER_LIB_H_
