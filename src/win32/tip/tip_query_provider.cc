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

#include "win32/tip/tip_query_provider.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/win32/wide_char.h"
#include "client/client_interface.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "win32/tip/tip_ref_count.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ::mozc::client::ClientFactory;
using ::mozc::client::ClientInterface;
using ::mozc::commands::KeyEvent;
using ::mozc::commands::Output;
using ::mozc::commands::SessionCommand;

class TipQueryProviderImpl : public TipQueryProvider {
 public:
  explicit TipQueryProviderImpl(std::unique_ptr<ClientInterface> client)
      : client_(std::move(client)) {}
  TipQueryProviderImpl(const TipQueryProviderImpl &) = delete;
  TipQueryProviderImpl &operator=(const TipQueryProviderImpl &) = delete;

 private:
  // The TipQueryProvider interface methods.
  bool Query(const std::wstring_view query, QueryType type,
             std::vector<std::wstring> *result) override {
    if (type == kReconversion) {
      return ReconvertQuery(query, result);
    }
    return SimpleQuery(query, result);
  }

  bool SimpleQuery(const std::wstring_view query,
                   std::vector<std::wstring> *result) {
    {
      KeyEvent key_event;
      key_event.set_key_string(WideToUtf8(query));
      key_event.set_activated(true);
      Output output;
      // TODO(yukawa): Consider to introduce a new command that does 1) real
      // time conversion and 2) some suggestions, regardless of the current
      // user settings.
      if (!client_->SendKey(key_event, &output)) {
        return false;
      }
      if (output.error_code() != Output::SESSION_SUCCESS) {
        return false;
      }
      const auto &candidates = output.all_candidate_words();
      for (size_t i = 0; i < candidates.candidates_size(); ++i) {
        result->push_back(Utf8ToWide(candidates.candidates(i).value()));
      }
    }
    {
      SessionCommand command;
      command.set_type(SessionCommand::REVERT);
      Output output;
      client_->SendCommand(command, &output);
    }
    return true;
  }

  bool ReconvertQuery(const std::wstring_view query,
                      std::vector<std::wstring> *result) {
    {
      SessionCommand command;
      command.set_type(SessionCommand::CONVERT_REVERSE);
      command.set_text(WideToUtf8(query));
      Output output;
      if (!client_->SendCommand(command, &output)) {
        return false;
      }
      const auto &candidates = output.all_candidate_words();
      for (size_t i = 0; i < candidates.candidates_size(); ++i) {
        result->push_back(Utf8ToWide(candidates.candidates(i).value()));
      }
    }
    {
      SessionCommand command;
      command.set_type(SessionCommand::REVERT);
      Output output;
      client_->SendCommand(command, &output);
    }
    return true;
  }

  TipRefCount ref_count_;
  std::unique_ptr<client::ClientInterface> client_;
};

}  // namespace

// static
std::unique_ptr<TipQueryProvider> TipQueryProvider::Create() {
  std::unique_ptr<ClientInterface> client(ClientFactory::NewClient());
  if (!client->EnsureSession()) {
    return nullptr;
  }
  client->set_suppress_error_dialog(true);
  return std::make_unique<TipQueryProviderImpl>(std::move(client));
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
