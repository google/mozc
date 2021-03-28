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

#include <memory>
#include <vector>

#include "base/util.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "win32/tip/tip_ref_count.h"

using ::mozc::client::ClientFactory;
using ::mozc::client::ClientInterface;
using ::mozc::commands::Input;
using ::mozc::commands::KeyEvent;
using ::mozc::commands::Output;
using ::mozc::commands::SessionCommand;
using ::std::unique_ptr;

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

class TipQueryProviderImpl : public TipQueryProvider {
 public:
  explicit TipQueryProviderImpl(ClientInterface *client) : client_(client) {}

 private:
  // The TipQueryProvider interface methods.
  virtual bool Query(const std::wstring &query, QueryType type,
                     std::vector<std::wstring> *result) {
    if (type == kReconversion) {
      return ReconvertQuery(query, result);
    }
    return SimpleQuery(query, result);
  }

  bool SimpleQuery(const std::wstring &query,
                   std::vector<std::wstring> *result) {
    {
      KeyEvent key_event;
      std::string utf8_query;
      Util::WideToUTF8(query, &utf8_query);
      key_event.set_key_string(utf8_query);
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
        const auto &utf8 = candidates.candidates(i).value();
        std::wstring wide;
        Util::UTF8ToWide(utf8, &wide);
        result->push_back(wide);
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

  bool ReconvertQuery(const std::wstring &query,
                      std::vector<std::wstring> *result) {
    {
      std::string utf8_query;
      Util::WideToUTF8(query, &utf8_query);
      SessionCommand command;
      command.set_type(SessionCommand::CONVERT_REVERSE);

      command.set_text(utf8_query);
      Output output;
      if (!client_->SendCommand(command, &output)) {
        return false;
      }
      const auto &candidates = output.all_candidate_words();
      for (size_t i = 0; i < candidates.candidates_size(); ++i) {
        const auto &utf8 = candidates.candidates(i).value();
        std::wstring wide;
        Util::UTF8ToWide(utf8, &wide);
        result->push_back(wide);
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
  unique_ptr<client::ClientInterface> client_;

  DISALLOW_COPY_AND_ASSIGN(TipQueryProviderImpl);
};

}  // namespace

TipQueryProvider::~TipQueryProvider() {}

// static
TipQueryProvider *TipQueryProvider::Create() {
  unique_ptr<ClientInterface> client(ClientFactory::NewClient());
  if (!client->EnsureSession()) {
    return nullptr;
  }
  client->set_suppress_error_dialog(true);
  return new TipQueryProviderImpl(client.release());
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
