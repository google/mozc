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

#include "converter/inner_segment.h"

#include <string>
#include <tuple>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "testing/gunit.h"

namespace mozc {
namespace converter {

using SVList = std::vector<absl::string_view>;

std::tuple<SVList, SVList, SVList, SVList, SVList, SVList> GetSegments(
    const InnerSegments& inner_segments) {
  SVList keys, values, content_keys, content_values, functional_keys,
      functional_values;

  for (const auto iter : inner_segments) {
    keys.push_back(iter.GetKey());
    values.push_back(iter.GetValue());
    content_keys.push_back(iter.GetContentKey());
    content_values.push_back(iter.GetContentValue());
    functional_keys.push_back(iter.GetFunctionalKey());
    functional_values.push_back(iter.GetFunctionalValue());
  }

  return {keys,           values,          content_keys,
          content_values, functional_keys, functional_values};
}

TEST(InnerSegment, InnerSegmentIterator) {
  const std::string key = "testfoobar";
  const std::string value = "redgreenblue";

  const InnerSegmentBoundary boundary =
      BuildInnerSegmentBoundary({{4, 3, 4, 3}, {6, 9, 3, 5}}, key, value);

  const InnerSegments inner_segments(key, value, boundary);

  EXPECT_EQ(inner_segments.size(), 2);

  const auto [keys, values, content_keys, content_values, functional_keys,
              functional_values] = GetSegments(inner_segments);

  // Tests stream operation.
  LOG(ERROR) << inner_segments;

  ASSERT_EQ(keys.size(), 2);
  EXPECT_EQ(keys[0], "test");
  EXPECT_EQ(keys[1], "foobar");

  ASSERT_EQ(values.size(), 2);
  EXPECT_EQ(values[0], "red");
  EXPECT_EQ(values[1], "greenblue");

  ASSERT_EQ(content_keys.size(), 2);
  EXPECT_EQ(content_keys[0], "test");
  EXPECT_EQ(content_keys[1], "foo");

  ASSERT_EQ(content_values.size(), 2);
  EXPECT_EQ(content_values[0], "red");
  EXPECT_EQ(content_values[1], "green");

  ASSERT_EQ(functional_keys.size(), 2);
  EXPECT_EQ(functional_keys[0], "");
  EXPECT_EQ(functional_keys[1], "bar");

  ASSERT_EQ(functional_values.size(), 2);
  EXPECT_EQ(functional_values[0], "");
  EXPECT_EQ(functional_values[1], "blue");

  const auto [merged_content_key, merged_content_value] =
      inner_segments.GetMergedContentKeyAndValue();
  EXPECT_EQ(merged_content_key, "testfoo");
  EXPECT_EQ(merged_content_value, "redgreen");
}

TEST(InnerSegmentBoundary, InnerSegmentIteratorEmpty) {
  // boundary info is not specified.
  {
    const InnerSegments inner_segment_boundary("abc", "ABC", {});
    EXPECT_EQ(inner_segment_boundary.size(), 1);

    const auto [keys, values, content_keys, content_values, functional_keys,
                functional_values] = GetSegments(inner_segment_boundary);

    ASSERT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], "abc");
    EXPECT_EQ(values[0], "ABC");
    EXPECT_EQ(content_keys[0], "abc");
    EXPECT_EQ(content_values[0], "ABC");
    EXPECT_EQ(functional_keys[0], "");
    EXPECT_EQ(functional_values[0], "");
  }

  {
    // Respects the length of content_key and content_value.
    const InnerSegments inner_segment_boundary("abc", "ABC", "ab", "A", {});
    EXPECT_EQ(inner_segment_boundary.size(), 1);

    const auto [keys, values, content_keys, content_values, functional_keys,
                functional_values] = GetSegments(inner_segment_boundary);

    ASSERT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], "abc");
    EXPECT_EQ(values[0], "ABC");
    EXPECT_EQ(content_keys[0], "ab");
    EXPECT_EQ(content_values[0], "A");
    EXPECT_EQ(functional_keys[0], "c");
    EXPECT_EQ(functional_values[0], "BC");
  }
}

TEST(InnerSegment, InnerSegmentIteratorInvalid) {
  // Invalid boundary.
  {
    const InnerSegmentBoundary boundary =
        BuildInnerSegmentBoundary({{2, 3, 2, 3}}, "ab", "ABC");
    EXPECT_EQ(boundary.size(), 1);

    const InnerSegments inner_segments("abc", "ABC", boundary);
    EXPECT_EQ(inner_segments.size(), 1);

    const auto [keys, values, content_keys, content_values, functional_keys,
                functional_values] = GetSegments(inner_segments);

    ASSERT_EQ(keys.size(), 2);
    EXPECT_EQ(keys[0], "ab");
    EXPECT_EQ(keys[1], "c");
    EXPECT_EQ(values[0], "ABC");
    EXPECT_EQ(values[1], "");
    EXPECT_EQ(content_keys[0], "ab");
    EXPECT_EQ(content_keys[1], "c");
    EXPECT_EQ(content_values[0], "ABC");
    EXPECT_EQ(content_values[1], "");
  }

  // Boundary info is not enough. Remained parts are handled as one segment.
  {
    const InnerSegmentBoundary boundary =
        BuildInnerSegmentBoundary({{1, 1, 1, 1}}, "a", "A");

    const InnerSegments inner_segments("abc", "ABC", boundary);
    EXPECT_EQ(inner_segments.size(), 1);

    const auto [keys, values, content_keys, content_values, functional_keys,
                functional_values] = GetSegments(inner_segments);

    ASSERT_EQ(keys.size(), 2);
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[1], "bc");
    EXPECT_EQ(values[0], "A");
    EXPECT_EQ(values[1], "BC");
    EXPECT_EQ(content_keys[0], "a");
    EXPECT_EQ(content_keys[1], "bc");
    EXPECT_EQ(content_values[0], "A");
    EXPECT_EQ(content_values[1], "BC");
  }

  // Too many boundaries. Ignores the remained info.
  {
    const InnerSegmentBoundary boundary = BuildInnerSegmentBoundary(
        {{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}},
        "aaaaa", "AAAAA");

    const InnerSegments inner_segments("abc", "ABC", boundary);
    // size() checks inner_segment_boundary.size(), so no the same as
    // the actual size.
    EXPECT_EQ(inner_segments.size(), 5);

    const auto [keys, values, content_keys, content_values, functional_keys,
                functional_values] = GetSegments(inner_segments);

    ASSERT_EQ(keys.size(), 3);
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[1], "b");
    EXPECT_EQ(keys[2], "c");
    EXPECT_EQ(values[0], "A");
    EXPECT_EQ(values[1], "B");
    EXPECT_EQ(values[2], "C");
    EXPECT_EQ(content_keys[0], "a");
    EXPECT_EQ(content_keys[1], "b");
    EXPECT_EQ(content_keys[2], "c");
    EXPECT_EQ(content_values[0], "A");
    EXPECT_EQ(content_values[1], "B");
    EXPECT_EQ(content_values[2], "C");
  }

  // empty key/value.
  {
    const InnerSegments inner_segments("", "", {1});
    EXPECT_EQ(inner_segments.size(), 0);

    const auto [keys, values, content_keys, content_values, functional_keys,
                functional_values] = GetSegments(inner_segments);
    ASSERT_EQ(keys.size(), 0);
  }

  // empty key.  allows empty key as value is not empty.
  {
    const InnerSegments inner_segments("", "value", {});
    EXPECT_EQ(inner_segments.size(), 1);

    const auto [keys, values, content_keys, content_values, functional_keys,
                functional_values] = GetSegments(inner_segments);
    ASSERT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], "");
    EXPECT_EQ(values[0], "value");
  }
}

TEST(InnerSegments, GetSuffixKeyAndValue) {
  auto suffix_key_default = [](const InnerSegments& inner_segments) {
    return inner_segments.GetSuffixKeyAndValue().first;
  };

  auto suffix_value_default = [](const InnerSegments& inner_segments) {
    return inner_segments.GetSuffixKeyAndValue().second;
  };

  auto suffix_key = [](const InnerSegments& inner_segments, int size) {
    return inner_segments.GetSuffixKeyAndValue(size).first;
  };

  auto suffix_value = [](const InnerSegments& inner_segments, int size) {
    return inner_segments.GetSuffixKeyAndValue(size).second;
  };

  {
    std::string all_key, all_value;

    converter::InnerSegmentBoundaryBuilder builder;

    auto add_segment = [&](absl::string_view key, absl::string_view value) {
      builder.Add(key.size(), value.size(), key.size(), value.size());
      absl::StrAppend(&all_key, key);
      absl::StrAppend(&all_value, value);
    };

    for (int i = 0; i < 3; ++i) {
      add_segment(absl::StrCat("k", i), absl::StrCat("v", i));
    }

    InnerSegmentBoundary inner_segment_boundary =
        builder.Build(all_key, all_value);

    const InnerSegments inner_segments(all_key, all_value,
                                       inner_segment_boundary);

    EXPECT_EQ(inner_segments.size(), 3);
    EXPECT_EQ(suffix_key_default(inner_segments), "k0k1k2");
    EXPECT_EQ(suffix_value_default(inner_segments), "v0v1v2");
    EXPECT_EQ(suffix_key(inner_segments, -1), "k0k1k2");
    EXPECT_EQ(suffix_value(inner_segments, -1), "v0v1v2");
    EXPECT_EQ(suffix_key(inner_segments, 10), "k0k1k2");
    EXPECT_EQ(suffix_value(inner_segments, 10), "v0v1v2");
    EXPECT_EQ(suffix_key(inner_segments, 0), "");
    EXPECT_EQ(suffix_value(inner_segments, 0), "");
    EXPECT_EQ(suffix_key(inner_segments, 1), "k2");
    EXPECT_EQ(suffix_value(inner_segments, 1), "v2");
    EXPECT_EQ(suffix_key(inner_segments, 2), "k1k2");
    EXPECT_EQ(suffix_value(inner_segments, 2), "v1v2");
    EXPECT_EQ(suffix_key(inner_segments, 3), "k0k1k2");
    EXPECT_EQ(suffix_value(inner_segments, 3), "v0v1v2");
    EXPECT_EQ(suffix_key(inner_segments, 4), "k0k1k2");
    EXPECT_EQ(suffix_value(inner_segments, 4), "v0v1v2");
  }

  {
    const InnerSegments inner_segments("", "", {});

    EXPECT_EQ(inner_segments.size(), 0);
    EXPECT_EQ(suffix_key_default(inner_segments), "");
    EXPECT_EQ(suffix_value_default(inner_segments), "");
    EXPECT_EQ(suffix_key(inner_segments, -1), "");
    EXPECT_EQ(suffix_value(inner_segments, -1), "");
    EXPECT_EQ(suffix_key(inner_segments, 10), "");
    EXPECT_EQ(suffix_value(inner_segments, 10), "");
    EXPECT_EQ(suffix_key(inner_segments, 0), "");
    EXPECT_EQ(suffix_value(inner_segments, 0), "");
    EXPECT_EQ(suffix_key(inner_segments, 2), "");
    EXPECT_EQ(suffix_value(inner_segments, 2), "");
    EXPECT_EQ(suffix_key(inner_segments, 1), "");
    EXPECT_EQ(suffix_value(inner_segments, 1), "");
  }

  {
    const InnerSegments inner_segments("key", "value", {});
    EXPECT_EQ(inner_segments.size(), 1);
    EXPECT_EQ(suffix_key_default(inner_segments), "key");
    EXPECT_EQ(suffix_value_default(inner_segments), "value");
    EXPECT_EQ(suffix_key(inner_segments, -1), "key");
    EXPECT_EQ(suffix_value(inner_segments, -1), "value");
    EXPECT_EQ(suffix_key(inner_segments, 1), "key");
    EXPECT_EQ(suffix_value(inner_segments, 1), "value");
    EXPECT_EQ(suffix_key(inner_segments, 2), "key");
    EXPECT_EQ(suffix_value(inner_segments, 2), "value");
  }
}

}  // namespace converter
}  // namespace mozc
