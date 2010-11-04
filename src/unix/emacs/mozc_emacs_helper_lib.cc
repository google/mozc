// Copyright 2010, Google Inc.
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

#include "unix/emacs/mozc_emacs_helper_lib.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "base/base.h"
#include "base/protobuf/descriptor.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"

namespace mozc {
namespace emacs {

namespace {
// forward declaration
void PrintField(
    const protobuf::Message &message,
    const protobuf::Reflection &reflection,
    const protobuf::FieldDescriptor &field,
    vector<string>* output);
void PrintFieldValue(
    const protobuf::Message &message,
    const protobuf::Reflection &reflection,
    const protobuf::FieldDescriptor &field,
    int index,
    vector<string>* output);
}  // namespace


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
    mozc::commands::Input *input) {
  CHECK(event_id);
  CHECK(session_id);
  CHECK(input);

  // Skip left parenthesis.
  int i = 0;
  while (i < line.length() && isspace(line[i])) {
    ++i;  // Skip white spaces.
  }
  if (!(i < line.length() && line[i] == '(')) {
    ErrorExit(kErrScanError, "No left parenthesis");
  }
  ++i;

  // Skip right parenthesis.
  int j = line.length() - 1;
  while (0 <= j && isspace(line[j])) {
    --j;  // Skip white spaces.
  }
  if (!(0 <= j && line[j] == ')')) {
    ErrorExit(kErrScanError, "No right parenthesis");
  }
  DCHECK(i <= j);

  vector<string> tokens;
  mozc::Util::SplitStringUsing(line.substr(i, j - i), "\t\n\v\f\r ", &tokens);

  // Read an event ID (a sequence number).
  if (!mozc::Util::SafeStrToUInt32(tokens[0], event_id)) {
    ErrorExit(kErrWrongTypeArgument, "Event ID is not an integer");
  }

  // Read a command.
  const string &func = tokens[1];
  if (func == "SendKey") {  // SendKey is a most-frequently-used command.
    input->set_type(mozc::commands::Input::SEND_KEY);
  } else if (func == "CreateSession") {
    input->set_type(mozc::commands::Input::CREATE_SESSION);
  } else if (func == "DeleteSession") {
    input->set_type(mozc::commands::Input::DELETE_SESSION);
  } else {
    // Mozc has SendTestKey and SendCommand commands in addition to the above.
    // But this code doesn't support them because of no need so far.
    ErrorExit(kErrVoidFunction, "Unknown function");
  }

  switch (input->type()) {
  case mozc::commands::Input::CREATE_SESSION: {
    // Suppose: (EVENT_ID CreateSession)
    if (tokens.size() != 2) {
      ErrorExit(kErrWrongNumberOfArguments, "Wrong number of arguments");
    }
    break;
  }
  case mozc::commands::Input::DELETE_SESSION: {
    // Suppose: (EVENT_ID DeleteSession SESSION_ID)
    if (tokens.size() != 3) {
      ErrorExit(kErrWrongNumberOfArguments, "Wrong number of arguments");
    }
    // Parse session ID.
    if (!mozc::Util::SafeStrToUInt32(tokens[2], session_id)) {
      ErrorExit(kErrWrongTypeArgument, "Session ID is not an integer");
    }
    break;
  }
  case mozc::commands::Input::SEND_KEY: {
    // Suppose: (EVENT_ID SendKey SESSION_ID KEY...)
    if (tokens.size() < 4) {
      ErrorExit(kErrWrongNumberOfArguments, "Wrong number of arguments");
    }
    // Parse session ID.
    if (!mozc::Util::SafeStrToUInt32(tokens[2], session_id)) {
      ErrorExit(kErrWrongTypeArgument, "Session ID is not an integer");
    }
    // Parse keys.
    vector<string> keys;
    for (int i = 3; i < tokens.size(); ++i) {
      if (isdigit(tokens[i][0])) {  // Numeric key code
        uint32 key_code;
        if (!mozc::Util::SafeStrToUInt32(tokens[i], &key_code) ||
            key_code > 255) {
          ErrorExit(kErrWrongTypeArgument, "Wrong character code");
        }
        keys.push_back(string(1, static_cast<char>(key_code)));
      } else {  // Key symbol
        keys.push_back(tokens[i]);
      }
    }
    if (!mozc::KeyParser::ParseKeyVector(keys, input->mutable_key())) {
      ErrorExit(kErrWrongTypeArgument, "Unknown key symbol");
    }
    break;
  }
  default:
    DCHECK(false);  // Code must not reach here.
  }
}


// Prints the content of a protocol buffer in S-expression.
// - 'message' and 'group' are mapped to alist (associative list)
// - 'repeated' is expressed as a list
// - other types are expressed as is
//
// Input parameter 'message' is a protocol buffer to be output.
// 'output' is a text buffer to output 'message'.
//
// This function never outputs newlines except for ones in strings.
void PrintMessage(
    const protobuf::Message &message,
    vector<string>* output) {
  DCHECK(output);

  const protobuf::Reflection *reflection = message.GetReflection();
  vector<const protobuf::FieldDescriptor*> fields;
  reflection->ListFields(message, &fields);

  output->push_back("(");
  for (int i = 0; i < fields.size(); ++i) {
    PrintField(message, *reflection, *fields[i], output);
  }
  output->push_back(")");
}


// Utilities

// Normalizes a symbol with the following rules:
// - all alphabets are converted to lowercase
// - underscore('_') is converted to dash('-')
string NormalizeSymbol(const string &symbol) {
  string s = symbol;
  mozc::Util::LowerString(&s);
  replace(s.begin(), s.end(), '_', '-');
  return s;
}

// Returns a quoted string as a string literal in S-expression.
// - double-quote is converted to backslash + double-quote
// - backslash is converted to backslash + backslash
//
// Control characters, including newline('\n'), in a given string remain as is.
string QuoteString(const string &str) {
  string tmp, escaped_body;
  mozc::Util::StringReplace(str, "\\", "\\\\", true, &tmp);
  mozc::Util::StringReplace(tmp, "\"", "\\\"", true, &escaped_body);
  return "\"" + escaped_body + "\"";
}

// Prints an error message in S-expression and terminates with status code 1.
void ErrorExit(const string &error, const string &message) {
  fprintf(stderr, "((error . %s)(message . %s))\n",
          error.c_str(), QuoteString(message).c_str());
  exit(1);
}

namespace {

// Prints one entry of a protocol buffer in S-expression.
// An entry is a cons cell of key and value.
//
// Input parameter 'message' is a protocol buffer to be output.
// 'reflection' must be a reflection object of 'message'.  'field' is
// a field descriptor in 'message' to be output.  'field' can have both of
// a single value and repeated values.
// 'os' is an output stream to output field's key and value(s).
void PrintField(
    const protobuf::Message &message,
    const protobuf::Reflection &reflection,
    const protobuf::FieldDescriptor &field,
    vector<string>* output) {
  output->push_back("(");
  output->push_back(NormalizeSymbol(field.name()));

  if (!field.is_repeated()) {
    output->push_back(" . ");  // Print an object as a value.
    PrintFieldValue(message, reflection, field, -1 /* dummy arg */, output);
  } else {
    output->push_back(" ");  // Print objects as a list.
    const int count = reflection.FieldSize(message, &field);
    const bool is_message =
        field.cpp_type() == protobuf::FieldDescriptor::CPPTYPE_MESSAGE;
    for (int i = 0; i < count; ++i) {
      if (i != 0 && !is_message) {
        output->push_back(" ");
      }
      PrintFieldValue(message, reflection, field, i, output);
    }
  }

  output->push_back(")");
}

// Prints a value of a field of a protocol buffer in S-expression.
// - integer and floating point number are represented as is
// - bool is represented as "t" or "nil"
// - enum is represented as symbol
// - string is represented as quoted string
// - message and group are represented as alist
//
// Input parameter 'message' is a protocol buffer to be output.
// 'reflection' must be a reflection object of 'message'.  'field' is
// a field descriptor in 'message' to be output.  'field' can have both of
// a single value and repeated values.  If 'field' has repeated values,
// 'index' specifies its index to be output.  Otherwise, 'index' is ignored.
// 'os' is an output stream to output the value.
void PrintFieldValue(
    const protobuf::Message &message,
    const protobuf::Reflection &reflection,
    const protobuf::FieldDescriptor &field,
    int index,
    vector<string>* output) {
#define GET_FIELD_VALUE(METHOD_TYPE)                                \
    (field.is_repeated() ?                                          \
     reflection.GetRepeated##METHOD_TYPE(message, &field, index) :  \
     reflection.Get##METHOD_TYPE(message, &field))

  switch (field.cpp_type()) {
    // Number (integer and floating point)
#define PRINT_FIELD_VALUE(CPP_TYPE, METHOD_TYPE, FORMAT)              \
    case protobuf::FieldDescriptor::CPPTYPE_##CPP_TYPE:               \
        output->push_back(                                            \
            mozc::Util::StringPrintf(FORMAT,                          \
                                     GET_FIELD_VALUE(METHOD_TYPE)));  \
    break;

    // Since Emacs does not support 64-bit integers, it supports only
    // 60-bit integers on 64-bit version, and 28-bit on 32-bit version,
    // we escape it into a string as a workaround.
    // We don't need any 64-bit values on Emacs so far, and 32-bit
    // integer values have never got over 28-bit yet.
    PRINT_FIELD_VALUE(INT32, Int32, "%d");
    PRINT_FIELD_VALUE(INT64, Int64, "\"%"GG_LL_FORMAT"d\"");  // as a string
    PRINT_FIELD_VALUE(UINT32, UInt32, "%u");
    PRINT_FIELD_VALUE(UINT64, UInt64, "\"%"GG_LL_FORMAT"u\"");  // as a string
    PRINT_FIELD_VALUE(DOUBLE, Double, "%f");
    PRINT_FIELD_VALUE(FLOAT, Float, "%f");
#undef PRINT_FIELD_VALUE

    case protobuf::FieldDescriptor::CPPTYPE_BOOL:  // bool
      output->push_back(GET_FIELD_VALUE(Bool) ? "t" : "nil");
      break;

    case protobuf::FieldDescriptor::CPPTYPE_ENUM:  // enum
      output->push_back(NormalizeSymbol(GET_FIELD_VALUE(Enum)->name()));
      break;

    case protobuf::FieldDescriptor::CPPTYPE_STRING: {  // string
      string str;
      str = field.is_repeated() ?
            reflection.GetRepeatedStringReference(
                message, &field, index, &str) :
            reflection.GetStringReference(message, &field, &str);
      output->push_back(QuoteString(str));
      break;
    }

    // message and group
    case protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      PrintMessage(GET_FIELD_VALUE(Message), output);
      break;
  }

#undef GET_FIELD_VALUE
}

}  // namespace
}  // namespace emacs
}  // namespace mozc
