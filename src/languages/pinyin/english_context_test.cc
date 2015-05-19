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

#include "languages/pinyin/english_context.h"

#include <string>
#include <vector>

#include "base/port.h"
#include "base/util.h"
#include "languages/pinyin/english_dictionary_factory.h"
#include "languages/pinyin/session_config.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::_;
using ::testing::Return;

namespace mozc {
namespace pinyin {
namespace english {

namespace {
class EnglishMockDictionary : public EnglishDictionaryInterface {
 public:
  EnglishMockDictionary() {
    const char *kWordList[] = {
      "aaa", "aab", "aac",
      "aa", "ab", "ac",
      "a", "b", "c",
    };
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kWordList); ++i) {
      word_list_.push_back(kWordList[i]);
    }
  }
  virtual ~EnglishMockDictionary() {}

  void GetSuggestions(const string &prefix, vector<string> *output) const {
    DCHECK(output);
    output->clear();

    if (prefix.empty()) {
      return;
    }

    string normalized_prefix = prefix;
    Util::LowerString(&normalized_prefix);

    for (size_t i = 0; i < word_list_.size(); ++i) {
      if (Util::StartsWith(word_list_[i], normalized_prefix)) {
        output->push_back(word_list_[i]);
      }
    }
  }

  MOCK_METHOD1(LearnWord, bool(const string &word));

 private:
  vector<string> word_list_;
};
}  // namespace

class EnglishContextTest : public testing::Test {
 protected:
  virtual void SetUp() {
    EXPECT_CALL(dictionary_, LearnWord(_)).WillRepeatedly(Return(true));
    EnglishDictionaryFactory::SetDictionary(&dictionary_);

    session_config_.reset(new SessionConfig);
    session_config_->full_width_word_mode = false;
    session_config_->full_width_punctuation_mode = true;
    session_config_->simplified_chinese_mode = true;
    context_.reset(new EnglishContext(*session_config_));
  }

  virtual void TearDown() {
    context_.reset(NULL);
    EnglishDictionaryFactory::SetDictionary(NULL);
  }

  void InsertCharacterChars(const string &chars) {
    for (size_t i = 0; i < chars.size(); ++i) {
      context_->Insert(chars[i]);
    }
  }

  void CheckContext(const string &input_text,
                    const string &commit_text,
                    size_t focused_candidate_index) {
    EXPECT_EQ(input_text, context_->input_text());
    EXPECT_EQ(commit_text, context_->commit_text());
    if (input_text.size() <= 1) {
      EXPECT_EQ(input_text, context_->auxiliary_text());
    } else {
      EXPECT_EQ("v " + input_text.substr(1), context_->auxiliary_text());
    }
    EXPECT_EQ("", context_->selected_text());
    EXPECT_EQ("", context_->conversion_text());
    EXPECT_EQ("", context_->rest_text());

    EXPECT_EQ(0, context_->cursor());
    EXPECT_EQ(focused_candidate_index, context_->focused_candidate_index());

    if (input_text.size() <= 1) {
      EXPECT_EQ(0, GetCandidatesSize());
    } else {
      vector<string> expected_candidates;
      string query = input_text.substr(1);
      Util::LowerString(&query);
      dictionary_.GetSuggestions(query, &expected_candidates);

      ASSERT_EQ(expected_candidates.size(), GetCandidatesSize());

      Candidate candidate;
      for (size_t i = 0; context_->GetCandidate(i, &candidate); ++i) {
        EXPECT_EQ(expected_candidates[i], candidate.text);
      }
    }
  }

  size_t GetCandidatesSize() {
    size_t size = 0;
    for (; context_->HasCandidate(size); ++size) {}
    return size;
  }

  EnglishMockDictionary dictionary_;
  scoped_ptr<SessionConfig> session_config_;
  scoped_ptr<EnglishContext> context_;
};

TEST_F(EnglishContextTest, InsertTest) {
  {
    SCOPED_TRACE("Inserts v");
    EXPECT_TRUE(context_->Insert('v'));
    CheckContext("v", "", 0);
  }

  {
    SCOPED_TRACE("Inserts va");
    EXPECT_TRUE(context_->Insert('a'));
    CheckContext("va", "", 0);
  }

  {
    SCOPED_TRACE("Inserts vaA");
    EXPECT_TRUE(context_->Insert('A'));
    CheckContext("vaA", "", 0);
  }

  {
    SCOPED_TRACE("Inserts 1 and fails");
    EXPECT_FALSE(context_->Insert('1'));
    CheckContext("vaA", "", 0);
  }
}

TEST_F(EnglishContextTest, CommitTest) {
  {
    SCOPED_TRACE("Commits [v]");
    InsertCharacterChars("v");
    EXPECT_CALL(dictionary_, LearnWord(_)).Times(0);
    context_->Commit();
    CheckContext("", "", 0);
  }

  {
    SCOPED_TRACE("Commits [va]");
    InsertCharacterChars("va");
    EXPECT_CALL(dictionary_, LearnWord("a")).Times(1);
    context_->Commit();
    CheckContext("", "a", 0);
  }

  context_->Clear();

  {
    SCOPED_TRACE("Commits preedit [va]");
    InsertCharacterChars("va");
    EXPECT_CALL(dictionary_, LearnWord("a")).Times(1);
    context_->CommitPreedit();
    CheckContext("", "a", 0);
  }

  context_->Clear();

  {  // Selects second candidate and commits.
    {
      SCOPED_TRACE("Focuses a 2nd candidate");
      InsertCharacterChars("va");
      context_->FocusCandidate(1);
      CheckContext("va", "", 1);
    }
    {
      SCOPED_TRACE("Commits when second candidate is focused");
      EXPECT_CALL(dictionary_, LearnWord("a")).Times(1);
      context_->Commit();
      CheckContext("", "a", 0);
    }
  }
}

TEST_F(EnglishContextTest, CursorTest) {
  InsertCharacterChars("va");

  {
    SCOPED_TRACE("Moves cursor to left");
    EXPECT_FALSE(context_->MoveCursorLeft());
    CheckContext("va", "", 0);
  }

  {
    SCOPED_TRACE("Moves cursor to right");
    EXPECT_FALSE(context_->MoveCursorRight());
    CheckContext("va", "", 0);
  }

  {
    SCOPED_TRACE("Moves cursor to left by word");
    EXPECT_FALSE(context_->MoveCursorLeftByWord());
    CheckContext("va", "", 0);
  }

  {
    SCOPED_TRACE("Moves cursor to right by word");
    EXPECT_FALSE(context_->MoveCursorRightByWord());
    CheckContext("va", "", 0);
  }

  {
    SCOPED_TRACE("Moves cursor to to beginning");
    EXPECT_FALSE(context_->MoveCursorToBeginning());
    CheckContext("va", "", 0);
  }

  {
    SCOPED_TRACE("Moves cursor to end");
    EXPECT_FALSE(context_->MoveCursorToEnd());
    CheckContext("va", "", 0);
  }
}

TEST_F(EnglishContextTest, RemoveTest) {
  InsertCharacterChars("vaa");

  {
    SCOPED_TRACE("Removes a previous character");
    EXPECT_TRUE(context_->RemoveCharBefore());
    CheckContext("va", "", 0);
  }

  {
    SCOPED_TRACE("Removes a previous character thrice");
    EXPECT_TRUE(context_->RemoveCharBefore());
    CheckContext("v", "", 0);

    EXPECT_TRUE(context_->RemoveCharBefore());
    CheckContext("", "", 0);

    EXPECT_TRUE(context_->RemoveCharBefore());
    CheckContext("", "", 0);
  }

  InsertCharacterChars("vaa");

  {
    SCOPED_TRACE("Removes a previous word");
    EXPECT_TRUE(context_->RemoveWordBefore());
    CheckContext("", "", 0);
  }

  InsertCharacterChars("vaa");

  {
    SCOPED_TRACE("Removes an after character and make no sense");
    EXPECT_FALSE(context_->RemoveCharAfter());
    CheckContext("vaa", "", 0);
  }

  {
    SCOPED_TRACE("Removes an after word and make no sense");
    EXPECT_FALSE(context_->RemoveWordAfter());
    CheckContext("vaa", "", 0);
  }
}

TEST_F(EnglishContextTest, FocusCandidateIndex) {
  InsertCharacterChars("vaa");

  const size_t last_index = GetCandidatesSize() - 1;

  {
    SCOPED_TRACE("Focuses a last candidate");
    EXPECT_TRUE(context_->FocusCandidate(last_index));
    CheckContext("vaa", "", last_index);
  }

  {
    SCOPED_TRACE("Focuses a invalid candidate and make no sense");
    EXPECT_FALSE(context_->FocusCandidate(last_index + 1));
    CheckContext("vaa", "", last_index);
  }

  {
    SCOPED_TRACE("Focuses a 1st candidate");
    EXPECT_TRUE(context_->FocusCandidate(0));
    CheckContext("vaa", "", 0);
  }
}

TEST_F(EnglishContextTest, SelectCandidate) {
  InsertCharacterChars("vaa");

  {
    SCOPED_TRACE("Selects a 100th candidate and fails");
    EXPECT_FALSE(context_->SelectCandidate(100));
    CheckContext("vaa", "", 0);
  }

  {
    SCOPED_TRACE("Selects a 3rd candidate");
    EXPECT_CALL(dictionary_, LearnWord("aac")).Times(1);
    EXPECT_TRUE(context_->SelectCandidate(2));
    CheckContext("", "aac", 0);
  }
}

TEST_F(EnglishContextTest, NoMatchingInput) {
  const char *kInputText = "vaaaaaaaaaaa";
  const char *kCommitText = "aaaaaaaaaaa";

  {
    SCOPED_TRACE("Inserts. There are no matching words in the mock dictionary");
    InsertCharacterChars(kInputText);
    CheckContext(kInputText, "", 0);
    EXPECT_EQ(0, GetCandidatesSize());
  }

  {
    SCOPED_TRACE("Focuses a candidate and fails.");
    EXPECT_FALSE(context_->FocusCandidate(0));
    CheckContext(kInputText, "", 0);
  }

  {
    SCOPED_TRACE("Selects a candidate and fails.");
    EXPECT_FALSE(context_->SelectCandidate(0));
    CheckContext(kInputText, "", 0);
  }

  {
    SCOPED_TRACE("Commits.");
    EXPECT_CALL(dictionary_, LearnWord(kCommitText)).Times(1);
    context_->Commit();
    CheckContext("", kCommitText, 0);
  }
}

TEST_F(EnglishContextTest, LongInput) {
  ASSERT_TRUE(context_->Insert('v'));

  const size_t kMaxInputLength = 80;
  for (size_t i = 0; i < kMaxInputLength - 1; ++i) {
    EXPECT_TRUE(context_->Insert('a'));
  }

  const string input_text = context_->input_text();
  EXPECT_FALSE(context_->Insert('a'));
  EXPECT_EQ(input_text, context_->input_text());
}

TEST_F(EnglishContextTest, FullWidthMode) {
  session_config_->full_width_word_mode = true;

  {
    SCOPED_TRACE("Inserts characters with full width mode");
    InsertCharacterChars("va");
    CheckContext("va", "", 0);
  }

  {
    SCOPED_TRACE("Commits full width a");
    EXPECT_CALL(dictionary_, LearnWord("a")).Times(1);
    context_->Commit();
    // "ａ"
    CheckContext("", "\xEF\xBD\x81", 0);
  }

  InsertCharacterChars("va");

  {
    SCOPED_TRACE("Selects aaa with full width mode");
    EXPECT_CALL(dictionary_, LearnWord("aaa")).Times(1);
    context_->SelectCandidate(0);
    CheckContext("", "aaa", 0);
  }
}

}  // namespace english
}  // namespace pinyin
}  // namespace mozc
