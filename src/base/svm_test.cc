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

#include "base/base.h"
#include "base/svm.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
TEST(SVM, TrainTest) {
  vector<vector<pair<int, double> > > x;
  vector<double> y;

  {
    vector<pair<int, double> > feature;
    feature.push_back(make_pair(1, 1.0));
    feature.push_back(make_pair(2, -1.0));
    feature.push_back(make_pair(3, 0.5));
    feature.push_back(make_pair(4, 0.2));
    x.push_back(feature);
    y.push_back(+1.0);
  }


  {
    vector<pair<int, double> > feature;
    feature.push_back(make_pair(1, 0.1));
    feature.push_back(make_pair(2, -2.0));
    feature.push_back(make_pair(3, -0.5));
    feature.push_back(make_pair(4, 0.4));
    x.push_back(feature);
    y.push_back(+1.0);
  }

  {
    vector<pair<int, double> > feature;
    feature.push_back(make_pair(1, -1.0));
    feature.push_back(make_pair(2, 2.0));
    feature.push_back(make_pair(3, 1.0));
    feature.push_back(make_pair(4, -2.0));
    x.push_back(feature);
    y.push_back(-1.0);
  }

  {
    vector<pair<int, double> > feature;
    feature.push_back(make_pair(1, -.0));
    feature.push_back(make_pair(2, 1.0));
    feature.push_back(make_pair(3, -0.5));
    feature.push_back(make_pair(4, 0.1));
    x.push_back(feature);
    y.push_back(-1.0);
  }

  vector<double> w;
  EXPECT_TRUE(SVM::Train(y, x, 0.1, &w));
}
}  // namespace mozc
