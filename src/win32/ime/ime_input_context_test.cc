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

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

#include "base/util.h"
#include "win32/ime/ime_input_context.h"

namespace mozc {
namespace win32 {
namespace {
InputContext *AsContext(INPUTCONTEXT *context_pointer) {
  return reinterpret_cast<InputContext *>(context_pointer);
}
}  // namespace

TEST(InputContextTest, InitializeTest) {
  // If the conversion mode is not initialized, initialize it with
  // IME_CMODE_NATIVE.
  {
    INPUTCONTEXT base_context = {0};

    InputContext *context(AsContext(&base_context));
    EXPECT_TRUE(context->Initialize());
    EXPECT_EQ(IME_CMODE_NATIVE, context->fdwConversion);
    EXPECT_EQ(INIT_CONVERSION, (context->fdwInit & INIT_CONVERSION));
  }

  // If any conversion mode is set, keep it as it is.
  {
    INPUTCONTEXT base_context = {0};

    InputContext *context(AsContext(&base_context));
    context->fdwConversion = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    context->fdwInit |= INIT_CONVERSION;

    EXPECT_TRUE(context->Initialize());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE, context->fdwConversion);
    EXPECT_EQ(INIT_CONVERSION, (context->fdwInit & INIT_CONVERSION));
  }

  // If the sentence mode is not initialized, initialize it with
  // IME_SMODE_PHRASEPREDICT.
  {
    INPUTCONTEXT base_context = {0};

    InputContext *context(AsContext(&base_context));
    EXPECT_TRUE(context->Initialize());
    EXPECT_EQ(IME_SMODE_PHRASEPREDICT, context->fdwSentence);
    EXPECT_EQ(INIT_SENTENCE, (context->fdwInit & INIT_SENTENCE));
  }

  // If any sentence mode is set, it should be updated to
  // IME_SMODE_PHRASEPREDICT.
  {
    INPUTCONTEXT base_context = {0};

    InputContext *context(AsContext(&base_context));
    context->fdwConversion = IME_SMODE_CONVERSATION;
    context->fdwInit |= INIT_SENTENCE;

    EXPECT_TRUE(context->Initialize());
    EXPECT_EQ(IME_SMODE_PHRASEPREDICT, context->fdwSentence);
    EXPECT_EQ(INIT_SENTENCE, (context->fdwInit & INIT_SENTENCE));
  }
}
}  // namespace win32
}  // namespace mozc
