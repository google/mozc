// Copyright 2010-2013, Google Inc.
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

#include <string>
#include <vector>

#include "net/json_util.h"

#include "base/base.h"
#include "base/testing_util.h"
#include "base/util.h"
#include "net/jsoncpp.h"
#include "net/json_util_test.pb.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace net {

TEST(JsonUtilTest, EmptyTest) {
  TestMsg msg;
  Json::Value json_value;
  EXPECT_TRUE(JsonUtil::ProtobufMessageToJsonValue(msg, &json_value));
  Json::Value expected_value(Json::objectValue);
  EXPECT_EQ(expected_value, json_value);
  TestMsg new_msg;
  EXPECT_TRUE(JsonUtil::JsonValueToProtobufMessage(json_value, &new_msg));
  EXPECT_PROTO_EQ(msg, new_msg);
}

TEST(JsonUtilTest, ConvertItemTest) {
#define TEST_CONVERT_ITEM(proto_setter, proto_value, json_name, json_value) \
  { \
    TestMsg msg; \
    msg.proto_setter(proto_value); \
    Json::Value value; \
    EXPECT_TRUE(JsonUtil::ProtobufMessageToJsonValue(msg, &value)); \
    Json::Value expected_value(Json::objectValue); \
    expected_value[json_name] = json_value; \
    EXPECT_EQ(expected_value, value); \
    TestMsg new_msg; \
    EXPECT_TRUE(JsonUtil::JsonValueToProtobufMessage(value, &new_msg)); \
    EXPECT_PROTO_EQ(msg, new_msg); \
  }
  TEST_CONVERT_ITEM(set_double_value, 1.0, "double_value", 1.0)
  TEST_CONVERT_ITEM(set_float_value, 2.0, "float_value", 2.0)
  TEST_CONVERT_ITEM(set_int32_value, 3, "int32_value", Json::Int(3))
  TEST_CONVERT_ITEM(set_int64_value, 4, "int64_value", Json::Int64(4))
  TEST_CONVERT_ITEM(set_uint32_value, 5, "uint32_value", Json::UInt(5))
  TEST_CONVERT_ITEM(set_uint64_value, 6, "uint64_value", Json::UInt64(6))
  TEST_CONVERT_ITEM(set_sint32_value, 7, "sint32_value", Json::Int(7))
  TEST_CONVERT_ITEM(set_sint64_value, 8, "sint64_value", Json::Int64(8))
  TEST_CONVERT_ITEM(set_fixed32_value, 9, "fixed32_value", Json::UInt(9))
  TEST_CONVERT_ITEM(set_fixed64_value, 10, "fixed64_value", Json::UInt64(10))
  TEST_CONVERT_ITEM(set_sfixed32_value, 11, "sfixed32_value", Json::Int(11))
  TEST_CONVERT_ITEM(set_sfixed64_value, 12, "sfixed64_value", Json::Int64(12))
  TEST_CONVERT_ITEM(set_bool_value, true, "bool_value", true)
  TEST_CONVERT_ITEM(set_bool_value, false, "bool_value", false)
  TEST_CONVERT_ITEM(set_string_value, "string", "string_value", "string")
  TEST_CONVERT_ITEM(set_bytes_value, "bytes", "bytes_value", "bytes")
  TEST_CONVERT_ITEM(set_enum_value, ENUM_A, "enum_value", "ENUM_A")
  TEST_CONVERT_ITEM(set_innerenum_value, TestMsg::ENUM_1,
                    "innerenum_value", "ENUM_1")
#undef TEST_CONVERT_ITEM
}

TEST(JsonUtilTest, ConvertRepeatedItemTest) {
#define TEST_CONVERT_REPEATED_ITEM( \
    proto_adder, proto_value1, proto_value2, proto_value3, \
    json_name, json_value1, json_value2, json_value3) \
  { \
    TestMsg msg; \
    msg.proto_adder(proto_value1); \
    msg.proto_adder(proto_value2); \
    msg.proto_adder(proto_value3); \
    Json::Value value; \
    EXPECT_TRUE(JsonUtil::ProtobufMessageToJsonValue(msg, &value)); \
    Json::Value expected_value(Json::objectValue); \
    expected_value[json_name].append(json_value1); \
    expected_value[json_name].append(json_value2); \
    expected_value[json_name].append(json_value3); \
    EXPECT_EQ(expected_value, value); \
    TestMsg new_msg; \
    EXPECT_TRUE(JsonUtil::JsonValueToProtobufMessage(value, &new_msg)); \
    EXPECT_PROTO_EQ(msg, new_msg); \
  }
  TEST_CONVERT_REPEATED_ITEM(add_repeated_double_value, 1.0, 2.0, 3.0,
                             "repeated_double_value", 1.0, 2.0, 3.0)
  TEST_CONVERT_REPEATED_ITEM(add_repeated_float_value, 1.0, 2.0, 3.0,
                             "repeated_float_value", 1.0, 2.0, 3.0)
  TEST_CONVERT_REPEATED_ITEM(add_repeated_int32_value, 1, 2, 3,
                             "repeated_int32_value",
                             Json::Int(1), Json::Int(2), Json::Int(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_int64_value, 1, 2, 3,
                             "repeated_int64_value",
                             Json::Int64(1), Json::Int64(2), Json::Int64(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_uint32_value, 1, 2, 3,
                             "repeated_uint32_value",
                             Json::UInt(1), Json::UInt(2), Json::UInt(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_uint64_value, 1, 2, 3,
                             "repeated_uint64_value",
                             Json::UInt64(1), Json::UInt64(2), Json::UInt64(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_sint32_value, 1, 2, 3,
                             "repeated_sint32_value",
                             Json::Int(1), Json::Int(2), Json::Int(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_sint64_value, 1, 2, 3,
                             "repeated_sint64_value",
                             Json::Int64(1), Json::Int64(2), Json::Int64(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_fixed32_value, 1, 2, 3,
                             "repeated_fixed32_value",
                             Json::UInt(1), Json::UInt(2), Json::UInt(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_fixed64_value, 1, 2, 3,
                             "repeated_fixed64_value",
                             Json::UInt64(1), Json::UInt64(2), Json::UInt64(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_sfixed32_value, 1, 2, 3,
                             "repeated_sfixed32_value",
                             Json::Int(1), Json::Int(2), Json::Int(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_sfixed64_value, 1, 2, 3,
                             "repeated_sfixed64_value",
                             Json::Int64(1), Json::Int64(2), Json::Int64(3))
  TEST_CONVERT_REPEATED_ITEM(add_repeated_bool_value, true, true, false,
                             "repeated_bool_value", true, true, false)
  TEST_CONVERT_REPEATED_ITEM(add_repeated_string_value, "ABC", "DEF", "GHQ",
                             "repeated_string_value", "ABC", "DEF", "GHQ")
  TEST_CONVERT_REPEATED_ITEM(add_repeated_bytes_value, "ABC", "DEF", "GHQ",
                             "repeated_bytes_value", "ABC", "DEF", "GHQ")
  TEST_CONVERT_REPEATED_ITEM(add_repeated_enum_value, ENUM_A, ENUM_C, ENUM_B,
                             "repeated_enum_value",
                             "ENUM_A", "ENUM_C", "ENUM_B")
  TEST_CONVERT_REPEATED_ITEM(add_repeated_innerenum_value,
                             TestMsg::ENUM_1, TestMsg::ENUM_2, TestMsg::ENUM_0,
                             "repeated_innerenum_value",
                             "ENUM_1", "ENUM_2", "ENUM_0")
#undef TEST_CONVERT_REPEATED_ITEM
}

TEST(JsonUtilTest, SubMsgTest) {
  {
    TestMsg msg;
    SubMsg *sub_msg = msg.mutable_sub_message();
    sub_msg->set_double_value(100.0);
    sub_msg->set_float_value(200.0);
    Json::Value value;
    EXPECT_TRUE(JsonUtil::ProtobufMessageToJsonValue(msg, &value));
    Json::Value expected_value(Json::objectValue);
    expected_value["sub_message"]["double_value"] = 100.0;
    expected_value["sub_message"]["float_value"] = 200.0;
    EXPECT_EQ(expected_value, value);
    TestMsg new_msg;
    EXPECT_TRUE(JsonUtil::JsonValueToProtobufMessage(value, &new_msg));
    EXPECT_PROTO_EQ(msg, new_msg);
  }
  {
    TestMsg msg;
    SubMsg *sub_msg1 = msg.add_repeated_sub_message();
    msg.add_repeated_sub_message();
    SubMsg *sub_msg3 = msg.add_repeated_sub_message();
    sub_msg1->set_double_value(100.0);
    sub_msg3->set_float_value(200.0);
    sub_msg3->add_repeated_bool_value(true);
    sub_msg3->add_repeated_bool_value(false);
    Json::Value value;
    EXPECT_TRUE(JsonUtil::ProtobufMessageToJsonValue(msg, &value));
    Json::Value expected_value(Json::objectValue);
    Json::Value sub_value1(Json::objectValue);
    Json::Value sub_value2(Json::objectValue);
    Json::Value sub_value3(Json::objectValue);
    sub_value1["double_value"] = 100.0;
    sub_value3["float_value"] = 200.0;
    sub_value3["repeated_bool_value"].append(true);
    sub_value3["repeated_bool_value"].append(false);
    expected_value["repeated_sub_message"].append(sub_value1);
    expected_value["repeated_sub_message"].append(sub_value2);
    expected_value["repeated_sub_message"].append(sub_value3);
    EXPECT_EQ(expected_value, value);
    TestMsg new_msg;
    EXPECT_TRUE(JsonUtil::JsonValueToProtobufMessage(value, &new_msg));
    EXPECT_PROTO_EQ(msg, new_msg);
  }
}

TEST(JsonUtilTest, CombinedTest) {
  TestMsg msg;
  msg.set_double_value(1.0);
  msg.set_float_value(2.0);
  msg.set_int32_value(3);
  SubMsg *sub_msg = msg.mutable_sub_message();
  sub_msg->set_string_value("123");
  sub_msg->add_repeated_int32_value(10);
  sub_msg->add_repeated_int32_value(20);
  sub_msg->add_repeated_string_value("abc");
  SubMsg *repeated_sub_msg1 = msg.add_repeated_sub_message();
  SubMsg *repeated_sub_msg2 = msg.add_repeated_sub_message();
  repeated_sub_msg1->set_uint32_value(12);
  repeated_sub_msg2->add_repeated_enum_value(ENUM_C);
  repeated_sub_msg2->add_repeated_enum_value(ENUM_A);

  Json::Value value;
  EXPECT_TRUE(JsonUtil::ProtobufMessageToJsonValue(msg, &value));
  Json::Value expected_value(Json::objectValue);
  Json::Value sub_value(Json::objectValue);
  Json::Value repeated_sub_value1(Json::objectValue);
  Json::Value repeated_sub_value2(Json::objectValue);
  expected_value["double_value"] = 1.0;
  expected_value["float_value"] = 2.0;
  expected_value["int32_value"] = Json::Int(3);
  expected_value["sub_message"]["string_value"] = "123";
  expected_value["sub_message"]["repeated_int32_value"].append(Json::Int(10));
  expected_value["sub_message"]["repeated_int32_value"].append(Json::Int(20));
  expected_value["sub_message"]["repeated_string_value"].append("abc");
  repeated_sub_value1["uint32_value"] = Json::UInt(12);
  expected_value["repeated_sub_message"].append(repeated_sub_value1);
  repeated_sub_value2["repeated_enum_value"].append("ENUM_C");
  repeated_sub_value2["repeated_enum_value"].append("ENUM_A");
  expected_value["repeated_sub_message"].append(repeated_sub_value2);
  EXPECT_EQ(expected_value, value);
  TestMsg new_msg;
  EXPECT_TRUE(JsonUtil::JsonValueToProtobufMessage(value, &new_msg));
  EXPECT_PROTO_EQ(msg, new_msg);
}

TEST(JsonUtilTest, FailureTest) {
  const char *kNumValueKeys[] = {
    "double_value", "float_value", "int32_value", "int64_value",
    "uint32_value", "uint64_value", "sint32_value", "sint64_value",
    "fixed32_value", "fixed64_value", "sfixed32_value", "sfixed64_value"};
  for (size_t i = 0; i <  arraysize(kNumValueKeys); ++i) {
    {
      Json::Value json_value;
      json_value[kNumValueKeys[i]] = "str";
      TestMsg msg;
      EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
    }
    {
      Json::Value json_value;
      json_value[kNumValueKeys[i]].append(1);
      TestMsg msg;
      EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
    }
    {
      Json::Value json_value;
      json_value[kNumValueKeys[i]]["test"] = 1;
      TestMsg msg;
      EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
    }
  }
  const char *kUnsignedNumValueKeys[] = {"uint32_value", "uint64_value"};
  for (size_t i = 0; i <  arraysize(kUnsignedNumValueKeys); ++i) {
    Json::Value json_value;
    json_value[kUnsignedNumValueKeys[i]] = -1;
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["bool_value"] = "str";
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }

  {
    Json::Value json_value;
    json_value["string_value"].append(1);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["string_value"]["test"] = 1;
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["bytes_value"].append(1);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["bytes_value"]["test"] = 1;
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["enum_value"] = "ENUM_X";
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["enum_value"].append(1);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["enum_value"]["test"] = 1;
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["innerenum_value"] = "ENUM_X";
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["innerenum_value"].append(1);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["innerenum_value"]["test"] = 1;
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  const char *kRepeatedNumValueKeys[] = {
    "repeated_double_value", "repeated_float_value", "repeated_int32_value",
    "repeated_int64_value", "repeated_uint32_value", "repeated_uint64_value",
    "repeated_sint32_value", "repeated_sint64_value", "repeated_fixed32_value",
    "repeated_fixed64_value", "repeated_sfixed32_value",
    "repeated_sfixed64_value"};
  for (size_t i = 0; i <  arraysize(kRepeatedNumValueKeys); ++i) {
    {
      Json::Value json_value;
      json_value[kRepeatedNumValueKeys[i]] = "str";
      TestMsg msg;
      EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
    }
    {
      Json::Value json_value;
      json_value[kRepeatedNumValueKeys[i]] = 1;
      TestMsg msg;
      EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
    }
    {
      Json::Value json_value;
      json_value[kRepeatedNumValueKeys[i]]["test"] = 1;
      TestMsg msg;
      EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
    }
  }
  const char *kRepeatedUnsignedNumValueKeys[] =
      {"repeated_uint32_value", "repeated_uint64_value"};
  for (size_t i = 0; i <  arraysize(kRepeatedUnsignedNumValueKeys); ++i) {
    Json::Value json_value;
    json_value[kRepeatedUnsignedNumValueKeys[i]].append(0);
    json_value[kRepeatedUnsignedNumValueKeys[i]].append(-1);
    json_value[kRepeatedUnsignedNumValueKeys[i]].append(2);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["repeated_bool_value"].append(true);
    json_value["repeated_bool_value"].append("xxx");
    json_value["repeated_bool_value"].append(false);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }

  {
    Json::Value json_value;
    json_value["repeated_string_value"].append("xx");
    json_value["repeated_string_value"].append("xxx");
    Json::Value array_value;
    array_value.append(1);
    json_value["repeated_string_value"].append(array_value);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["repeated_bytes_value"].append("xx");
    json_value["repeated_bytes_value"].append("xxx");
    Json::Value array_value;
    array_value.append(1);
    json_value["repeated_bytes_value"].append(array_value);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["repeated_enum_value"].append("ENUM_A");
    json_value["repeated_enum_value"].append("ENUM_B");
    TestMsg msg1, msg2;
    EXPECT_TRUE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg1));
    json_value["repeated_enum_value"].append("ENUM_X");
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg2));
  }
  {
    Json::Value json_value;
    json_value["repeated_enum_value"].append("xx");
    json_value["repeated_enum_value"].append("xxx");
    Json::Value array_value;
    array_value.append(1);
    json_value["repeated_enum_value"].append(array_value);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["repeated_innerenum_value"].append("ENUM_0");
    json_value["repeated_innerenum_value"].append("ENUM_1");
    TestMsg msg1, msg2;
    EXPECT_TRUE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg1));
    json_value["repeated_innerenum_value"].append("ENUM_X");
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg2));
  }
  {
    Json::Value json_value;
    json_value["repeated_innerenum_value"].append("xx");
    json_value["repeated_innerenum_value"].append("xxx");
    Json::Value array_value;
    array_value.append(1);
    json_value["repeated_innerenum_value"].append(array_value);
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["nested_message"] = 1;
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
  {
    Json::Value json_value;
    json_value["repeated_nested_message"] = 1;
    TestMsg msg;
    EXPECT_FALSE(JsonUtil::JsonValueToProtobufMessage(json_value, &msg));
  }
}

}  // namespace net
}  // namespace mozc
