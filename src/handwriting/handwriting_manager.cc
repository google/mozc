// Copyright 2010-2011, Google Inc.
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

// Handwriting module manager class

#include <set>

#include "base/singleton.h"
#include "handwriting/handwriting_manager.h"

namespace mozc {
namespace handwriting {

class HandwritingManagerImpl {
 public:
  HandwritingManagerImpl() {}
  virtual ~HandwritingManagerImpl() {}

  void AddHandwritingModule(HandwritingInterface *module) {
    modules_.push_back(module);
  }

  void ClearHandwritingModules() {
    modules_.clear();
  }

  void Recognize(const Strokes &strokes, vector<string> *candidates) const {
    candidates->clear();
    set<string> contained;
    for (size_t i = 0; i < modules_.size(); ++i) {
      vector<string> module_candidates;
      modules_[i]->Recognize(strokes, &module_candidates);
      for (size_t j = 0; j < module_candidates.size(); ++j) {
        const string &word = module_candidates[j];
        if (contained.find(word) == contained.end()) {
          contained.insert(word);
          candidates->push_back(word);
        }
      }
    }
  }

  void Commit(const Strokes &strokes, const string &result) {
    for (size_t i = 0; i < modules_.size(); ++i) {
      modules_[i]->Commit(strokes, result);
    }
  }

 private:
  vector<HandwritingInterface *> modules_;
};

// static
void HandwritingManager::AddHandwritingModule(HandwritingInterface *module) {
  Singleton<HandwritingManagerImpl>::get()->AddHandwritingModule(module);
}

// static
void HandwritingManager::ClearHandwritingModules() {
  Singleton<HandwritingManagerImpl>::get()->ClearHandwritingModules();
}

// static
void HandwritingManager::Recognize(const Strokes &strokes,
                                   vector<string> *candidates) {
  Singleton<HandwritingManagerImpl>::get()->Recognize(strokes, candidates);
}

// static
void HandwritingManager::Commit(const Strokes &strokes,
                                const string &result) {
  Singleton<HandwritingManagerImpl>::get()->Commit(strokes, result);
}

}  // namespace handwriting
}  // namespace mozc
