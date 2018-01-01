// Copyright 2010-2018, Google Inc.
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

#ifndef MOZC_TESTING_BASE_PUBLIC_NACL_MOCK_MODULE_H_
#define MOZC_TESTING_BASE_PUBLIC_NACL_MOCK_MODULE_H_

#ifdef OS_NACL

namespace mozc {
namespace testing {

// This method does nothing, and is just for avoiding link errors.
//
// This method should be called in somewhere. Otherwise, the object file
// generated from this file will be avoided by clang++ since there is no code
// which uses this module.
// Actually, pp::CreateModule() in this module is called if NaCl module is
// loaded on Chrome, so we need to link this module.
// TODO(hsumita): Remove this workaround.
void WorkAroundEmptyFunctionToAvoidLinkError();

}  // namespace testing
}  // namespace mozc

#endif  // OS_NACL

#endif  // MOZC_TESTING_BASE_PUBLIC_NACL_MOCK_MODULE_H_
