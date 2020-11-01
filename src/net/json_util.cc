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

#include "net/json_util.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/protobuf/descriptor.h"
#include "net/jsoncpp.h"

using mozc::protobuf::Descriptor;
using mozc::protobuf::EnumValueDescriptor;
using mozc::protobuf::FieldDescriptor;
using mozc::protobuf::Message;
using mozc::protobuf::Reflection;

// <WinUser.h> defines GetMessage as a macro, which eventually
// conflicts with mozc::protobuf::Reflection::GetMessage.
#ifdef GetMessage
#undef GetMessage
#endif  // GetMessage

namespace mozc {
namespace net {
namespace {

bool ProtobufRepeatedFieldValueToJsonValue(const Message &message,
                                           const Reflection &reflection,
                                           const FieldDescriptor &field,
                                           int index, Json::Value *value) {
  switch (field.cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32: {
      *value = Json::Value(reflection.GetRepeatedInt32(message, &field, index));
      return true;
    }
    case FieldDescriptor::CPPTYPE_INT64: {
      *value = Json::Value(std::to_string(static_cast<int64>(
          reflection.GetRepeatedInt64(message, &field, index))));
      return true;
    }
    case FieldDescriptor::CPPTYPE_UINT32: {
      *value =
          Json::Value(reflection.GetRepeatedUInt32(message, &field, index));
      return true;
    }
    case FieldDescriptor::CPPTYPE_UINT64: {
      *value = Json::Value(std::to_string(static_cast<uint64>(
          reflection.GetRepeatedUInt64(message, &field, index))));
      return true;
    }
    case FieldDescriptor::CPPTYPE_FLOAT: {
      *value = Json::Value(reflection.GetRepeatedFloat(message, &field, index));
      return true;
    }
    case FieldDescriptor::CPPTYPE_DOUBLE: {
      *value =
          Json::Value(reflection.GetRepeatedDouble(message, &field, index));
      return true;
    }
    case FieldDescriptor::CPPTYPE_BOOL: {
      *value = Json::Value(reflection.GetRepeatedBool(message, &field, index));
      return true;
    }
    case FieldDescriptor::CPPTYPE_ENUM: {
      *value = Json::Value(
          reflection.GetRepeatedEnum(message, &field, index)->name());
      return true;
    }
    case FieldDescriptor::CPPTYPE_STRING: {
      std::string scratch;
      const std::string &str = reflection.GetRepeatedStringReference(
          message, &field, index, &scratch);
      *value = Json::Value(str);
      return true;
    }
    case FieldDescriptor::CPPTYPE_MESSAGE: {
      return JsonUtil::ProtobufMessageToJsonValue(
          reflection.GetRepeatedMessage(message, &field, index), value);
    }
    default: {
      DLOG(WARNING) << "unsupported filed CppType: " << field.cpp_type();
      break;
    }
  }
  return false;
}

bool ProtobufFieldValueToJsonValue(const Message &message,
                                   const Reflection &reflection,
                                   const FieldDescriptor &field,
                                   Json::Value *value) {
  switch (field.cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32: {
      *value = Json::Value(reflection.GetInt32(message, &field));
      return true;
    }
    case FieldDescriptor::CPPTYPE_INT64: {
      *value = Json::Value(std::to_string(
          static_cast<int64>(reflection.GetInt64(message, &field))));
      return true;
    }
    case FieldDescriptor::CPPTYPE_UINT32: {
      *value = Json::Value(reflection.GetUInt32(message, &field));
      return true;
    }
    case FieldDescriptor::CPPTYPE_UINT64: {
      *value = Json::Value(std::to_string(
          static_cast<uint64>(reflection.GetUInt64(message, &field))));
      return true;
    }
    case FieldDescriptor::CPPTYPE_FLOAT: {
      *value = Json::Value(reflection.GetFloat(message, &field));
      return true;
    }
    case FieldDescriptor::CPPTYPE_DOUBLE: {
      *value = Json::Value(reflection.GetDouble(message, &field));
      return true;
    }
    case FieldDescriptor::CPPTYPE_BOOL: {
      *value = Json::Value(reflection.GetBool(message, &field));
      return true;
    }
    case FieldDescriptor::CPPTYPE_ENUM: {
      *value = Json::Value(reflection.GetEnum(message, &field)->name());
      return true;
    }
    case FieldDescriptor::CPPTYPE_STRING: {
      std::string scratch;
      const std::string &str =
          reflection.GetStringReference(message, &field, &scratch);
      *value = Json::Value(str);
      return true;
    }
    case FieldDescriptor::CPPTYPE_MESSAGE: {
      return JsonUtil::ProtobufMessageToJsonValue(
          reflection.GetMessage(message, &field), value);
    }
    default: {
      DLOG(WARNING) << "unsupported filed CppType: " << field.cpp_type();
      break;
    }
  }
  return false;
}

bool JsonValueToProtobufFieldValue(const Json::Value &value,
                                   const FieldDescriptor *field,
                                   const Reflection *reflection,
                                   Message *message) {
  DCHECK(!field->is_repeated());
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32: {
      if (!value.isConvertibleTo(Json::intValue)) {
        DLOG(ERROR) << "value is not convertible to intValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetInt32(message, field, value.asInt());
      break;
    }
    case FieldDescriptor::CPPTYPE_INT64: {
      if (!value.isConvertibleTo(Json::stringValue)) {
        DLOG(ERROR) << "value is not convertible to stringValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      int64 int_value;
      if (!NumberUtil::SafeStrToInt64(value.asString(), &int_value)) {
        DLOG(ERROR) << "value is not convertible to int64: "
                    << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetInt64(message, field, int_value);
      break;
    }
    case FieldDescriptor::CPPTYPE_UINT32: {
      if (!value.isConvertibleTo(Json::uintValue)) {
        DLOG(ERROR) << "value is not convertible to uintValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetUInt32(message, field, value.asUInt());
      break;
    }
    case FieldDescriptor::CPPTYPE_UINT64: {
      if (!value.isConvertibleTo(Json::stringValue)) {
        DLOG(ERROR) << "value is not convertible to stringValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      uint64 uint_value;
      if (!NumberUtil::SafeStrToUInt64(value.asString(), &uint_value)) {
        DLOG(ERROR) << "value is not convertible to uint64: "
                    << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetUInt64(message, field, uint_value);
      break;
    }
    case FieldDescriptor::CPPTYPE_DOUBLE: {
      if (!value.isConvertibleTo(Json::realValue)) {
        DLOG(ERROR) << "value is not convertible to realValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetDouble(message, field, value.asDouble());
      break;
    }
    case FieldDescriptor::CPPTYPE_FLOAT: {
      if (!value.isConvertibleTo(Json::realValue)) {
        DLOG(ERROR) << "value is not convertible to realValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetFloat(message, field, value.asFloat());
      break;
    }
    case FieldDescriptor::CPPTYPE_BOOL: {
      if (!value.isConvertibleTo(Json::booleanValue)) {
        DLOG(ERROR) << "value is not convertible to booleanValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetBool(message, field, value.asBool());
      break;
    }
    case FieldDescriptor::CPPTYPE_ENUM: {
      if (!value.isConvertibleTo(Json::stringValue)) {
        DLOG(ERROR) << "value is not convertible to stringValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      const EnumValueDescriptor *enum_value =
          field->enum_type()->FindValueByName(value.asString());
      if (!enum_value) {
        DLOG(ERROR) << "value is not enum: " << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetEnum(message, field, enum_value);
      break;
    }
    case FieldDescriptor::CPPTYPE_STRING: {
      if (!value.isConvertibleTo(Json::stringValue)) {
        DLOG(ERROR) << "value is not convertible to stringValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      reflection->SetString(message, field, value.asString());
      break;
    }
    case FieldDescriptor::CPPTYPE_MESSAGE: {
      if (!value.isConvertibleTo(Json::objectValue)) {
        DLOG(ERROR) << "value is not convertible to objectValue: "
                    << Json::FastWriter().write(value);
        return false;
      }
      return JsonUtil::JsonValueToProtobufMessage(
          value, reflection->MutableMessage(message, field, nullptr));
      break;
    }
    default: {
      DLOG(ERROR) << "Unknown or unsupported type: " << field->cpp_type();
      return false;
    }
  }
  return true;
}

bool JsonValueToProtobufRepeatedFieldValue(const Json::Value &value,
                                           const FieldDescriptor *field,
                                           const Reflection *reflection,
                                           Message *message) {
  DCHECK(field->is_repeated());
  DCHECK(value.isArray());
  bool result = true;
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        if (!value[i].isConvertibleTo(Json::intValue)) {
          DLOG(ERROR) << "value is not convertible to intValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          reflection->AddInt32(message, field, value[i].asInt());
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_INT64: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        int64 int_value;
        if (!value[i].isConvertibleTo(Json::stringValue)) {
          DLOG(ERROR) << "value is not convertible to stringValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else if (!NumberUtil::SafeStrToInt64(value[i].asString(),
                                               &int_value)) {
          DLOG(ERROR) << "value is not convertible to int64: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          reflection->AddInt64(message, field, int_value);
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_UINT32: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        if (!value[i].isConvertibleTo(Json::uintValue)) {
          DLOG(ERROR) << "value is not convertible to uintValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          reflection->AddUInt32(message, field, value[i].asUInt());
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_UINT64: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        uint64 uint_value;
        if (!value[i].isConvertibleTo(Json::stringValue)) {
          DLOG(ERROR) << "value is not convertible to stringValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else if (!NumberUtil::SafeStrToUInt64(value[i].asString(),
                                                &uint_value)) {
          DLOG(ERROR) << "value is not convertible to uint64: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          reflection->AddUInt64(message, field, uint_value);
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_DOUBLE: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        if (!value[i].isConvertibleTo(Json::realValue)) {
          DLOG(ERROR) << "value is not convertible to realValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          reflection->AddDouble(message, field, value[i].asDouble());
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_FLOAT: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        if (!value[i].isConvertibleTo(Json::realValue)) {
          DLOG(ERROR) << "value is not convertible to realValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          reflection->AddFloat(message, field, value[i].asFloat());
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_BOOL: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        if (!value[i].isConvertibleTo(Json::booleanValue)) {
          DLOG(ERROR) << "value is not convertible to booleanValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          reflection->AddBool(message, field, value[i].asBool());
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_ENUM: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        if (!value[i].isConvertibleTo(Json::stringValue)) {
          DLOG(ERROR) << "value is not convertible to stringValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          const EnumValueDescriptor *enum_value =
              field->enum_type()->FindValueByName(value[i].asString());
          if (!enum_value) {
            DLOG(ERROR) << "value is not enum: " << value[i].asString();
            result = false;
          } else {
            reflection->AddEnum(message, field, enum_value);
          }
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_STRING: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        if (!value[i].isConvertibleTo(Json::stringValue)) {
          DLOG(ERROR) << "value is not convertible to stringValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          reflection->AddString(message, field, value[i].asString());
        }
      }
      break;
    }
    case FieldDescriptor::CPPTYPE_MESSAGE: {
      for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
        if (!value[i].isConvertibleTo(Json::objectValue)) {
          DLOG(ERROR) << "value is not convertible to objectValue: "
                      << Json::FastWriter().write(value[i]);
          result = false;
        } else {
          if (!JsonUtil::JsonValueToProtobufMessage(
                  value[i], reflection->AddMessage(message, field, nullptr))) {
            result = false;
          }
        }
      }
      break;
    }
    default: {
      DLOG(ERROR) << "Unknown or unsupported type: " << field->cpp_type();
      return false;
    }
  }
  return result;
}

}  // namespace

bool JsonUtil::ProtobufMessageToJsonValue(const Message &message,
                                          Json::Value *value) {
  *value = Json::Value(Json::objectValue);
  const Descriptor *descriptor = message.GetDescriptor();
  const Reflection *reflection = message.GetReflection();
  const int field_count = descriptor->field_count();
  bool result = true;
  for (size_t i = 0; i < field_count; ++i) {
    const FieldDescriptor *field = descriptor->field(i);
    if (!field) {
      result = false;
      continue;
    }
    if (field->is_repeated()) {
      Json::Value *items = &(*value)[field->name()];
      *items = Json::Value(Json::arrayValue);
      const int count = reflection->FieldSize(message, field);
      for (int j = 0; j < count; ++j) {
        if (!ProtobufRepeatedFieldValueToJsonValue(message, *reflection, *field,
                                                   j, &(*items)[j])) {
          result = false;
        }
      }
    } else {
      if (reflection->HasField(message, field) || field->is_required()) {
        if (!ProtobufFieldValueToJsonValue(message, *reflection, *field,
                                           &(*value)[field->name()])) {
          result = false;
        }
      }
    }
  }
  return result;
}

bool JsonUtil::JsonValueToProtobufMessage(const Json::Value &value,
                                          Message *message) {
  const Descriptor *descriptor = message->GetDescriptor();
  const Reflection *reflection = message->GetReflection();
  const Json::Value::Members &members = value.getMemberNames();

  bool result = true;
  for (size_t i = 0; i < members.size(); ++i) {
    const FieldDescriptor *field = descriptor->FindFieldByName(members[i]);
    if (!field) {
      DLOG(ERROR) << "Unknown field: \"" << members[i] << "\"";
      result = false;
      continue;
    }
    if (field->is_repeated()) {
      if (!value[members[i]].isArray()) {
        DLOG(ERROR) << "\"" << members[i] << "\" is repeated."
                    << "But json is not array";
        result = false;
        continue;
      }
      if (!JsonValueToProtobufRepeatedFieldValue(value[members[i]], field,
                                                 reflection, message)) {
        DLOG(ERROR) << "JsonValueToProtobufRepeatedFieldValue error: \""
                    << members[i] << "\"";
        result = false;
      }
    } else {
      if (!JsonValueToProtobufFieldValue(value[members[i]], field, reflection,
                                         message)) {
        DLOG(ERROR) << "JsonValueToProtobufFieldValue error: \"" << members[i]
                    << "\"";
        result = false;
      }
    }
  }
  return result;
}

}  // namespace net
}  // namespace mozc
