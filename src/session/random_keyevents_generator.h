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

#ifndef MOZC_SESSION_RANDOM_KEYEVENTS_GENERATOR_H_
#define MOZC_SESSION_RANDOM_KEYEVENTS_GENERATOR_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace session {
class RandomKeyEventsGenerator {
 public:
  RandomKeyEventsGenerator() = default;
  // Initialize with seed.
  template <typename Rng>
  explicit RandomKeyEventsGenerator(Rng&& rng)
      : bitgen_(std::forward<Rng>(rng)) {}

  // Load all data to avoid a increase of memory usage
  // during memory leak tests.
  void PrepareForMemoryLeakTest();

  // Generate a random test keyevents sequence for desktop
  void GenerateSequence(std::vector<commands::KeyEvent>* keys);

  // Generate a random test keyevents sequence for mobile
  void GenerateMobileSequence(bool create_probable_key_events,
                              std::vector<commands::KeyEvent>* keys);

  // return test sentence set embedded in RandomKeyEventsGenerator.
  // Example:
  // const absl::Span<const char *> sentences =
  //     RandomKeyEventsGenerator::GetTestSentences();
  // const absl::string_view s = sentences[10];
  static absl::Span<const char*> GetTestSentences();

 private:
  uint32_t GetRandomAsciiKey();
  void TypeRawKeys(absl::string_view romaji, bool create_probable_key_events,
                   std::vector<commands::KeyEvent>* keys);
  void GenerateMobileSequenceInternal(absl::string_view sentence,
                                      bool create_probable_key_events,
                                      std::vector<commands::KeyEvent>* keys);

  absl::BitGen bitgen_;
};
}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_RANDOM_KEYEVENTS_GENERATOR_H_
