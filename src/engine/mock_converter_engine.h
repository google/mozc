// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_ENGINE_MOCK_CONVERTER_ENGINE_H_
#define MOZC_ENGINE_MOCK_CONVERTER_ENGINE_H_

#include "base/base.h"
#include "base/logging.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "engine/engine_interface.h"

namespace mozc {

class PredictorInterface;
class SuppressionDictionary;

// Engine class with mock converter.
class MockConverterEngine : public EngineInterface {
 public:
  MockConverterEngine() : converter_mock_(new ConverterMock) {}
  virtual ~MockConverterEngine() {}

  virtual ConverterInterface *GetConverter() const {
    return converter_mock_.get();
  }

  virtual PredictorInterface *GetPredictor() const {
    LOG(FATAL) << "Mock predictor is not implemented.";
    return NULL;
  }

  virtual SuppressionDictionary *GetSuppressionDictionary() {
    LOG(FATAL) << "Suffix dictionary is not supported.";
    return NULL;
  }

  virtual bool Reload() {
    return true;
  }

  ConverterMock* mutable_converter_mock() {
    return converter_mock_.get();
  }

 private:
  scoped_ptr<ConverterMock> converter_mock_;

  DISALLOW_COPY_AND_ASSIGN(MockConverterEngine);
};

}  // namespace mozc

#endif  // MOZC_ENGINE_MOCK_CONVERTER_ENGINE_H_
