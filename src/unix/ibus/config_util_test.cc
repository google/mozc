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

// Test for config_util

#include "unix/ibus/config_util.h"

#include "testing/base/public/gunit.h"
#include "session/config.pb.h"

TEST(ConfigUtilTest, IntValue) {
  mozc::config::Config config;
  EXPECT_EQ("", config.DebugString());

#if IBUS_CHECK_VERSION(1, 3, 99)
  GVariant *value = g_variant_new_int32(42);
#else
  g_type_init();
  GValue actual_value = {0};
  GValue *value = &actual_value;
  g_value_init(value, G_TYPE_INT);
  g_value_set_int(value, 42);
#endif
  mozc::ibus::ConfigUtil::SetFieldForName(
      "config_version", value, &config);

  EXPECT_EQ("config_version: 42\n",
            config.DebugString());
#if IBUS_CHECK_VERSION(1, 3, 99)
  g_variant_unref(value);
#else
  g_value_unset(value);
#endif
}

TEST(ConfigUtilTest, EnumValue) {
  mozc::config::Config config;
  EXPECT_EQ("", config.DebugString());

#if IBUS_CHECK_VERSION(1, 3, 99)
  GVariant *value = g_variant_new_string("ROMAN");
#else
  g_type_init();
  GValue actual_value = {0};
  GValue *value = &actual_value;
  g_value_init(value, G_TYPE_STRING);
  g_value_set_static_string(value, "ROMAN");
#endif
  mozc::ibus::ConfigUtil::SetFieldForName(
      "preedit_method", value, &config);

  EXPECT_EQ("preedit_method: ROMAN\n",
            config.DebugString());
#if IBUS_CHECK_VERSION(1, 3, 99)
  g_variant_unref(value);
#else
  g_value_unset(value);
#endif
}

TEST(ConfigUtilTest, BooleanValue) {
  mozc::config::Config config;
  EXPECT_EQ("", config.DebugString());

#if IBUS_CHECK_VERSION(1, 3, 99)
  GVariant *value = g_variant_new_boolean(true);
#else
  g_type_init();
  GValue actual_value = {0};
  GValue *value = &actual_value;
  g_value_init(value, G_TYPE_BOOLEAN);
  g_value_set_boolean(value, true);
#endif
  mozc::ibus::ConfigUtil::SetFieldForName(
      "incognito_mode", value, &config);

  EXPECT_EQ("incognito_mode: true\n",
            config.DebugString());
#if IBUS_CHECK_VERSION(1, 3, 99)
  g_variant_unref(value);
#else
  g_value_unset(value);
#endif
}

TEST(ConfigUtilTest, NoSuchField) {
  mozc::config::Config config;
  EXPECT_EQ("", config.DebugString());

#if IBUS_CHECK_VERSION(1, 3, 99)
  GVariant *value = g_variant_new_int32(10);
#else
  g_type_init();
  GValue actual_value = {0};
  GValue *value = &actual_value;
  g_value_init(value, G_TYPE_INT);
  g_value_set_int(value, 10);
#endif
  mozc::ibus::ConfigUtil::SetFieldForName(
      "no_such_field", value, &config);

  // Does not affect the config value.
  EXPECT_EQ("", config.DebugString());
#if IBUS_CHECK_VERSION(1, 3, 99)
  g_variant_unref(value);
#else
  g_value_unset(value);
#endif
}

TEST(ConfigUtilTest, TypeMismatch) {
  mozc::config::Config config;
  EXPECT_EQ("", config.DebugString());

#if IBUS_CHECK_VERSION(1, 3, 99)
  GVariant *value = g_variant_new_int32(10);
#else
  g_type_init();
  GValue actual_value = {0};
  GValue *value = &actual_value;
  g_value_init(value, G_TYPE_INT);
  g_value_set_int(value, 10);
#endif

  // enum field
  mozc::ibus::ConfigUtil::SetFieldForName(
      "preedit_method", value, &config);

  // Does not affect the config value.
  EXPECT_EQ("", config.DebugString());
#if IBUS_CHECK_VERSION(1, 3, 99)
  g_variant_unref(value);
#else
  g_value_unset(value);
#endif
}
