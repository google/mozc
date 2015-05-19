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

#include <string>
#include "base/base.h"
#include "base/util.h"
#include "converter/segmenter.h"
#include "converter/segmenter_inl.h"
#include "converter/node.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/pos_matcher.h"
#include "testing/base/public/gunit.h"

namespace mozc {

class SegmenterTest : public testing::Test {
 protected:
  SegmenterTest() {
    segmenter_ = Singleton<Segmenter>::get();
    pos_matcher_ = UserPosManager::GetUserPosManager()->GetPOSMatcher();
  }

  Segmenter *segmenter_;
  const POSMatcher *pos_matcher_;
};

TEST_F(SegmenterTest, SegmenterTest) {
  for (size_t rid = 0; rid < kLSize; ++rid) {
    for (size_t lid = 0; lid < kRSize; ++lid) {
      EXPECT_EQ(IsBoundaryInternal(rid, lid),
                segmenter_->IsBoundary(rid, lid)) << rid << " " << lid;
    }
  }
}

TEST_F(SegmenterTest, SegmenterLNodeTest) {
  // lnode is BOS
  Node lnode, rnode;
  lnode.node_type = Node::BOS_NODE;
  rnode.node_type = Node::NOR_NODE;
  for (size_t rid = 0; rid < kLSize; ++rid) {
    for (size_t lid = 0; lid < kRSize; ++lid) {
      lnode.rid = rid;
      lnode.lid = lid;
      EXPECT_TRUE(segmenter_->IsBoundary(&lnode, &rnode, false));
      EXPECT_TRUE(segmenter_->IsBoundary(&lnode, &rnode, true));
    }
  }
}

TEST_F(SegmenterTest, SegmenterRNodeTest) {
  // rnode is EOS
  Node lnode, rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::EOS_NODE;
  for (size_t rid = 0; rid < kLSize; ++rid) {
    for (size_t lid = 0; lid < kRSize; ++lid) {
      lnode.rid = rid;
      lnode.lid = lid;
      EXPECT_TRUE(segmenter_->IsBoundary(&lnode, &rnode, false));
      EXPECT_TRUE(segmenter_->IsBoundary(&lnode, &rnode, true));
    }
  }
}

TEST_F(SegmenterTest, SegmenterNodeTest) {
  Node lnode, rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  for (size_t rid = 0; rid < kLSize; ++rid) {
    for (size_t lid = 0; lid < kRSize; ++lid) {
      lnode.rid = rid;
      rnode.lid = lid;
      EXPECT_EQ(segmenter_->IsBoundary(rid, lid),
                segmenter_->IsBoundary(&lnode, &rnode, false));
      EXPECT_FALSE(segmenter_->IsBoundary(&lnode, &rnode, true));
    }
  }
}

TEST_F(SegmenterTest, ParticleTest) {
  Node lnode, rnode;
  lnode.Init();
  rnode.Init();
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  // "助詞"
  lnode.rid = pos_matcher_->GetAcceptableParticleAtBeginOfSegmentId();
  // "名詞,サ変".
  rnode.lid = pos_matcher_->GetUnknownId();
  EXPECT_TRUE(segmenter_->IsBoundary(&lnode, &rnode, false));

  lnode.attributes |= Node::STARTS_WITH_PARTICLE;
  EXPECT_FALSE(segmenter_->IsBoundary(&lnode, &rnode, false));
}
}  // namespace mozc
