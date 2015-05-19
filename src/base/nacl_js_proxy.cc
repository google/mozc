// Copyright 2010-2013, Google Inc.
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

#include "base/nacl_js_proxy.h"

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>
#include <ppapi/utility/completion_callback_factory.h>

#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/scoped_ptr.h"
#include "base/unnamed_event.h"
#include "net/jsoncpp.h"

namespace mozc {

class NaclJsProxyImpl : public NaclJsProxyImplInterface {
 public:
  explicit NaclJsProxyImpl(pp::Instance *instance);
  virtual ~NaclJsProxyImpl();
  virtual bool GetAuthToken(bool interactive, string *access_token);
  virtual void OnProxyCallResult(Json::Value *result);

 private:
  void PostMessage(int32_t result, const string &message);
  pp::Instance *instance_;
  pp::CompletionCallbackFactory<NaclJsProxyImpl> factory_;
  scoped_ptr<Json::Value> result_;
  mozc::Mutex mutex_;
  UnnamedEvent event_;
};

NaclJsProxyImpl::NaclJsProxyImpl(pp::Instance *instance)
  : instance_(instance),
    factory_(this) {
}

NaclJsProxyImpl::~NaclJsProxyImpl() {
}

bool NaclJsProxyImpl::GetAuthToken(bool interactive, string *access_token) {
  scoped_lock l(&mutex_);
  Json::Value message(Json::objectValue);
  message["jscall"] = "GetAuthToken";
  message["args"] = Json::objectValue;
  message["args"]["interactive"] = interactive;
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      factory_.NewCallback(
          &NaclJsProxyImpl::PostMessage,
          Json::FastWriter().write(message)));
  event_.Wait(-1);
  scoped_ptr<Json::Value> result(result_.release());
  DCHECK(result.get());
  if (result->isMember("access_token")) {
    *access_token = (*result)["access_token"].asString();
    DLOG(INFO) << "GetAuthToken succeeded";
    return true;
  }
  DLOG(INFO) << "GetAuthToken failed";
  return false;
}

void NaclJsProxyImpl::OnProxyCallResult(Json::Value *result) {
  result_.reset(result);
  event_.Notify();
}

void NaclJsProxyImpl::PostMessage(int32_t result, const string &message) {
  instance_->PostMessage(message);
}

// static
scoped_ptr<NaclJsProxyImplInterface> NaclJsProxy::impl_;

// static
void NaclJsProxy::Initialize(pp::Instance *instance) {
  impl_.reset(new NaclJsProxyImpl(instance));
}

// static
bool NaclJsProxy::GetAuthToken(bool interactive, string *access_token) {
  CHECK(impl_.get());
  return impl_->GetAuthToken(interactive, access_token);
}

// static
void NaclJsProxy::OnProxyCallResult(Json::Value *result) {
  CHECK(impl_.get());
  impl_->OnProxyCallResult(result);
}

// static
void NaclJsProxy::RegisterNaclJsProxyImplForTest(
    NaclJsProxyImplInterface *impl) {
  impl_.reset(impl);
}

}  // namespace mozc
