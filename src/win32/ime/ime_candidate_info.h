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

#ifndef MOZC_WIN32_IME_IME_CANDIDATE_INFO_H_
#define MOZC_WIN32_IME_IME_CANDIDATE_INFO_H_

#include <windows.h>

#include <vector>

#include "base/port.h"

#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST()
#include "win32/ime/ime_types.h"

namespace mozc {

namespace commands {
class Output;
}  // namespace commands

namespace win32 {
struct CandidateInfo {
  CandidateInfo();
  void Clear();

  // Data size to be stored in CANDIDATEINFO::dwSize in bytes.
  DWORD candidate_info_size;
  // Data size to be stored in CANDIDATELIST::dwSize in bytes.
  DWORD candidate_list_size;

  DWORD count;
  DWORD selection;
  bool show_candidate;
  std::vector<DWORD> offsets;
  std::vector<wchar_t> text_buffer;
};

class CandidateInfoUtil {
 public:
  // Returns an Input Method Context Component (IMCC) handle with initializing
  // it with an empty CANDIDATEINFO data.  Returns nullptr if fails.
  // You can specify the previously used handle in |current_handle| to transfer
  // the ownership so that this method can reuse the handle and its memory
  // block.  If nullptr is specified in |current_handle|, this method allocates
  // a new memory block.  The caller is responsible for the lifetime management
  // of the returned handle either way.
  static HIMCC Initialize(HIMCC current_handle);

  // Returns an Input Method Context Component (IMCC) handle with filling
  // candidate list information based on the Mozc output specified in |output|.
  // Returns nullptr if fails.
  // You can specify the previously used handle in |current_handle| to transfer
  // the ownership so that this method can reuse the handle and its memory
  // block.  If nullptr is specified in |current_handle|, this method allocates
  // a new memory block.  The caller is responsible for the lifetime management
  // of the returned handle either way.
  static HIMCC Update(HIMCC current_handle,
                      const mozc::commands::Output &output,
                      std::vector<UIMessage> *messages);

 private:
  static bool Convert(const mozc::commands::Output &output,
                      CandidateInfo *info);

  static void Write(const CandidateInfo &info, CANDIDATEINFO *target);

  static void SetSafeDefault(CandidateInfo *info);

  static HIMCC UpdateCandidateInfo(HIMCC current_handle,
                                   const CandidateInfo &info);

  FRIEND_TEST(CandidateInfoUtilTest, ConversionTest);
  FRIEND_TEST(CandidateInfoUtilTest, SuggestionTest);
  FRIEND_TEST(CandidateInfoUtilTest, WriteResultTest);
  FRIEND_TEST(CandidateInfoUtilTest, WriteSafeDefaultTest);
  FRIEND_TEST(CandidateInfoUtilTest, PagingEmulation_Issue4077022);

 private:
  DISALLOW_COPY_AND_ASSIGN(CandidateInfoUtil);
};

}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_IME_IME_CANDIDATE_INFO_H_
