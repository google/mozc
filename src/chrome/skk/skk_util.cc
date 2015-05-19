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

#include "chrome/skk/skk_util.h"

#include <algorithm>
#include <set>

#include "base/scoped_ptr.h"
#include "converter/node_allocator.h"
#include "dictionary/system/system_dictionary.h"

namespace mozc {

namespace chrome {
namespace skk {

const size_t kMaxPredictions = 128;

// JSON message field names.
const char *kStatusDebug = "DEBUG";
const char *kStatusError = "ERROR";
const char *kStatusOK = "OK";

const char *kMethodLookup = "LOOKUP";

const char *kMessageBodyField = "body";
const char *kMessageIdField = "id";
const char *kMessageMethodField = "method";
const char *kMessageStatusField = "status";

const char *kMessageBaseField = "base";
const char *kMessageStemField = "stem";

const char *kMessageCandidatesField = "candidates";
const char *kMessagePredictionsField = "predictions";
const char *kMessageMessageField = "message";

}  // namespace skk
}  // namespace chrome

namespace {

bool NodeWcostLessThan(const mozc::Node *node1, const mozc::Node *node2) {
  return node1->wcost < node2->wcost;
}

}  // namespace

bool SkkUtil::IsSupportedMethod(const string &method) {
  return method == chrome::skk::kMethodLookup;
}

// Doesn't use unique() because candidates is not sorted in lexical order
void SkkUtil::RemoveDuplicateEntry(vector<string> *candidates) {
  set<string> seen_entries;
  for (vector<string>::iterator iter = candidates->begin();
       iter != candidates->end();
       /* empty */) {
    if (seen_entries.count(*iter) > 0) {
      iter = candidates->erase(iter);
    } else {
      seen_entries.insert(*iter);
      ++iter;
    }
  }
}

bool SkkUtil::ValidateMessage(const Json::Value &json_message,
                              string *error_message) {
  error_message->clear();

  if (!json_message.isObject()) {
    *error_message = "Message is not a object";
    return false;
  }

  if (json_message[chrome::skk::kMessageIdField].isNull()) {
    *error_message = "Required parameter \"id\" is unspecified";
    return false;
  }

  Json::Value method = json_message[chrome::skk::kMessageMethodField];
  if (!method.isString()
      || !IsSupportedMethod(method.asString())) {
    *error_message
        = "Required parameter \"method\" is unspecified or invalid";
    return false;
  }

  if (method.asString() == chrome::skk::kMethodLookup) {
    Json::Value base = json_message[chrome::skk::kMessageBaseField];
    if (!base.isString()) {
      *error_message
          = "Required parameter \"base\" is unspecified or invalid";
      return false;
    }

    Json::Value stem = json_message[chrome::skk::kMessageStemField];
    if (!stem.isString()) {
      *error_message
          = "Required parameter \"stem\" is unspecified or invalid";
      return false;
    }
  }

  // TODO(sekia): More strict check

  return true;
}

// This method keeps existing elements in |candidates| and |predictions|.
// Because when the word looking up for can be conjugated, this method will be
// called multiple times.
void SkkUtil::LookupEntry(mozc::dictionary::SystemDictionary *dictionary,
                          const string &reading,
                          vector<string> *candidates,
                          vector<string> *predictions) {
  scoped_ptr<mozc::NodeAllocator> allocator(new mozc::NodeAllocator());
  mozc::Node *node = dictionary->LookupPredictive(
      reading.c_str(), reading.size(), allocator.get());
  vector<mozc::Node *> candidate_nodes;
  vector<mozc::Node *> prediction_nodes;
  for (; node != NULL; node = node->bnext) {
    if (node->key == reading) {
      candidate_nodes.push_back(node);
    } else {
      prediction_nodes.push_back(node);
    }
  }

  sort(candidate_nodes.begin(), candidate_nodes.end(), NodeWcostLessThan);
  for (vector<mozc::Node *>::const_iterator iter =
       candidate_nodes.begin();
       iter != candidate_nodes.end();
       ++iter) {
    candidates->push_back((*iter)->value);
  }
  sort(prediction_nodes.begin(), prediction_nodes.end(),
            NodeWcostLessThan);
  for (vector<mozc::Node *>::const_iterator iter =
       prediction_nodes.begin();
       iter != prediction_nodes.end() &&
       predictions->size() <= chrome::skk::kMaxPredictions;
       ++iter) {
    predictions->push_back((*iter)->value);
  }
}

}  // namespace mozc
