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

#include "languages/pinyin/english_dictionary_data_builder.h"

#include <cmath>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/util.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/file/section.h"
#include "storage/louds/louds_trie_builder.h"

namespace mozc {
namespace pinyin {
namespace english {

using mozc::storage::louds::LoudsTrieBuilder;

namespace {
// Priority = (1 / (sqrt(index + offset))) + used_count * multiplier
const float kIndexOffset = 10.0;
const float kLearningMultiplier = 0.02;
}  // namespace

EnglishDictionaryDataBuilder::EnglishDictionaryDataBuilder()
    : builder_(NULL), louds_id_to_priority_(NULL), words_num_(0) {
}

EnglishDictionaryDataBuilder::~EnglishDictionaryDataBuilder() {
}

void EnglishDictionaryDataBuilder::BuildFromStream(istream *input_stream) {
  DCHECK(input_stream);

  vector<string> words;
  string line;
  while (getline(*input_stream, line)) {
    if (line.empty() || Util::StartsWith(line, "#")) {
      continue;
    }
    words.push_back(line);
  }

  builder_.reset(new LoudsTrieBuilder);
  for (size_t i = 0; i < words.size(); ++i) {
    builder_->Add(words[i]);
  }
  builder_->Build();

  words_num_ = words.size();
  louds_id_to_priority_.reset(new float[words_num_]);

  for (size_t i = 0; i < words.size(); ++i) {
      const int word_id = builder_->GetId(words[i]);

    DCHECK_LT(word_id, words.size());
    DCHECK_NE(-1, word_id);

    louds_id_to_priority_[word_id] = 1.0 / (sqrt(kIndexOffset + i));
  }
}

void EnglishDictionaryDataBuilder::WriteToStream(ostream *output_stream) const {
  DCHECK(output_stream);
  DCHECK(builder_.get());
  DCHECK(louds_id_to_priority_.get());

  vector<DictionaryFileSection> sections;
  DictionaryFileCodecInterface *file_codec =
      DictionaryFileCodecFactory::GetCodec();

  DictionaryFileSection dictionary_trie(
      builder_->image().data(),
      builder_->image().size(),
      file_codec->GetSectionName("english_dictionary_trie"));
  sections.push_back(dictionary_trie);

  DictionaryFileSection word_priority_table(
      reinterpret_cast<const char *>(louds_id_to_priority_.get()),
      words_num_ * static_cast<int>(sizeof(louds_id_to_priority_[0])),
      file_codec->GetSectionName("english_word_priority_table"));
  sections.push_back(word_priority_table);

  DictionaryFileSection learning_multiplier(
    reinterpret_cast<const char *>(&kLearningMultiplier),
    sizeof(kLearningMultiplier),
    file_codec->GetSectionName("learning_multiplier"));
  sections.push_back(learning_multiplier);

  file_codec->WriteSections(sections, output_stream);
}

}  // namespace english
}  // namespace pinyin
}  // namespace mozc
