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

// CalculatorMock is a mock implementation of CalculatorInterface. It converts
// key to value with key-value pairs which are set beforehand.

#ifndef MOZC_REWRITER_CALCULATOR_CALCULATOR_MOCK_H_
#define MOZC_REWRITER_CALCULATOR_CALCULATOR_MOCK_H_

#include <map>
#include <string>
#include <utility>

#include "rewriter/calculator/calculator_interface.h"

namespace mozc {

class CalculatorMock : public CalculatorInterface {
 public:
  CalculatorMock();
  virtual ~CalculatorMock();

  // Injects the behavior that CalculateString converts |key| to |value| and
  // returns |return_value|.
  void SetCalculatePair(const string &key,
                        const string &value,
                        bool return_value);

  // The number that CalculateString() has been called.
  int calculation_counter() const;

  // If |key| has been set by SetCalculatePair, then sets |*result| to the
  // corresponding value and returns |return_value|, otherwise clear |*result|
  // and returns false.
  virtual bool CalculateString(const string &key, string *result) const;

 private:
  typedef map<string, pair<string, bool> > CalculationMap;

  CalculationMap calculation_map_;
  mutable int calculation_counter_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_CALCULATOR_MOCK_H_
