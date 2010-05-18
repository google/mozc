// Copyright 2010, Google Inc.
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

#include "prediction/suggestion_filter.h"

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "storage/existence_filter.h"

namespace mozc {
namespace {

#include "prediction/suggestion_filter_data.h"

class SuggestionFilterImpl {
 public:
  bool IsBadSuggestion(const string &text) const {
    if (filter_.get() == NULL) {
      return false;
    }
    return filter_->Exists(Util::Fingerprint(text.data(), text.size()));
  }

  SuggestionFilterImpl() : filter_(NULL) {
    // kSuggestionFilterData_data and kSuggestionFilterData_size
    // are defined in suggest_filter_data.h
    filter_.reset(ExistenceFilter::Read(kSuggestionFilterData_data,
                                        kSuggestionFilterData_size));
    LOG_IF(ERROR, filter_.get() == NULL)
        << "SuggestionFilterData is broken";
  }

 private:
  scoped_ptr<ExistenceFilter> filter_;
};
}  // namespace

bool SuggestionFilter::IsBadSuggestion(const string &text) {
  return Singleton<SuggestionFilterImpl>::get()->IsBadSuggestion(text);
}
}  // namespace mozc
