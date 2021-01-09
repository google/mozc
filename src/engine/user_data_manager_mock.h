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

// An implementation of UserDataManagerInterface for testing.

#ifndef MOZC_ENGINE_USER_DATA_MANAGER_MOCK_H_
#define MOZC_ENGINE_USER_DATA_MANAGER_MOCK_H_

#include "engine/user_data_manager_interface.h"

#include <map>
#include <string>

#include "base/port.h"

namespace mozc {

class UserDataManagerMock : public UserDataManagerInterface {
 public:
  UserDataManagerMock();
  ~UserDataManagerMock() override;

  bool Sync() override;
  bool Reload() override;
  bool ClearUserHistory() override;
  bool ClearUserPrediction() override;
  bool ClearUnusedUserPrediction() override;
  bool ClearUserPredictionEntry(const std::string &key,
                                const std::string &value) override;
  bool Wait() override;

  int GetFunctionCallCount(const std::string &name) const;
  const std::string &GetLastClearedKey() const;
  const std::string &GetLastClearedValue() const;

 private:
  std::map<std::string, int> function_counters_;
  std::string last_cleared_key_;
  std::string last_cleared_value_;
  DISALLOW_COPY_AND_ASSIGN(UserDataManagerMock);
};

}  // namespace mozc

#endif  // MOZC_ENGINE_USER_DATA_MANAGER_MOCK_H_
