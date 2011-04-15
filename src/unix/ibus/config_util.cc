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

#include "unix/ibus/config_util.h"

#include "base/base.h"
#include "base/protobuf/descriptor.h"

namespace mozc {
namespace ibus {
#if IBUS_CHECK_VERSION(1, 3, 99)
// IBus-1.4 uses Glib's GVariant for configration.
bool ConfigUtil::GetString(GVariant *value, const gchar **out_string) {
  if (g_variant_classify(value) != G_VARIANT_CLASS_STRING) {
    return false;
  }
  *out_string = g_variant_get_string(value, NULL);
  return true;
}

bool ConfigUtil::GetInteger(GVariant *value, gint *out_integer) {
  if (g_variant_classify(value) != G_VARIANT_CLASS_INT32) {
    return false;
  }
  *out_integer = g_variant_get_int32(value);
  return true;
}

bool ConfigUtil::GetBoolean(GVariant *value, gboolean *out_boolean) {
  if (g_variant_classify(value) != G_VARIANT_CLASS_BOOLEAN) {
    return false;
  }
  *out_boolean = g_variant_get_boolean(value);
  return true;
}
#else
// IBus-1.2 and 1.3 use GValue for configration.
bool ConfigUtil::GetString(GValue *value, const gchar **out_string) {
  if (!G_VALUE_HOLDS_STRING(value)) {
    return false;
  }
  *out_string = g_value_get_string(value);
  return true;
}

bool ConfigUtil::GetInteger(GValue *value, gint *out_integer) {
  if (!G_VALUE_HOLDS_INT(value)) {
    return false;
  }
  *out_integer = g_value_get_int(value);
  return true;
}

bool ConfigUtil::GetBoolean(GValue *value, gboolean *out_boolean) {
  if (!G_VALUE_HOLDS_BOOLEAN(value)) {
    return false;
  }
  *out_boolean = g_value_get_boolean(value);
  return true;
}
#endif

void ConfigUtil::SetFieldForName(const gchar *name,
#if IBUS_CHECK_VERSION(1, 3, 99)
                                 GVariant *value,
#else
                                 GValue *value,
#endif
                                 protobuf::Message *result) {
  if (!name || !value) {
    LOG(ERROR) << "name or value is not specified";
    return;
  }
  DCHECK(result);

  const google::protobuf::Descriptor *descriptor = result->GetDescriptor();
  const google::protobuf::Reflection *reflection = result->GetReflection();
  const google::protobuf::FieldDescriptor *field_to_update =
      descriptor->FindFieldByName(name);

  if (!field_to_update) {
    LOG(ERROR) << "Unknown field name: " << name;
    return;
  }

  // Set |value| to |result|.
  switch (field_to_update->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
      // |value| should be STRING.
      const gchar *string_value = NULL;
      if (!GetString(value, &string_value)) {
        LOG(ERROR) << "Bad value type for " << name;
        return;
      }
      DCHECK(string_value);
      const google::protobuf::EnumValueDescriptor *enum_value =
          descriptor->FindEnumValueByName(string_value);
      if (!enum_value) {
        LOG(ERROR) << "Bad value for " << name << ": " << string_value;
        return;
      }
      reflection->SetEnum(result, field_to_update, enum_value);
      VLOG(2) << "setting field: " << name << " = " << string_value;
      break;
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
      // unsigned int is not supported as chrome's preference type and int is
      // used as an alternative type, so |value| should be INT.
      gint int_value = -1;
      if (!GetInteger(value, &int_value)) {
        LOG(ERROR) << "Bad value type for " << name;
        return;
      }
      reflection->SetUInt32(result, field_to_update, int_value);
      VLOG(2) << "setting field: " << name << " = " << int_value;
      break;
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
      // |value| should be BOOLEAN.
      gboolean boolean_value = FALSE;
      if (!GetBoolean(value, &boolean_value)) {
        LOG(ERROR) << "Bad value type for " << name;
        return;
      }
      reflection->SetBool(result, field_to_update, boolean_value);
      VLOG(2) << "setting field: " << name << " = "
              << (boolean_value ? "true" : "false");
      break;
    }
    default: {
      // TODO(yusukes): Support other types.
      LOG(ERROR) << "Unknown or unsupported type: " << name << ": "
                 << field_to_update->cpp_type();
      return;
    }
  }
}

}  // namespace ibus
}  // namespace mozc
