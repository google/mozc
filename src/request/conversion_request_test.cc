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

#include "request/conversion_request.h"

#include <memory>
#include <string>
#include <utility>

#include "composer/composer.h"
#include "composer/table.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {

TEST(CopyOrViewPtrTest, BasicTest) {
  {
    // Default constructor is view.
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr(s);
    EXPECT_EQ(*ptr, s);
    EXPECT_EQ(&*ptr, &s);
  }

  {
    // Copy
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    EXPECT_FALSE(ptr);
    ptr.copy_from(s);
    EXPECT_EQ(*ptr, s);
    EXPECT_NE(&*ptr, &s);
  }

  {
    // View
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    EXPECT_FALSE(ptr);
    ptr.set_view(s);
    EXPECT_EQ(*ptr, s);
    EXPECT_EQ(&*ptr, &s);
  }

  {
    // Move
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    EXPECT_FALSE(ptr);
    ptr.move_from(std::move(s));
    EXPECT_EQ(*ptr, "test");
  }

  {
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    ptr.copy_from(s);
    internal::copy_or_view_ptr<std::string> ptr2 = ptr;
    EXPECT_EQ(*ptr, s);
    EXPECT_EQ(*ptr2, s);
    EXPECT_NE(&*ptr, &s);
    EXPECT_NE(&*ptr2, &s);
  }

  {
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    ptr.set_view(s);
    internal::copy_or_view_ptr<std::string> ptr2 = ptr;
    EXPECT_EQ(*ptr, s);
    EXPECT_EQ(*ptr2, s);
    EXPECT_EQ(&*ptr, &s);
    EXPECT_EQ(&*ptr2, &s);
  }

  {
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    ptr.set_view(s);
    internal::copy_or_view_ptr<std::string> ptr2 = std::move(ptr);
    EXPECT_EQ(*ptr2, s);
    EXPECT_EQ(&*ptr2, &s);
  }
}

TEST(ConversionRequestTest, SetKeyTest) {
  auto request = std::make_shared<commands::Request>();
  auto config = std::make_shared<config::Config>();
  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(*request, *config);

  composer::Composer composer(table, request, config);
  composer.SetPreeditTextForTestOnly("query");

  // SetKey("") is ignored. The actual key is created from composer.
  ConversionRequest conversion_request1 = ConversionRequestBuilder()
                                              .SetConfigView(*config)
                                              .SetRequestView(*request)
                                              .SetComposer(composer)
                                              .SetKey("")
                                              .Build();
  EXPECT_EQ("query", conversion_request1.key());

  ConversionRequest conversion_request2 = ConversionRequestBuilder()
                                              .SetConfigView(*config)
                                              .SetRequestView(*request)
                                              .SetComposer(composer)
                                              .SetKey("foo")
                                              .Build();
  EXPECT_EQ("foo", conversion_request2.key());
}

}  // namespace mozc
