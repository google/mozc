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

#include "testing/testing_util.h"

#include <memory>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/protobuf/text_format.h"
#include "testing/gunit.h"

#undef GetMessage  // Undef Win32 macro.

namespace mozc {
namespace testing {

using ::mozc::protobuf::Descriptor;
using ::mozc::protobuf::FieldDescriptor;
using ::mozc::protobuf::Message;
using ::mozc::protobuf::Reflection;
using ::mozc::protobuf::TextFormat;

namespace internal {

namespace {

bool EqualsProtoInternal(const Message& message1, const Message& message2,
                         bool is_partial);

// Compares (non-repeated) filed of the given messages.
bool EqualsField(const FieldDescriptor* field, const Reflection* reflection,
                 const Message& message1, const Message& message2,
                 bool is_partial) {
  const bool has_field = reflection->HasField(message1, field);
  if (is_partial && !has_field) {
    // Don't check empty fields for partial equality check.
    return true;
  }

  if (has_field != reflection->HasField(message2, field)) {
    return false;
  }

// Use macro for boilerplate code generation.
#define MOZC_PROTO_FIELD_EQ_CASE(cpptype, method) \
  case FieldDescriptor::cpptype:                  \
    if (reflection->method(message1, field) !=    \
        reflection->method(message2, field)) {    \
      return false;                               \
    }                                             \
    break

  switch (field->cpp_type()) {
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_INT32, GetInt32);
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_INT64, GetInt64);
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_UINT32, GetUInt32);
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_UINT64, GetUInt64);
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_DOUBLE, GetDouble);
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_FLOAT, GetFloat);
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_BOOL, GetBool);
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_ENUM, GetEnum);
    MOZC_PROTO_FIELD_EQ_CASE(CPPTYPE_STRING, GetString);
    case FieldDescriptor::CPPTYPE_MESSAGE:
      if (!EqualsProtoInternal(reflection->GetMessage(message1, field),
                               reflection->GetMessage(message2, field),
                               is_partial)) {
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Unknown cpp_type: " << field->cpp_type();
      return false;
  }
#undef MOZC_PROTO_FIELD_EQ_CASE

  return true;
}

bool EqualsRepeatedField(const FieldDescriptor* field,
                         const Reflection* reflection, const Message& message1,
                         const Message& message2, bool is_partial) {
  const int field_size = reflection->FieldSize(message1, field);
  if (is_partial && field_size == 0) {
    // Don't check empty fields for partial equality check.
    return true;
  }

  if (field_size != reflection->FieldSize(message2, field)) {
    return false;
  }

// Use macro for boilerplate code generation.
#define MOZC_PROTO_REPEATED_FIELD_EQ_CASE(cpptype, method) \
  case FieldDescriptor::cpptype:                           \
    for (int i = 0; i < field_size; ++i) {                 \
      if (reflection->method(message1, field, i) !=        \
          reflection->method(message2, field, i)) {        \
        return false;                                      \
      }                                                    \
    }                                                      \
    break

  switch (field->cpp_type()) {
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_INT32, GetRepeatedInt32);
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_INT64, GetRepeatedInt64);
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_UINT32, GetRepeatedUInt32);
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_UINT64, GetRepeatedUInt64);
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_DOUBLE, GetRepeatedDouble);
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_FLOAT, GetRepeatedFloat);
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_BOOL, GetRepeatedBool);
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_ENUM, GetRepeatedEnum);
    MOZC_PROTO_REPEATED_FIELD_EQ_CASE(CPPTYPE_STRING, GetRepeatedString);
    case FieldDescriptor::CPPTYPE_MESSAGE:
      for (int i = 0; i < field_size; ++i) {
        if (!EqualsProtoInternal(
                reflection->GetRepeatedMessage(message1, field, i),
                reflection->GetRepeatedMessage(message2, field, i),
                is_partial)) {
          return false;
        }
      }
      break;
    default:
      LOG(ERROR) << "Unknown cpp_type: " << field->cpp_type();
      return false;
  }
#undef MOZC_PROTO_REPEATED_FIELD_EQ_CASE

  return true;
}

bool EqualsProtoInternal(const Message& message1, const Message& message2,
                         bool is_partial) {
  const Descriptor* descriptor = message1.GetDescriptor();
  CHECK(descriptor == message2.GetDescriptor());

  const Reflection* reflection = message1.GetReflection();
  CHECK(reflection == message2.GetReflection());

  for (int i = 0; i < descriptor->field_count(); ++i) {
    const FieldDescriptor* field = descriptor->field(i);
    CHECK(field != nullptr);
    if (field->is_repeated()) {
      if (!EqualsRepeatedField(field, reflection, message1, message2,
                               is_partial)) {
        return false;
      }
    } else {
      if (!EqualsField(field, reflection, message1, message2, is_partial)) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace

::testing::AssertionResult EqualsProtoFormat(absl::string_view expect_string,
                                             absl::string_view actual_string,
                                             const Message& expect,
                                             const Message& actual,
                                             bool is_partial) {
  if (EqualsProtoInternal(expect, actual, is_partial)) {
    return ::testing::AssertionSuccess();
  }

  return ::testing::AssertionFailure()
         << "EXPECT_PROTO_" << (is_partial ? "P" : "") << "EQ(" << expect_string
         << ", " << actual_string << ")" << " evaluates to false, where\n"
         << expect_string << " evaluates to " << absl::StrCat(expect) << "\n"
         << actual_string << " evaluates to " << absl::StrCat(actual);
}

}  // namespace internal

namespace {
::testing::AssertionResult EqualsProtoWithParse(absl::string_view expect_string,
                                                absl::string_view actual_string,
                                                absl::string_view expect,
                                                const Message& actual,
                                                bool is_partial) {
  // Note: Message::New returns an instance of the actual type,
  // so we can convert the string representation of the "actual"'s type,
  // by simply parsing it.
  std::unique_ptr<Message> expect_message(actual.New());
  TextFormat::Parser parser;
  parser.AllowPartialMessage(is_partial);
  CHECK(parser.ParseFromString(expect, expect_message.get()))
      << "Failed to parse message: " << expect;

  return internal::EqualsProtoFormat(expect_string, actual_string,
                                     *expect_message, actual, is_partial);
}
}  // namespace

::testing::AssertionResult EqualsProto(absl::string_view expect_string,
                                       absl::string_view actual_string,
                                       absl::string_view expect,
                                       const Message& actual) {
  return EqualsProtoWithParse(expect_string, actual_string, expect, actual,
                              false);
}

::testing::AssertionResult PartiallyEqualsProto(absl::string_view expect_string,
                                                absl::string_view actual_string,
                                                absl::string_view expect,
                                                const Message& actual) {
  return EqualsProtoWithParse(expect_string, actual_string, expect, actual,
                              true);
}

}  // namespace testing
}  // namespace mozc
