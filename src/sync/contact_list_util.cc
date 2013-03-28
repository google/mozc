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

#include "sync/contact_list_util.h"

#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "dictionary/user_dictionary_storage.pb.h"
#include "dictionary/user_dictionary_util.h"
#include "net/jsoncpp.h"

namespace mozc {
namespace sync {

// The input string |contact_update| must be a json script, and is expected to
// have a structure like described below.
// { "feed" : { "entry" : {
//   [
//     "gd$name" : {
//       "gd$familyName" : { "$t" : "具卯", "yomi" : "ぐう" },
//       "gd$givenName" : { "$t" : "狗流", "yomi" : "ぐる" }
//     }
//   ],
//   [ ... (other members' information like above) ... ],
// }}}
// Please refer [1] and [2] for its details.
// [1] http://code.google.com/intl/ja/apis/gdata/docs/2.0/elements.html
// [2] http://code.google.com/intl/ja/apis/gdata/docs/json.html
bool ContactListUtil::ParseContacts(
    const string &contact_update,
    user_dictionary::UserDictionary *user_dictionary,
    string *last_timestamp) {
  DCHECK(user_dictionary);
  DCHECK(last_timestamp);

  // convert Json script to Json class
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(contact_update, root)) {
    LOG(INFO) << "Parsing contact information failed.";
    return false;
  }

  if (!root.isMember("feed")) {
    LOG(INFO) << "contact_update has no feed member";
    return false;
  }
  if (!root["feed"].isMember("entry")) {
    LOG(INFO) << "contact_update has no entry member";
    return false;
  }

  Json::Value &members = root["feed"]["entry"];
  for (Json::Value::iterator it = members.begin(); it != members.end(); ++it) {
    if (!(*it).isMember("gd$name")) {
      continue;
    }
    Json::Value &name = (*it)["gd$name"];

    string kanji;
    string yomi;
    // Some users may set their full name in family name field or given name
    // field, so we check both fields to make full names.
    if (name.isMember("gd$familyName")) {
      Json::Value &family = name["gd$familyName"];
      if (family.isMember("$t")) {
        kanji += family["$t"].asString();
      }
      if (family.isMember("yomi")) {
        yomi += family["yomi"].asString();
      }
    }
    if (name.isMember("gd$givenName")) {
      Json::Value &given = name["gd$givenName"];
      if (given.isMember("$t")) {
        kanji += given["$t"].asString();
      }
      if (given.isMember("yomi")) {
        yomi += given["yomi"].asString();
      }
    }

    if (kanji.empty() || yomi.empty()) {
      continue;
    }

    // push <normalize(yomi), kanji> into dictionary;
    string normal_yomi;
    UserDictionaryUtil::NormalizeReading(yomi, &normal_yomi);
    user_dictionary::UserDictionary::Entry *entry =
        user_dictionary->add_entries();
    DCHECK(entry);
    entry->set_key(normal_yomi);
    entry->set_value(kanji);
    // "人名"
    entry->set_pos(user_dictionary::UserDictionary::PERSONAL_NAME);
  }

  *last_timestamp = root["feed"]["updated"]["$t"].asString();
  return true;
}

}  // namespace sync
}  // namespace mozc
