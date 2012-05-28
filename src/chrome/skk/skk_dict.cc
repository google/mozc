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

#include "chrome/skk/skk_dict.h"

#include <string>
#include <vector>

#include "base/util.h"  // for RomanjiToHiragana
#include "chrome/skk/skk_util.h"
#include "converter/node_allocator.h"
#include "dictionary/embedded_dictionary_data.h"
#include "dictionary/system/system_dictionary.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace mozc {

namespace {

const char *kVowels[] = { "a", "i", "u", "e", "o" };
const int kNoId = -1;

}  // namespace

void SkkDictInstance::PostErrorMessage(int id, const string &error_message) {
  Json::Value response(Json::objectValue);
  response[mozc::chrome::skk::kMessageIdField]
      = id == kNoId ? Json::Value() : Json::Value(id);
  response[mozc::chrome::skk::kMessageStatusField]
      = Json::Value(mozc::chrome::skk::kStatusError);
  response[mozc::chrome::skk::kMessageMessageField]
      = Json::Value(error_message);
  PostMessage(Json::StyledWriter().write(response));
}

void SkkDictInstance::PostDebugMessage(const string &message) {
#ifdef DEBUG
  Json::Value response(Json::objectValue);
  response[mozc::chrome::skk::kMessageIdField] = Json::Value();
  response[mozc::chrome::skk::kMessageStatusField]
      = Json::Value(mozc::chrome::skk::kStatusDebug);
  response[mozc::chrome::skk::kMessageMessageField] = Json::Value(message);
  PostMessage(Json::StyledWriter().write(response));
#endif
}

void SkkDictInstance::LookupEntry(const Json::Value &request) {
  const string &base = request[mozc::chrome::skk::kMessageBaseField].asString();
  const string &stem = request[mozc::chrome::skk::kMessageStemField].asString();
  PostDebugMessage("base: " + base + ". stem: " + stem);

  vector<string> readings;
  if (stem.empty()) {
    readings.push_back(base);
  } else {
    for (size_t i = 0; i < arraysize(kVowels); ++i) {
      string kana;
      mozc::Util::RomanjiToHiragana(stem + kVowels[i], &kana);
      if (!kana.empty()) {
        readings.push_back(base + kana);
      }
    }
  }

  vector<string> candidates, predictions;
  for (size_t i = 0; i < readings.size(); ++i) {
    SkkUtil::LookupEntry(dictionary_, readings[i], &candidates, &predictions);
  }
  SkkUtil::RemoveDuplicateEntry(&candidates);
  SkkUtil::RemoveDuplicateEntry(&predictions);

  Json::Value candidate_values(Json::arrayValue);
  Json::Value prediction_values(Json::arrayValue);
  for (size_t i = 0; i < candidates.size(); ++i) {
    candidate_values.append(Json::Value(candidates[i]));
  }
  for (size_t i = 0; i < predictions.size(); ++i) {
    prediction_values.append(Json::Value(predictions[i]));
  }

  Json::Value response(Json::objectValue), body(Json::objectValue);
  response[mozc::chrome::skk::kMessageIdField]
      = Json::Value(request[mozc::chrome::skk::kMessageIdField].asInt());
  response[mozc::chrome::skk::kMessageStatusField]
      = Json::Value(mozc::chrome::skk::kStatusOK);
  body[mozc::chrome::skk::kMessageCandidatesField] = candidate_values;
  body[mozc::chrome::skk::kMessagePredictionsField] = prediction_values;
  response[mozc::chrome::skk::kMessageBodyField] = body;

  PostMessage(Json::StyledWriter().write(response));
}

SkkDictInstance::SkkDictInstance(PP_Instance instance)
    : pp::Instance(instance),
      dictionary_(mozc::SystemDictionary::CreateSystemDictionaryFromImage(
          kDictionaryData_data, kDictionaryData_size)) {
  DCHECK(dictionary_);
}

SkkDictInstance::~SkkDictInstance() {}

void SkkDictInstance::HandleMessage(const pp::Var &message) {
  string error_message;
  if (!message.is_string()) {
    error_message = "Message must be a string";
    PostErrorMessage(kNoId, error_message);
    return;
  }

  Json::Value parsed;
  if (!Json::Reader().parse(message.AsString(), parsed)) {
    error_message = "Error occured during JSON parsing";
    PostErrorMessage(kNoId, error_message);
    return;
  }

  if (!(SkkUtil::ValidateMessage(parsed, &error_message))) {
    PostErrorMessage(kNoId, error_message);
    return;
  }

  const string method
      = parsed[mozc::chrome::skk::kMessageMethodField].asString();
  if (method == mozc::chrome::skk::kMethodLookup) {
    LookupEntry(parsed);
    return;
  } else {
    PostErrorMessage(
        parsed[mozc::chrome::skk::kMessageIdField].asInt(),
        "Unknown method: " + method);
    return;
  }
}

SkkDictModule::SkkDictModule() {}

SkkDictModule::~SkkDictModule() {}

pp::Instance* SkkDictModule::CreateInstance(PP_Instance instance) {
  return new SkkDictInstance(instance);
}

}  // namespace mozc

// NaCl's entry point
namespace pp {
Module* CreateModule() {
  return new mozc::SkkDictModule;
}
}  // namespace pp
