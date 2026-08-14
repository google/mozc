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

#ifndef MOZC_TESTING_GMOCK_H_
#define MOZC_TESTING_GMOCK_H_

#include <gmock/gmock.h>  // IWYU pragma: export

#ifndef EXPECT_OK

namespace mozc {

MATCHER(IsOkStatus, negation ? "is not OK" : "is OK") { return arg.ok(); }

}  // namespace mozc

#define ASSERT_OK(expr) ASSERT_THAT(expr, ::mozc::IsOkStatus())
#define EXPECT_OK(expr) EXPECT_THAT(expr, ::mozc::IsOkStatus())

#define MOZC_GMOCK_CONCAT_IMPL_(x, y) x##y
#define MOZC_GMOCK_CONCAT_(x, y) MOZC_GMOCK_CONCAT_IMPL_(x, y)

#define MOZC_ASSERT_OK_AND_ASSIGN_IMPL_(status_or, var, expr) \
  auto status_or = (expr);                                    \
  ASSERT_OK(status_or);                                       \
  var = *std::move(status_or)

#define ASSERT_OK_AND_ASSIGN(var, ...) \
  MOZC_ASSERT_OK_AND_ASSIGN_IMPL_(     \
      MOZC_GMOCK_CONCAT_(_status_or_value, __LINE__), var, (__VA_ARGS__))

#endif  // EXPECT_OK


#endif  // MOZC_TESTING_GMOCK_H_
