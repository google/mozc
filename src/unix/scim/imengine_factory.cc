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

#include "unix/scim/imengine_factory.h"

#include "base/logging.h"
#include "unix/scim/scim_mozc.h"

#ifndef SCIM_MOZC_ICON_FILE
  #define SCIM_MOZC_ICON_FILE (SCIM_ICONDIR "/scim-mozc.png")
#endif

namespace {

const char kLanguage[] = "ja_JP";
const char kUUID[] = "d13c8a1c-5c16-4fa8-83ff-f7f5e6b70256";
const char kEngineName[] = "Mozc";
const char kAuthors[] = "Google Inc.";
const char kCredits[] = "Copyright 2010 Google Inc. All Rights Reserved.";

}  // namespace

namespace mozc_unix_scim {

IMEngineFactory::IMEngineFactory(const scim::ConfigPointer *config)
    : config_(config) {
  set_languages(kLanguage);
}

IMEngineFactory::~IMEngineFactory() {
}

// Implements scim::IMEngineFactoryBase::get_name().
scim::WideString IMEngineFactory::get_name() const {
  return scim::utf8_mbstowcs(kEngineName);
}

// Implements scim::IMEngineFactoryBase::get_uuid().
scim::String IMEngineFactory::get_uuid() const {
  return kUUID;
}

// Implements scim::IMEngineFactoryBase::get_icon_file().
scim::String IMEngineFactory::get_icon_file() const {
  return SCIM_MOZC_ICON_FILE;
}

// Implements scim::IMEngineFactoryBase::get_authors().
scim::WideString IMEngineFactory::get_authors() const {
  return scim::utf8_mbstowcs(kAuthors);
}

// Implements scim::IMEngineFactoryBase::get_credits().
scim::WideString IMEngineFactory::get_credits() const {
  return scim::utf8_mbstowcs(kCredits);
}

// Implements scim::IMEngineFactoryBase::get_help().
scim::WideString IMEngineFactory::get_help() const {
  return scim::utf8_mbstowcs("No help available.");
}

// Implements scim::IMEngineFactoryBase::create_instance().
scim::IMEngineInstancePointer IMEngineFactory::create_instance(
    const scim::String &encoding, int id) {
  VLOG(1) << "Create ScimMozc";
  return ScimMozc::CreateScimMozc(this, encoding, id, config_);
}

}  // namespace mozc_unix_scim
