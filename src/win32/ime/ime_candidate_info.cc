// Copyright 2010-2014, Google Inc.
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

#include "win32/ime/ime_candidate_info.h"

#include <safeint.h>
#include <windows.h>  // windows.h must be included before strsafe.h
#include <strsafe.h>

#include <algorithm>

#include "google/protobuf/stubs/common.h"
#include "base/logging.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "win32/ime/ime_ui_window.h"

namespace mozc {
namespace win32 {
namespace {

using ::mozc::commands::CandidateList;
using ::mozc::commands::Candidates;

using ::msl::utilities::SafeAdd;
using ::msl::utilities::SafeCast;
using ::msl::utilities::SafeMultiply;
using ::msl::utilities::SafeSubtract;

// Since IMM32 uses DWORD rather than size_t for data size in data structures,
// relevant data size are stored into DWORD constants here.
static_assert(sizeof(DWORD) <= kint32max, "Check DWORD size.");

const DWORD kSizeOfDWORD = static_cast<DWORD>(sizeof(DWORD));

static_assert(sizeof(wchar_t) <= kint32max, "Check wchar_t size.");
const DWORD kSizeOfWCHAR = static_cast<DWORD>(sizeof(wchar_t));

static_assert(sizeof(CANDIDATEINFO) <= kint32max, "Check CANDIDATEINFO size.");
const DWORD kSizeOfCANDIDATEINFO = static_cast<DWORD>(sizeof(CANDIDATEINFO));

static_assert(sizeof(CANDIDATELIST) <= kint32max, "Check CANDIDATELIST size.");
const DWORD kSizeOfCANDIDATELIST = static_cast<DWORD>(sizeof(CANDIDATELIST));

static_assert(sizeof(CANDIDATELIST) > sizeof(DWORD),
              "Check CANDIDATELIST size.");
const DWORD kSizeOfCANDIDATELISTHeader =
    static_cast<DWORD>(sizeof(CANDIDATELIST) - sizeof(DWORD));

static_assert((static_cast<int64>(sizeof(CANDIDATEINFO)) +
               static_cast<int64>(sizeof(CANDIDATELIST))) < kint32max,
               "Check CANDIDATEINFO + CANDIDATELIST size.");
const DWORD kSizeOfCANDIDATEINFOAndCANDIDATELIST =
    static_cast<DWORD>(sizeof(CANDIDATEINFO) + sizeof(CANDIDATELIST));

// Some games such as EMIL CHRONICLE ONLINE assumes that
// CANDIDATELIST::dwPageSize never be zero nor greater than 10 despite that
// the WDK document for IMM32 declares this field can be 0.  See b/3033499.
// We conform to those applications by always setting a safe number.
// Note that Office-IME 2010 always returns 9 CANDIDATELIST::dwPageSize
// regardless of the actual number of candidates.  So we use the same strategy.
const int kSafePageSize = 9;

bool GetCandidateCountInternal(
    const CANDIDATEINFO *info, DWORD buffer_size, DWORD *count) {
  DCHECK_NE(nullptr, info);
  DCHECK_NE(nullptr, count);

  if (info->dwCount < 1) {
    return false;
  }

  // |count_data_offset = info->dwOffset[0] + offsetof(CANDIDATELIST, dwCount)|
  DWORD count_data_offset = info->dwOffset[0];
  if (!SafeAdd(count_data_offset, offsetof(CANDIDATELIST, dwCount),
               count_data_offset)) {
    return false;
  }

  // |count_next_data_offset = count_data_offset + sizeof(DWORD)|
  DWORD count_next_data_offset = count_data_offset;
  if (!SafeAdd(count_next_data_offset, kSizeOfDWORD, count_next_data_offset)) {
    return false;
  }

  // Check if the target memory block has sufficient size to contain
  // CANDIDATELIST::dwCount.
  if (count_next_data_offset > buffer_size) {
    return false;
  }

  // Dereference the data.
  *count = *reinterpret_cast<const DWORD *>(
    reinterpret_cast<const BYTE *>(info) + count_data_offset);

  return true;
}

bool GetCandidateCount(HIMCC candidate_info_handle, DWORD *count) {
  if (candidate_info_handle == nullptr) {
    return false;
  }
  if (count == nullptr) {
    return false;
  }

  // If the target memory block does not have sufficient size, stop
  // reading the content.
  const DWORD buffer_size = ::ImmGetIMCCSize(candidate_info_handle);
  if (buffer_size < kSizeOfCANDIDATEINFO) {
    return false;
  }

  const CANDIDATEINFO *info =
      static_cast<CANDIDATEINFO *>(::ImmLockIMCC(candidate_info_handle));
  if (info == nullptr) {
    return false;
  }

  const bool result = GetCandidateCountInternal(info, buffer_size, count);
  ::ImmUnlockIMCC(candidate_info_handle);
  return result;
}
}  // anonymous namespace

CandidateInfo::CandidateInfo()
    : candidate_info_size(0),
      candidate_list_size(0),
      count(0),
      selection(0),
      show_candidate(false) {}

void CandidateInfo::Clear() {
  candidate_info_size = 0;
  candidate_list_size = 0;
  count = 0;
  selection = 0;
  show_candidate = false;
  offsets.clear();
  text_buffer.clear();
}

void CandidateInfoUtil::SetSafeDefault(CandidateInfo *info) {
  DCHECK_NE(nullptr, info);
  info->Clear();
  info->candidate_info_size = kSizeOfCANDIDATEINFOAndCANDIDATELIST;
  info->candidate_list_size = kSizeOfCANDIDATELIST;
  info->offsets.push_back(0);
}

bool CandidateInfoUtil::Convert(const mozc::commands::Output &output,
                                CandidateInfo *info) {
  if (info == nullptr) {
    return false;
  }
  info->Clear();

  // Note that |output.all_candidate_words()| delivers the result of the
  // latest activities in the server while |output.candidates()| reflects an
  // expected content of the candidate-window-like UI.  Here is an example to
  // explain the difference between |output.all_candidate_words()| and
  // |output.candidates()|
  //   1. Type "あ"
  //       -> |all_candidate_words()| == empty
  //       -> |candidates()| == empty
  //   2. Hit space to convert.
  //       -> |all_candidate_words()| == [CONVERSION: "あ", "吾", ...]
  //       -> |candidates()| == empty
  //          (candidate window is still invisible)
  //   3. Hit space again.
  //       -> |all_candidate_words()| == [CONVERSION: "あ", "吾", ...]
  //       -> |candidates()|  == [CONVERSION: "あ", "吾", ...]
  //          (candidate window shows up)
  // As filed in b/2978825, candidate list should be updated when and only
  // when the candidate window is visible.  This is why |output.candidates()|
  // is used here.
  if (!output.has_candidates()) {
    return true;
  }

  const Candidates &candidates = output.candidates();
  if (candidates.has_category() &&
      (candidates.category() == mozc::commands::SUGGESTION)) {
    // If this is a suggest UI popup, we will not update the candidate info.
    return true;
  }

  // Even though the update timing should be determined by the availability of
  // |output.candidates()|, |output.all_candidate_words()| is preferable to
  // fill the candidate list itself because it contains all of the candidates
  // as opposed to |output.candidates()| which contains candidates which is in
  // the current page.
  if (!output.has_all_candidate_words()) {
    return false;
  }

  const CandidateList &candidate_list = output.all_candidate_words();

  if (!candidate_list.has_focused_index()) {
    return false;
  }

  if (!SafeCast(candidate_list.candidates_size(), info->count)) {
    return false;
  }

  DCHECK(candidate_list.has_focused_index());
  info->selection = candidate_list.focused_index();

  // |offset_buffer_size = sizeof(DWORD) * info.count|
  DWORD offset_buffer_size = 0;
  if (!SafeMultiply(kSizeOfDWORD, info->count, offset_buffer_size)) {
    return false;
  }

  // |text_buffer_initial_offset =
  //      sizeof(CANDIDATELIST) - sizeof(DWORD) + offset_buffer_size|
  DWORD text_buffer_initial_offset = kSizeOfCANDIDATELISTHeader;
  if (!SafeAdd(text_buffer_initial_offset, offset_buffer_size,
               text_buffer_initial_offset)) {
    return false;
  }

  DWORD text_buffer_size = 0;
  for (size_t i = 0; i < candidate_list.candidates_size(); ++i) {
    // Calculates the offset from the top of CANDIDATELIST.
    DWORD offset = text_buffer_initial_offset;
    if (!SafeAdd(offset, text_buffer_size, offset)) {
      return false;
    }
    info->offsets.push_back(offset);

    wstring value;
    if (mozc::Util::UTF8ToWide(candidate_list.candidates(i).value(), &value) ==
        0) {
      value.clear();
    }

    // Add '\0'
    value += L'\0';

    DWORD text_len = 0;
    // |text_len = sizeof(wchar_t) * value.size()|
    if (!SafeMultiply(kSizeOfWCHAR, value.size(), text_len)) {
      return false;
    }
    // |text_buffer_size += text_len|
    if (!SafeAdd(text_buffer_size, text_len, text_buffer_size)) {
      return false;
    }
    for (size_t char_index = 0; char_index < value.size(); ++char_index) {
      info->text_buffer.push_back(value[char_index]);
    }
  }

  // |candidate_list_size = (sizeof(CANDIDATELIST) - sizeof(DWORD))|
  DWORD candidate_list_size = kSizeOfCANDIDATELISTHeader;
  // |candidate_list_size += offset_buffer_size|
  if (!SafeAdd(candidate_list_size, offset_buffer_size, candidate_list_size)) {
    return false;
  }
  // |candidate_list_size += text_buffer_size|
  if (!SafeAdd(candidate_list_size, text_buffer_size, candidate_list_size)) {
    return false;
  }

  // |candidate_info_size = sizeof(CANDIDATEINFO)|
  DWORD candidate_info_size = kSizeOfCANDIDATEINFO;
  // |candidate_info_size += candidate_list_size|
  if (!SafeAdd(candidate_info_size, candidate_list_size, candidate_info_size)) {
    return false;
  }

  info->candidate_info_size = candidate_info_size;
  info->candidate_list_size = candidate_list_size;
  info->show_candidate = true;
  return true;
}

void CandidateInfoUtil::Write(const CandidateInfo &info,
                              CANDIDATEINFO *target) {
  if (target == nullptr) {
    return;
  }

  target->dwSize = info.candidate_info_size;
  if (info.candidate_list_size == 0) {
    target->dwCount = 0;
    target->dwPrivateOffset = 0;
    target->dwPrivateSize = 0;
    for (size_t i = 0; i < arraysize(target->dwOffset); ++i) {
      target->dwOffset[i] = 0;
    }
    return;
  }

  target->dwCount = 1;  // Only 1 Candidate Window
  target->dwPrivateOffset = 0;
  target->dwPrivateSize = 0;
  target->dwOffset[0] = kSizeOfCANDIDATEINFO;

  for (size_t i = 1; i < arraysize(target->dwOffset); ++i) {
    // CANDIDATELIST is to be placed just after CANDIDATEINFO.
    target->dwOffset[i] = 0;
  }

  CANDIDATELIST *candidate_list =
      reinterpret_cast<CANDIDATELIST *>(reinterpret_cast<BYTE *>(target) +
                                        kSizeOfCANDIDATEINFO);

  candidate_list->dwSize = info.candidate_list_size;
  candidate_list->dwStyle = IME_CAND_READ;
  candidate_list->dwCount = info.count;
  candidate_list->dwSelection = info.selection;
  // Emulate dwPageStart to work around b/4077022 because Mozc server
  // does not support paging. See b/1855733
  // Note that IMM32 Office IME 2010 sets 0 to |dwPageStart| unless
  // it receives NI_SETCANDIDATE_PAGESTART.
  // TODO(yukawa): Investigate further.
  candidate_list->dwPageStart =
      (info.selection / kSafePageSize) * kSafePageSize;
  candidate_list->dwPageSize = kSafePageSize;

  if (info.count > 0) {
    DWORD *offsets = &candidate_list->dwOffset[0];
    wchar_t *text_buffer = reinterpret_cast<wchar_t *>(
        reinterpret_cast<BYTE *>(candidate_list) + info.offsets[0]);
    copy(info.offsets.begin(), info.offsets.end(), offsets);
    copy(info.text_buffer.begin(), info.text_buffer.end(), text_buffer);
  }
}

HIMCC CandidateInfoUtil::Initialize(HIMCC current_handle) {
  CandidateInfo info;
  SetSafeDefault(&info);

  return UpdateCandidateInfo(current_handle, info);
}

HIMCC CandidateInfoUtil::Update(HIMCC current_handle,
                                const mozc::commands::Output &output,
                                vector<UIMessage> *messages) {
  CandidateInfo info;
  Convert(output, &info);

  // If the data is not initialized or too small for some reason,
  // replace it with harmful data just in case.
  if (info.candidate_info_size < kSizeOfCANDIDATEINFOAndCANDIDATELIST) {
    SetSafeDefault(&info);
  }

  bool was_empty = true;
  if (current_handle != nullptr) {
    DWORD previous_count = 0;
    if (GetCandidateCount(current_handle, &previous_count) &&
        previous_count != 0) {
      was_empty = false;
    }
  }

  HIMCC new_handle = UpdateCandidateInfo(current_handle, info);

  if (messages != nullptr) {
    if (was_empty && info.show_candidate) {
      messages->push_back(UIMessage(WM_IME_NOTIFY, IMN_OPENCANDIDATE, 1));
    }

    if (!was_empty && info.show_candidate) {
      messages->push_back(UIMessage(WM_IME_NOTIFY, IMN_CHANGECANDIDATE, 1));
    }

    if (!was_empty && !info.show_candidate) {
      messages->push_back(UIMessage(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1));
    }
  }

  return new_handle;
}

HIMCC CandidateInfoUtil::UpdateCandidateInfo(
    HIMCC current_handle, const CandidateInfo &list) {
  DCHECK_GE(list.candidate_info_size, kSizeOfCANDIDATEINFOAndCANDIDATELIST);

  HIMCC new_handle = nullptr;
  if (current_handle == nullptr) {
    new_handle = ::ImmCreateIMCC(list.candidate_info_size);
  } else {
    new_handle = ::ImmReSizeIMCC(current_handle, list.candidate_info_size);
  }

  if (new_handle != nullptr) {
    CANDIDATEINFO *buffer =
        static_cast<CANDIDATEINFO *>(::ImmLockIMCC(new_handle));
    if (buffer != nullptr) {
      Write(list, buffer);
      ::ImmUnlockIMCC(new_handle);
    }
  }
  return new_handle;
}
}  // namespace win32
}  // namespace mozc
