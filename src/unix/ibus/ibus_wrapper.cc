// Copyright 2010-2021, Google Inc.
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

#include "unix/ibus/ibus_wrapper.h"

#include <string>
#include <type_traits>

static_assert(std::is_same<gint, int>::value, "gint must be int.");
static_assert(std::is_same<guint, uint>::value, "guint must be uint.");
static_assert(std::is_same<gchar, char>::value, "gchar must be char.");
static_assert(std::is_same<gboolean, int>::value, "gboolean must be int.");

#if !IBUS_CHECK_VERSION(1, 5, 4)
#error "ibus-mozc requires IBus>=1.5.4"
#endif  // libibus (<1.5.4)


// GobjectWrapper

void GobjectWrapper::Unref() {
  GObject *obj = GetGobject();
  if (obj) {
    g_object_unref(obj);
  }
}
void GobjectWrapper::RefSink() {
  GObject *obj = GetGobject();
  if (obj) {
    g_object_ref_sink(obj);
  }
}


// IbusPropertyWrapper

IbusPropertyWrapper::IbusPropertyWrapper(IBusProperty *property)
    : property_(property) {}

IbusPropertyWrapper::IbusPropertyWrapper(const char *key, IBusPropType type,
                                         const std::string &label,
                                         const char *icon, IBusPropState state,
                                         IBusPropList *prop_list) {
  Initialize(key, type, label, icon, state, prop_list);
}

GObject *IbusPropertyWrapper::GetGobject() {
  return G_OBJECT(property_);
}

IBusProperty *IbusPropertyWrapper::GetProperty() { return property_; }

void IbusPropertyWrapper::Initialize(const char *key, IBusPropType type,
                                     const std::string &label, const char *icon,
                                     IBusPropState state,
                                     IBusPropList *prop_list) {
  IBusText *ibus_label = ibus_text_new_from_string(label.c_str());

  constexpr IBusText *tooltip = nullptr;
  constexpr bool sensitive = true;
  constexpr bool visible = true;

  property_ = ibus_property_new(key, type, ibus_label, icon, tooltip, sensitive,
                                visible, state, prop_list);
}

bool IbusPropertyWrapper::IsInitialized() { return property_ != nullptr; }

const char *IbusPropertyWrapper::GetKey() {
  return ibus_property_get_key(property_);
}

IbusPropertyWrapper IbusPropertyWrapper::GetSubProp(uint index) {
  IBusProperty *sub_prop =
      ibus_prop_list_get(ibus_property_get_sub_props(property_), index);
  return IbusPropertyWrapper(sub_prop);
}

void IbusPropertyWrapper::SetIcon(const char *icon) {
  ibus_property_set_icon(property_, icon);
}
void IbusPropertyWrapper::SetLabel(const char *label) {
  IBusText *ibus_label = ibus_text_new_from_string(label);
  ibus_property_set_label(property_, ibus_label);
}
void IbusPropertyWrapper::SetSymbol(const char *symbol) {
  IBusText *ibus_symbol = ibus_text_new_from_string(symbol);
  ibus_property_set_symbol(property_, ibus_symbol);
}
void IbusPropertyWrapper::SetState(IBusPropState state) {
  ibus_property_set_state(property_, state);
}


// IbusPropListWrapper

IbusPropListWrapper::IbusPropListWrapper() {
  prop_list_ = ibus_prop_list_new();
}

GObject *IbusPropListWrapper::GetGobject() {
  return G_OBJECT(prop_list_);
}

IBusPropList *IbusPropListWrapper::GetPropList() { return prop_list_; }

void IbusPropListWrapper::Append(IbusPropertyWrapper *property) {
  // `prop_list_` owns appended item.
  // `g_object_ref_sink` is internally called.
  ibus_prop_list_append(prop_list_, property->GetProperty());
}


// IbusTextWrapper

IbusTextWrapper::IbusTextWrapper(const std::string &text) {
  text_ = ibus_text_new_from_string(text.c_str());
}

IBusText *IbusTextWrapper::GetText() { return text_; }

// `end_index` is `int` by following the base function.
// https://ibus.github.io/docs/ibus-1.5/IBusText.html#ibus-text-append-attribute
void IbusTextWrapper::AppendAttribute(uint type, uint value, uint start_index,
                                      int end_index) {
  ibus_text_append_attribute(text_, type, value, start_index, end_index);
}

// IbusEngineWrapper

IbusEngineWrapper::IbusEngineWrapper(IBusEngine *engine) : engine_(engine) {}

IBusEngine *IbusEngineWrapper::GetEngine() { return engine_; }

const char *IbusEngineWrapper::GetName() {
  return ibus_engine_get_name(engine_);
}

void IbusEngineWrapper::GetContentType(uint *purpose, uint *hints) {
  ibus_engine_get_content_type(engine_, purpose, hints);
}

void IbusEngineWrapper::CommitText(const std::string &text) {
  IBusText *ibus_text = ibus_text_new_from_string(text.c_str());
  ibus_engine_commit_text(engine_, ibus_text);
  // `ibus_text` is released by ibus_engine_commit_text.
}

void IbusEngineWrapper::UpdatePreeditTextWithMode(IbusTextWrapper *text,
                                                  int cursor) {
  constexpr bool visible = true;
  ibus_engine_update_preedit_text_with_mode(
      engine_, text->GetText(), cursor, visible, IBUS_ENGINE_PREEDIT_COMMIT);
}

void IbusEngineWrapper::HidePreeditText() {
  ibus_engine_hide_preedit_text(engine_);
}


void IbusEngineWrapper::RegisterProperties(IbusPropListWrapper *properties) {
  ibus_engine_register_properties(engine_, properties->GetPropList());
}

void IbusEngineWrapper::UpdateProperty(IbusPropertyWrapper *property) {
  ibus_engine_update_property(engine_, property->GetProperty());
}

void IbusEngineWrapper::EnableSurroundingText() {
  // If engine wants to use surrounding text, we should call
  // ibus_engine_get_surrounding_text once when the engine enabled.
  // https://ibus.github.io/docs/ibus-1.5/IBusEngine.html#ibus-engine-get-surrounding-text
  ibus_engine_get_surrounding_text(engine_, nullptr, nullptr, nullptr);
}

const char *IbusEngineWrapper::GetSurroundingText(uint *cursor_pos,
                                                  uint *anchor_pos) {
  // DO NOT call g_object_unref against this.
  // http://developer.gnome.org/gobject/stable/gobject-The-Base-Object-Type.html#gobject-The-Base-Object-Type.description
  IBusText *text = nullptr;
  ibus_engine_get_surrounding_text(engine_, &text, cursor_pos, anchor_pos);
  return ibus_text_get_text(text);
}

void IbusEngineWrapper::DeleteSurroundingText(int offset, uint size) {
  // Nowadays 'ibus_engine_delete_surrounding_text' becomes functional on
  // many of the major applications.  Confirmed that it works on
  // Firefox 10.0, LibreOffice 3.3.4 and GEdit 3.2.3.
  ibus_engine_delete_surrounding_text(engine_, offset, size);
}

uint IbusEngineWrapper::GetCapabilities() {
  return engine_->client_capabilities;
}

bool IbusEngineWrapper::CheckCapabilities(uint capabilities) {
  return (engine_->client_capabilities & capabilities) == capabilities;
}

IbusEngineWrapper::Rectangle IbusEngineWrapper::GetCursorArea() {
  const IBusRectangle &cursor_area = engine_->cursor_area;
  return {cursor_area.x, cursor_area.y, cursor_area.width, cursor_area.height};
}


// IbusComponentWrapper

IbusComponentWrapper::IbusComponentWrapper(
    const char *name, const char *description, const char *version,
    const char *license, const char *author, const char *homepage,
    const char *command_line, const char *textdomain) {
  component_ = ibus_component_new(name, description, version, license, author,
                                  homepage, command_line, textdomain);
}

GObject *IbusComponentWrapper::GetGobject() { return G_OBJECT(component_); }

IBusComponent *IbusComponentWrapper::GetComponent() { return component_; }

void IbusComponentWrapper::AddEngine(const char *name, const char *longname,
                                     const char *description,
                                     const char *language, const char *license,
                                     const char *author, const char *icon,
                                     const char *layout) {
  ibus_component_add_engine(
      component_, ibus_engine_desc_new(name, longname, description, language,
                                       license, author, icon, layout));
}

std::vector<const char *> IbusComponentWrapper::GetEngineNames() {
  std::vector<const char *> names;
  GList *engines = ibus_component_get_engines(component_);
  for (GList *p = engines; p; p = p->next) {
    IBusEngineDesc *engine = reinterpret_cast<IBusEngineDesc *>(p->data);
    const char *engine_name = ibus_engine_desc_get_name(engine);
    names.push_back(engine_name);
  }
  return names;
}


// IbusBusWrapper

IbusBusWrapper::IbusBusWrapper() { bus_ = ibus_bus_new(); }

IBusBus *IbusBusWrapper::GetBus() { return bus_; }

void IbusBusWrapper::AddEngines(const std::vector<const char *> engine_names,
                                GType type) {
  IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus_));
  for (const char *engine_name : engine_names) {
    ibus_factory_add_engine(factory, engine_name, type);
  }
}
void IbusBusWrapper::RequestName(const char *name) {
  constexpr uint32_t flags = 0;
  ibus_bus_request_name(bus_, name, flags);
}
void IbusBusWrapper::RegisterComponent(IbusComponentWrapper *component) {
  ibus_bus_register_component(bus_, component->GetComponent());
}
