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

#include "unix/ibus/gtk_candidate_window_handler.h"

#include <unistd.h>

#include "base/logging.h"
#include "renderer/renderer_command.pb.h"
#include "renderer/renderer_interface.h"
#include "renderer/unix/const.h"

namespace mozc {
namespace ibus {

namespace {
const char kDefaultFont[] = "SansSerif 11";
}  // namespace

GtkCandidateWindowHandler::GtkCandidateWindowHandler(
    renderer::RendererInterface *renderer)
    : renderer_(renderer),
      last_update_output_(new commands::Output()),
      use_custom_font_description_(false) {
}

GtkCandidateWindowHandler::~GtkCandidateWindowHandler() {
}

bool GtkCandidateWindowHandler::SendUpdateCommand(
    const commands::Output &output, bool visibility) const {
  using commands::RendererCommand;

  RendererCommand command;
  command.mutable_output()->CopyFrom(output);
  command.set_type(commands::RendererCommand::UPDATE);
  command.set_visible(visibility);
  RendererCommand::ApplicationInfo *appinfo
      = command.mutable_application_info();

  // Set pid
  COMPILE_ASSERT(sizeof(::getpid()) <= sizeof(appinfo->process_id()),
                 pid_t_value_size_check);
  appinfo->set_process_id(::getpid());

  // Do not set thread_id returned from ::pthread_self() because:
  // 1. the returned value is valid only in the caller process.
  // 2. the returned value may exceed uint32.
  // TODO(team): Consider to use ::gettid()

  // Set InputFramework type
  appinfo->set_input_framework(RendererCommand::ApplicationInfo::IBus);
  appinfo->set_pango_font_description(GetFontDescription());

  return renderer_->ExecCommand(command);
}

void GtkCandidateWindowHandler::Update(IBusEngine *engine,
                                       const commands::Output &output) {
  last_update_output_->CopyFrom(output);

  SendUpdateCommand(output, output.candidates().candidate_size() != 0);
}

void GtkCandidateWindowHandler::Hide(IBusEngine *engine) {
  SendUpdateCommand(*(last_update_output_.get()), false);
}

void GtkCandidateWindowHandler::Show(IBusEngine *engine) {
  SendUpdateCommand(*(last_update_output_.get()), true);
}

void GtkCandidateWindowHandler::OnIBusCustomFontDescriptionChanged(
    const string &custom_font_description) {
  custom_font_description_.assign(custom_font_description);
}

void GtkCandidateWindowHandler::OnIBusUseCustomFontDescriptionChanged(
    bool use_custom_font_description) {
  use_custom_font_description_ = use_custom_font_description;
}

string GtkCandidateWindowHandler::GetFontDescription() const {
  if (!use_custom_font_description_) {
    // TODO(nona): Load application default font settings.
    return kDefaultFont;
  }
  DCHECK(!custom_font_description_.empty());
  return custom_font_description_;
}

}  // namespace ibus
}  // namespace mozc
