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

#ifndef MOZC_ENGINE_ENGINE_BUILDER_INTERFACE_H_
#define MOZC_ENGINE_ENGINE_BUILDER_INTERFACE_H_

#include <memory>

#include "engine/engine_interface.h"
#include "protocol/engine_builder.pb.h"
#include "absl/strings/string_view.h"

namespace mozc {

// Defines interface to build Engine instance in asynchronous way.
class EngineBuilderInterface {
 public:
  EngineBuilderInterface(const EngineBuilderInterface &) = delete;
  EngineBuilderInterface &operator=(const EngineBuilderInterface &) = delete;

  virtual ~EngineBuilderInterface() = default;

  // Accepts data load request and sets |response->status()| to one of the
  // following values:
  //   * ACCEPTED: Request is successfully accepted.
  //   * ALREADY_RUNNING: The previous request is still being processed.
  virtual void PrepareAsync(const EngineReloadRequest &request,
                            EngineReloadResponse *response) = 0;

  // Returns true if a response to PrepareAsync() is ready.
  virtual bool HasResponse() const = 0;

  // Gets the response to PrepareAsync() if available.
  virtual void GetResponse(EngineReloadResponse *response) const = 0;

  // Builds an engine using the data requested by PrepareAsync().
  // May return nullptr if bad data was requested in PrepareAsync().
  virtual std::unique_ptr<EngineInterface> BuildFromPreparedData() = 0;

  // Clears internal states to accept next request.
  virtual void Clear() = 0;

 protected:
  EngineBuilderInterface() = default;
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_BUILDER_INTERFACE_H_
