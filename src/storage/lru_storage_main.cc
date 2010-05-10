// Copyright 2010, Google Inc.
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
#include "storage/lru_storage.h"

DEFINE_bool(create_db, false, "initialize database");
DEFINE_string(file, "test.db", "");
DEFINE_int32(size, 10, "size");

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  if (FLAGS_create_db) {
    CHECK(mozc::LRUStorage::CreateStorageFile(FLAGS_file.c_str(),
                                              static_cast<uint32>(4),
                                              FLAGS_size, 0xff02));
  }

  string line;
  vector<string> fields;
  mozc::LRUStorage s;
  CHECK(s.Open(FLAGS_file.c_str()));

  LOG(INFO) << "size=" << s.size();
  LOG(INFO) << "used_size=" << s.used_size();
  LOG(INFO) << "usage=" << 100.0 * s.used_size() / s.size() << "%";
  LOG(INFO) << "value_size=" << s.value_size();

  while (getline(cin, line)) {
    fields.clear();
    mozc::Util::SplitStringUsing(line, "\t ", &fields);
    if (fields.size() >=2 && fields[0] == "g") {
      uint32 lat;
      const char *v = s.Lookup(fields[1], &lat);
      if (v != NULL) {
        cout << "found " << fields[1] << "\t"
             << lat << "\t" << *reinterpret_cast<const uint32 *>(v) << endl;
      } else {
        cout << "not found " << fields[1] << endl;
      }
    } else if (fields.size() >= 3 && fields[0] == "i") {
      uint32 value = atoi32(fields[2].c_str());
      s.Insert(fields[1], reinterpret_cast<const char*>(&value));
    } else {
      LOG(INFO) << "unknown command: " << line;
    }
  }

  return 0;
}
