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

#include "base/stl_util.h"

#include "testing/base/public/gunit.h"

// This templatized helper can subclass any class and count how many times
// instances have been constructed or destructed.  It is used to make sure
// that deletes are actually occuring.

template <class T>
class InstanceCounter: public T {
 public:
  InstanceCounter<T>() : T () {
    ++instance_count;
  }
  ~InstanceCounter<T>() {
    --instance_count;
  }
  static int instance_count;
};
template <class T>
int InstanceCounter<T>::instance_count = 0;

TEST(STLDeleteElementsTest, STLDeleteElements) {
  vector<InstanceCounter<string> *> v;
  v.push_back(new InstanceCounter<string>());
  v.push_back(new InstanceCounter<string>());
  v.push_back(new InstanceCounter<string>());
  EXPECT_EQ(3, InstanceCounter<string>::instance_count);
  v.push_back(NULL);
  EXPECT_EQ(3, InstanceCounter<string>::instance_count);
  mozc::STLDeleteElements(&v);
  EXPECT_EQ(0, InstanceCounter<string>::instance_count);
  EXPECT_EQ(0, v.size());

  // Deleting NULL pointers to containers is ok.
  vector<InstanceCounter<string> *> *p = NULL;
  mozc::STLDeleteElements(p);
}
