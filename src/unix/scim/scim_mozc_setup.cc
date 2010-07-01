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

#include <gtk/gtk.h>

#include <string>

#include "base/const.h"
#include "base/process.h"
#include "base/run_level.h"
#include "base/util.h"

namespace scim {
class ConfigPointer;
}

namespace {

void Spawn(const string &tool) {
  string args = "--mode=" + tool;
  mozc::Process::SpawnMozcProcess(mozc::kMozcTool, args);
}

bool IsRunLevelNormal() {
  return (mozc::RunLevel::GetRunLevel(mozc::RunLevel::CLIENT) ==
          mozc::RunLevel::NORMAL);
}

void OnButton1Clicked(GtkButton *button, gpointer user_data) {
  Spawn("dictionary_tool");
}

void OnButton2Clicked(GtkButton *button, gpointer user_data) {
  Spawn("config_dialog");
}

void OnLabelClicked(GtkButton *button, gpointer user_data) {
  Spawn("about_dialog");
}

}  // namespace

// SCIM interface functions defined in modules/SetupUI/scim_imengine_setup.h.
extern "C" {
  void mozc_setup_LTX_scim_module_init() {
  }

  void mozc_setup_LTX_scim_module_exit() {
  }

  string mozc_setup_LTX_scim_setup_module_get_category() {
    return "IMEngine";
  }

  string mozc_setup_LTX_scim_setup_module_get_name() {
    return "Mozc";
  }

  string mozc_setup_LTX_scim_setup_module_get_description() {
    return "Mozc IME";
  }

  void mozc_setup_LTX_scim_setup_module_load_config(
      const scim::ConfigPointer &config) {
  }

  void mozc_setup_LTX_scim_setup_module_save_config(
      const scim::ConfigPointer &config) {
  }

  bool mozc_setup_LTX_scim_setup_module_query_changed() {
    return false;
  }

  GtkWidget *mozc_setup_LTX_scim_setup_module_create_ui() {
    GtkWidget *vbox;
    GtkWidget *eventbox;
    GtkWidget *label;

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox);

    if (mozc::Util::FileExists(mozc::Util::JoinPath(
            mozc::Util::GetServerDirectory(), mozc::kMozcTool))) {
      if (IsRunLevelNormal()) {
        GtkWidget *button1;
        GtkWidget *button2;

        button1 = gtk_button_new_with_mnemonic("Dictionary tool");
        gtk_widget_show(button1);
        gtk_box_pack_start(GTK_BOX(vbox), button1, FALSE, FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(button1), 5);

        button2 = gtk_button_new_with_mnemonic("Property");
        gtk_widget_show(button2);
        gtk_box_pack_start(GTK_BOX(vbox), button2, FALSE, FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(button2), 5);

        g_signal_connect(static_cast<gpointer>(button1), "clicked",
                         G_CALLBACK(OnButton1Clicked),
                         NULL);
        g_signal_connect(static_cast<gpointer>(button2), "clicked",
                         G_CALLBACK(OnButton2Clicked),
                         NULL);
      }

      eventbox = gtk_event_box_new();
      gtk_widget_show(eventbox);
      gtk_box_pack_end(GTK_BOX(vbox), eventbox, FALSE, FALSE, 0);
      gtk_event_box_set_above_child(GTK_EVENT_BOX(eventbox), FALSE);

      label = gtk_label_new(NULL);
      gtk_label_set_markup(
          GTK_LABEL(label),
          // Pango markup format.
          "<span foreground=\"blue\" underline=\"single\">About Mozc</span>");
      gtk_widget_show(label);
      gtk_container_add(GTK_CONTAINER(eventbox), label);
      gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
      gtk_misc_set_padding(GTK_MISC(label), 10, 10);

      g_signal_connect(static_cast<gpointer>(eventbox), "button_press_event",
                       G_CALLBACK(OnLabelClicked),
                       NULL);
    }

    return vbox;
  }
}  // extern "C"
