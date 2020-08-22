// Copyright 2010-2020, Google Inc.
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

#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/protobuf/descriptor.h"
#include "base/util.h"
#include "composer/key_parser.h"
#include "protocol/commands.pb.h"
#include "absl/strings/str_format.h"

namespace mozc {
namespace emacs {

namespace {
// forward declaration
void PrintField(const protobuf::Message &message,
                const protobuf::Reflection &reflection,
                const protobuf::FieldDescriptor &field,
                std::vector<std::string> *output);
void PrintFieldValue(const protobuf::Message &message,
                     const protobuf::Reflection &reflection,
                     const protobuf::FieldDescriptor &field, int index,
                     std::vector<std::string> *output);
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
void ParseInputLine(const std::string &line, uint32 *event_id,
                    uint32 *session_id, mozc::commands::Input *input) {
  CHECK(event_id);
  CHECK(session_id);
  CHECK(input);

  std::vector<std::string> tokens;
  if (!TokenizeSExpr(line, &tokens) ||
      tokens.size() < 4 ||  // Must be at least '(' EVENT_ID COMMAND ')'.
      tokens.front() != "(" || tokens.back() != ")") {
    ErrorExit(kErrScanError, "S expression in the wrong format");
  }

  // Read an event ID (a sequence number).
  if (!NumberUtil::SafeStrToUInt32(tokens[1], event_id)) {
    ErrorExit(kErrWrongTypeArgument, "Event ID is not an integer");
  }

  // Read a command.
  const std::string &func = tokens[2];
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
      if (tokens.size() != 4) {
        ErrorExit(kErrWrongNumberOfArguments, "Wrong number of arguments");
      }
      break;
    }
    case mozc::commands::Input::DELETE_SESSION: {
      // Suppose: (EVENT_ID DeleteSession SESSION_ID)
      if (tokens.size() != 5) {
        ErrorExit(kErrWrongNumberOfArguments, "Wrong number of arguments");
      }
      // Parse session ID.
      if (!NumberUtil::SafeStrToUInt32(tokens[3], session_id)) {
        ErrorExit(kErrWrongTypeArgument, "Session ID is not an integer");
      }
      break;
    }
    case mozc::commands::Input::SEND_KEY: {
      // Suppose: (EVENT_ID SendKey SESSION_ID KEY...)
      if (tokens.size() < 6) {
        ErrorExit(kErrWrongNumberOfArguments, "Wrong number of arguments");
      }
      // Parse session ID.
      if (!NumberUtil::SafeStrToUInt32(tokens[3], session_id)) {
        ErrorExit(kErrWrongTypeArgument, "Session ID is not an integer");
      }
      // Parse keys.
      std::vector<std::string> keys;
      std::string key_string;
      for (int i = 4; i < tokens.size() - 1; ++i) {
        if (isdigit(tokens[i][0])) {  // Numeric key code
          uint32 key_code;
          if (!NumberUtil::SafeStrToUInt32(tokens[i], &key_code) ||
              key_code > 255) {
            ErrorExit(kErrWrongTypeArgument, "Wrong character code");
          }
          keys.push_back(std::string(1, static_cast<char>(key_code)));
        } else if (tokens[i][0] == '\"') {  // String literal
          if (!key_string.empty()) {
            ErrorExit(kErrWrongTypeArgument, "Wrong number of key strings");
          }
          if (!UnquoteString(tokens[i], &key_string)) {
            ErrorExit(kErrWrongTypeArgument, "Wrong key string literal");
          }
        } else {  // Key symbol
          keys.push_back(tokens[i]);
        }
      }
      if (!mozc::KeyParser::ParseKeyVector(keys, input->mutable_key()) &&
          // If there are any unsupported key symbols, falls back to
          // mozc::commands::KeyEvent::UNDEFINED_KEY.
          !mozc::KeyParser::ParseKey("undefinedkey", input->mutable_key())) {
        DLOG(FATAL);  // Code must not reach here.
      }
      if (!key_string.empty()) {
        input->mutable_key()->set_key_string(key_string);
      }
      break;
    }
    default:
      DLOG(FATAL);  // Code must not reach here.
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
void PrintMessage(const protobuf::Message &message,
                  std::vector<std::string> *output) {
  DCHECK(output);

  const protobuf::Reflection *reflection = message.GetReflection();
  std::vector<const protobuf::FieldDescriptor *> fields;
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
std::string NormalizeSymbol(const std::string &symbol) {
  std::string s = symbol;
  mozc::Util::LowerString(&s);
  std::replace(s.begin(), s.end(), '_', '-');
  return s;
}

// Returns a quoted string as a string literal in S-expression.
// - double-quote is converted to backslash + double-quote
// - backslash is converted to backslash + backslash
//
// Control characters, including newline('\n'), in a given string remain as is.
std::string QuoteString(const std::string &str) {
  std::string tmp, escaped_body;
  mozc::Util::StringReplace(str, "\\", "\\\\", true, &tmp);
  mozc::Util::StringReplace(tmp, "\"", "\\\"", true, &escaped_body);
  return "\"" + escaped_body + "\"";
}

// Unquotes and unescapes a double-quoted string.
// The input string must begin and end with double quotes.
bool UnquoteString(const std::string &input, std::string *output) {
  DCHECK(output);
  output->clear();

  if (input.length() < 2 || *input.begin() != '\"' || *input.rbegin() != '\"') {
    return false;  // wrong format
  }

  std::string result;
  result.reserve(input.size());

  bool escape = false;
  for (std::string::const_iterator i = ++input.begin(), e = --input.end();
       i != e; ++i) {
    if (escape) {
      char c = *i;
      switch (*i) {
        case 'a':
          c = '\x07';
          break;  // control-g
        case 'b':
          c = '\x08';
          break;  // backspace
        case 't':
          c = '\x09';
          break;  // tab
        case 'n':
          c = '\x0a';
          break;  // newline
        case 'v':
          c = '\x0b';
          break;  // vertical tab
        case 'f':
          c = '\x0c';
          break;  // formfeed
        case 'r':
          c = '\x0d';
          break;  // carriage return
        case 'e':
          c = '\x1b';
          break;  // escape
        case 's':
          c = '\x20';
          break;  // space
        case 'd':
          c = '\x7f';
          break;  // delete
      }
      result.push_back(c);
      escape = false;
    } else if (*i == '\\') {
      escape = true;
    } else if (*i == '\"') {
      // Double-quote w/o the escape sign must not appear inside a quoted
      // string.
      return false;
    } else {
      result.push_back(*i);
    }
  }

  if (escape) {  // wrong format
    return false;
  }
  output->swap(result);
  return true;
}

// Tokenizes the given string as S expression.  Returns true if success.
//
// This function implements very simple tokenization and is NOT conforming to
// the definition of S expression.  For example, this function does not return
// an error for the input "\'".
bool TokenizeSExpr(const std::string &input, std::vector<std::string> *output) {
  DCHECK(output);

  std::vector<std::string> results;

  for (std::string::const_iterator i = input.begin(); i != input.end(); ++i) {
    if (isspace(*i)) {
      continue;
    }  // Skip white space.

    if (!isgraph(*i)) {
      return false;  // unrecognized control character
    }

    switch (*i) {
      case ';':  // comment
        while (i != input.end() && *i != '\n') {
          ++i;
        }
        break;
      case '(':
      case ')':  // list parantheses
      case '[':
      case ']':   // vector parantheses
      case '\'':  // quote
      case '`':   // quasiquote
        results.push_back(std::string(1, *i));
        break;
      case '\"': {  // string
        std::string::const_iterator start = i++;
        for (bool escape = false;; ++i) {
          if (i == input.end()) {
            return false;  // unexpected end of string
          }
          if (escape) {
            escape = false;
          } else if (*i == '\\') {
            escape = true;
          } else if (*i == '\"') {
            break;
          }
        }
        results.push_back(std::string(start, i + 1));
        break;
      }
      default: {  // must be atom
        std::string::const_iterator start = i++;
        for (;; ++i) {
          if (i == input.end()) {
            break;
          }
          if (!isgraph(*i)) {
            break;
          }
          bool is_special_char = false;
          switch (*i) {
            case ';':  // comment
            case '(':
            case ')':  // list parantheses
            case '[':
            case ']':   // vector parantheses
            case '\'':  // quote
            case '`':   // quasiquote
            case '\"':  // string
              is_special_char = true;
          }
          if (is_special_char) {
            break;
          }
        }
        results.push_back(std::string(start, i));
        --i;  // Put the last char back.
        break;
      }
    }
  }

  output->swap(results);
  return true;
}

// Prints an error message in S-expression and terminates with status code 1.
void ErrorExit(const std::string &error, const std::string &message) {
  absl::FPrintF(stdout, "((error . %s)(message . %s))\n", error,
                QuoteString(message));
  exit(1);
}

bool RemoveUsageData(mozc::commands::Output *output) {
  if (!output->has_candidates()) {
    return false;
  }
  if (!output->candidates().has_usages()) {
    return false;
  }
  output->mutable_candidates()->mutable_usages()->Clear();
  return true;
}

namespace {

// Prints one entry of a protocol buffer in S-expression.
// An entry is a cons cell of key and value.
//
// Input parameter 'message' is a protocol buffer to be output.
// 'reflection' must be a reflection object of 'message'.  'field' is
// a field descriptor in 'message' to be output.  'field' can have both of
// a single value and repeated values.
// 'output' is a pseudo output stream to output field's key and value(s).
void PrintField(const protobuf::Message &message,
                const protobuf::Reflection &reflection,
                const protobuf::FieldDescriptor &field,
                std::vector<std::string> *output) {
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
// 'output' is a pseudo output stream to output the value.
void PrintFieldValue(const protobuf::Message &message,
                     const protobuf::Reflection &reflection,
                     const protobuf::FieldDescriptor &field, int index,
                     std::vector<std::string> *output) {
#define GET_FIELD_VALUE(METHOD_TYPE)                                 \
  (field.is_repeated()                                               \
       ? reflection.GetRepeated##METHOD_TYPE(message, &field, index) \
       : reflection.Get##METHOD_TYPE(message, &field))

  switch (field.cpp_type()) {
    // Number (integer and floating point)
#define PRINT_FIELD_VALUE(PROTO_CPP_TYPE, METHOD_TYPE, CPP_TYPE, FORMAT) \
  case protobuf::FieldDescriptor::CPPTYPE_##PROTO_CPP_TYPE:              \
    output->push_back(mozc::Util::StringPrintf(                          \
        FORMAT, static_cast<CPP_TYPE>(GET_FIELD_VALUE(METHOD_TYPE))));   \
    break;

    // Since Emacs does not support 64-bit integers, it supports only
    // 60-bit integers on 64-bit version, and 28-bit on 32-bit version,
    // we escape it into a string as a workaround.
    // We don't need any 64-bit values on Emacs so far, and 32-bit
    // integer values have never got over 28-bit yet.
    PRINT_FIELD_VALUE(INT32, Int32, int32, "%d");
    PRINT_FIELD_VALUE(INT64, Int64, int64, "\"%d\"");  // as a string
    PRINT_FIELD_VALUE(UINT32, UInt32, uint32, "%u");
    PRINT_FIELD_VALUE(UINT64, UInt64, uint64, "\"%u\"");  // as a string
    PRINT_FIELD_VALUE(DOUBLE, Double, double, "%f");
    PRINT_FIELD_VALUE(FLOAT, Float, float, "%f");
#undef PRINT_FIELD_VALUE

    case protobuf::FieldDescriptor::CPPTYPE_BOOL:  // bool
      output->push_back(GET_FIELD_VALUE(Bool) ? "t" : "nil");
      break;

    case protobuf::FieldDescriptor::CPPTYPE_ENUM:  // enum
      output->push_back(NormalizeSymbol(GET_FIELD_VALUE(Enum)->name()));
      break;

    case protobuf::FieldDescriptor::CPPTYPE_STRING: {  // string
      std::string str;
      str = field.is_repeated()
                ? reflection.GetRepeatedStringReference(message, &field, index,
                                                        &str)
                : reflection.GetStringReference(message, &field, &str);
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
