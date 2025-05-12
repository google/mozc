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

#include <cstdint>
#include <string>
#include <type_traits>

#include "absl/strings/string_view.h"

static_assert(std::is_same<gint, int>::value, "gint must be int.");
static_assert(std::is_same<guint, uint>::value, "guint must be uint.");
static_assert(std::is_same<gulong, ulong>::value, "guint must be ulong.");
static_assert(std::is_same<gchar, char>::value, "gchar must be char.");
static_assert(std::is_same<gboolean, int>::value, "gboolean must be int.");
static_assert(std::is_same<gpointer, void *>::value, "gpointer must be void*.");

#if !IBUS_CHECK_VERSION(1, 5, 4)
#error "ibus-mozc requires IBus>=1.5.4"
#endif  // libibus (<1.5.4)

namespace mozc {
namespace ibus {

namespace {
absl::string_view MakeStringView(const char *str) {
  // If str is nullptr, returns null_sv as the default string_view.
  // Then null_sv.data() returns nullptr.
  // Note, std::string_view requires this treatment,
  // but absl::string_view handles it internally.
  constexpr absl::string_view null_sv = {};
  return str ? str : null_sv;
}
}  // namespace

// GobjectWrapper

void GobjectWrapper::Unref() {
  GObject *obj = GetGobject();
  if (obj == nullptr) {
    return;
  }
  g_object_unref(obj);
}

void GobjectWrapper::RefSink() {
  GObject *obj = GetGobject();
  if (obj == nullptr) {
    return;
  }
  g_object_ref_sink(obj);
}

void GobjectWrapper::SignalHandlerDisconnect(ulong id) {
  GObject *obj = GetGobject();
  if (obj == nullptr) {
    return;
  }
  g_signal_handler_disconnect(obj, id);
}


// GsettingsWrapper

namespace {
GSettings *CreateGsettings(absl::string_view schema_name) {
  GSettingsSchemaSource *schema_source = g_settings_schema_source_get_default();
  if (schema_source == nullptr) {
    return nullptr;
  }
  GSettingsSchema *schema =
      g_settings_schema_source_lookup(schema_source, schema_name.data(), TRUE);
  if (schema == nullptr) {
    return nullptr;
  }
  g_settings_schema_unref(schema);
  return g_settings_new(schema_name.data());
}
}  // namespace

GsettingsWrapper::GsettingsWrapper(GSettings *settings) : settings_(settings) {}
GsettingsWrapper::GsettingsWrapper(absl::string_view schema_name)
    : settings_(CreateGsettings(schema_name)) {}

GObject *GsettingsWrapper::GetGobject() { return G_OBJECT(settings_); }
GSettings *GsettingsWrapper::GetGsettings() { return settings_; }

bool GsettingsWrapper::IsInitialized() { return settings_ != nullptr; }

GsettingsWrapper::Variant GsettingsWrapper::GetVariant(std::string_view key) {
  GsettingsWrapper::Variant value;
  GVariant *variant = g_settings_get_value(settings_, key.data());
  if (g_variant_classify(variant) == G_VARIANT_CLASS_BOOLEAN) {
    value = static_cast<bool>(g_variant_get_boolean(variant));
  } else if (g_variant_classify(variant) == G_VARIANT_CLASS_STRING) {
    value = std::string(g_variant_get_string(variant, nullptr));
  }
  g_variant_unref(variant);
  return value;
}


// IbusPropertyWrapper

IbusPropertyWrapper::IbusPropertyWrapper(IBusProperty *property)
    : property_(property) {}

IbusPropertyWrapper::IbusPropertyWrapper(
    absl::string_view key, IBusPropType type, absl::string_view label,
    absl::string_view icon, IBusPropState state, IBusPropList *prop_list) {
  Initialize(key.data(), type, label.data(), icon.data(), state, prop_list);
}

GObject *IbusPropertyWrapper::GetGobject() {
  return G_OBJECT(property_);
}

IBusProperty *IbusPropertyWrapper::GetProperty() { return property_; }

void IbusPropertyWrapper::Initialize(absl::string_view key, IBusPropType type,
                                     absl::string_view label,
                                     absl::string_view icon,
                                     IBusPropState state,
                                     IBusPropList *prop_list) {
  IBusText *ibus_label = ibus_text_new_from_string(label.data());

  constexpr IBusText *tooltip = nullptr;
  constexpr bool sensitive = true;
  constexpr bool visible = true;

  property_ =
      ibus_property_new(key.data(), type, ibus_label, icon.data(),
                        tooltip, sensitive, visible, state, prop_list);
}

bool IbusPropertyWrapper::IsInitialized() { return property_ != nullptr; }

absl::string_view IbusPropertyWrapper::GetKey() {
  const char *key = ibus_property_get_key(property_);
  return MakeStringView(key);
}

IbusPropertyWrapper IbusPropertyWrapper::GetSubProp(uint index) {
  IBusProperty *sub_prop =
      ibus_prop_list_get(ibus_property_get_sub_props(property_), index);
  return IbusPropertyWrapper(sub_prop);
}

void IbusPropertyWrapper::SetIcon(absl::string_view icon) {
  ibus_property_set_icon(property_, icon.data());
}
void IbusPropertyWrapper::SetLabel(absl::string_view label) {
  IBusText *ibus_label = ibus_text_new_from_string(label.data());
  ibus_property_set_label(property_, ibus_label);
}
void IbusPropertyWrapper::SetSymbol(absl::string_view symbol) {
  IBusText *ibus_symbol = ibus_text_new_from_string(symbol.data());
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

IbusTextWrapper::IbusTextWrapper(IBusText *text) {
  text_ = text;
}
IbusTextWrapper::IbusTextWrapper(absl::string_view text) {
  text_ = ibus_text_new_from_string(text.data());
}

IBusText *IbusTextWrapper::GetText() { return text_; }
bool IbusTextWrapper::IsInitialized() { return text_ != nullptr; }

// `end_index` is `int` by following the base function.
// https://ibus.github.io/docs/ibus-1.5/IBusText.html#ibus-text-append-attribute
void IbusTextWrapper::AppendAttribute(uint type, uint value, uint start_index,
                                      int end_index) {
  ibus_text_append_attribute(text_, type, value, start_index, end_index);
}


// IbusLookupTableWrapper

IbusLookupTableWrapper::IbusLookupTableWrapper(size_t page_size, int cursor_pos,
                                               bool cursor_visible) {
  constexpr bool round = true;  // for lookup table wrap around.
  table_ = ibus_lookup_table_new(page_size, cursor_pos, cursor_visible, round);
}

IBusLookupTable *IbusLookupTableWrapper::GetLookupTable() { return table_; }

void IbusLookupTableWrapper::AppendCandidate(absl::string_view candidate) {
  // `text` is released by ibus_engine_update_lookup_table along with `table_`.
  IBusText *text = ibus_text_new_from_string(candidate.data());
  ibus_lookup_table_append_candidate(table_, text);
}
void IbusLookupTableWrapper::AppendLabel(absl::string_view label) {
  IBusText *text = ibus_text_new_from_string(label.data());
  ibus_lookup_table_append_label(table_, text);
}

void IbusLookupTableWrapper::SetOrientation(IBusOrientation orientation) {
  ibus_lookup_table_set_orientation(table_, orientation);
}


// IbusEngineWrapper

IbusEngineWrapper::IbusEngineWrapper(IBusEngine *engine) : engine_(engine) {}

IBusEngine *IbusEngineWrapper::GetEngine() { return engine_; }

absl::string_view IbusEngineWrapper::GetName() {
  return MakeStringView(ibus_engine_get_name(engine_));
}

void IbusEngineWrapper::GetContentType(uint *purpose, uint *hints) {
  ibus_engine_get_content_type(engine_, purpose, hints);
}

void IbusEngineWrapper::CommitText(absl::string_view text) {
  IBusText *ibus_text = ibus_text_new_from_string(text.data());
  ibus_engine_commit_text(engine_, ibus_text);
  // `ibus_text` is released by ibus_engine_commit_text.
}

void IbusEngineWrapper::UpdatePreeditTextWithMode(IbusTextWrapper *text,
                                                  int cursor) {
  constexpr bool kVisible = true;
  ibus_engine_update_preedit_text_with_mode(
      engine_, text->GetText(), cursor, kVisible, IBUS_ENGINE_PREEDIT_COMMIT);
}

void IbusEngineWrapper::ClearPreeditText() {
  constexpr int kCursor = 0;
  constexpr bool kVisible = false;
  IBusText *empty_text = ibus_text_new_from_string("");
  ibus_engine_update_preedit_text_with_mode(
      engine_, empty_text, kCursor, kVisible, IBUS_ENGINE_PREEDIT_CLEAR);
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

absl::string_view IbusEngineWrapper::GetSurroundingText(uint *cursor_pos,
                                                       uint *anchor_pos) {
  // DO NOT call g_object_unref against this.
  // http://developer.gnome.org/gobject/stable/gobject-The-Base-Object-Type.html#gobject-The-Base-Object-Type.description
  IBusText *text = nullptr;
  ibus_engine_get_surrounding_text(engine_, &text, cursor_pos, anchor_pos);
  return MakeStringView(ibus_text_get_text(text));
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

void IbusEngineWrapper::ShowLookupTable() {
  ibus_engine_show_lookup_table(engine_);
}

void IbusEngineWrapper::HideLookupTable() {
  ibus_engine_hide_lookup_table(engine_);
}

void IbusEngineWrapper::UpdateLookupTable(IbusLookupTableWrapper *table) {
  constexpr bool visible = true;
  // `table` is released by ibus_engine_update_lookup_table.
  ibus_engine_update_lookup_table(engine_, table->GetLookupTable(), visible);
}

void IbusEngineWrapper::ShowAuxiliaryText() {
  ibus_engine_show_auxiliary_text(engine_);
}

void IbusEngineWrapper::HideAuxiliaryText() {
  ibus_engine_hide_auxiliary_text(engine_);
}

void IbusEngineWrapper::UpdateAuxiliaryText(absl::string_view auxiliary_text) {
  constexpr bool visible = true;
  // `text` is released by ibus_engine_update_auxiliary_text.
  IBusText *text = ibus_text_new_from_string(auxiliary_text.data());
  ibus_engine_update_auxiliary_text(engine_, text, visible);
}


// IbusComponentWrapper

IbusComponentWrapper::IbusComponentWrapper(
    absl::string_view name, absl::string_view description,
    absl::string_view version, absl::string_view license,
    absl::string_view author, absl::string_view homepage,
    absl::string_view command_line, absl::string_view textdomain) {
  component_ = ibus_component_new(
      name.data(), description.data(), version.data(), license.data(),
      author.data(), homepage.data(), command_line.data(), textdomain.data());
}

GObject *IbusComponentWrapper::GetGobject() { return G_OBJECT(component_); }

IBusComponent *IbusComponentWrapper::GetComponent() { return component_; }

void IbusComponentWrapper::AddEngine(
    absl::string_view name, absl::string_view longname,
    absl::string_view description, absl::string_view language,
    absl::string_view license, absl::string_view author, absl::string_view icon,
    absl::string_view layout) {
  ibus_component_add_engine(
      component_,
      ibus_engine_desc_new(name.data(), longname.data(), description.data(),
                           language.data(), license.data(), author.data(),
                           icon.data(), layout.data()));
}

std::vector<absl::string_view > IbusComponentWrapper::GetEngineNames() {
  std::vector<absl::string_view> names;
  GList *engines = ibus_component_get_engines(component_);
  for (GList *p = engines; p; p = p->next) {
    IBusEngineDesc *engine = reinterpret_cast<IBusEngineDesc *>(p->data);
    const char *engine_name = ibus_engine_desc_get_name(engine);
    names.push_back(MakeStringView(engine_name));
  }
  return names;
}


// IbusBusWrapper

IbusBusWrapper::IbusBusWrapper() { bus_ = ibus_bus_new(); }

GObject *IbusBusWrapper::GetGobject() { return G_OBJECT(bus_); }

IBusBus *IbusBusWrapper::GetBus() { return bus_; }

void IbusBusWrapper::AddEngines(
    const std::vector<absl::string_view> engine_names, GType type) {
  IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus_));
  for (absl::string_view engine_name : engine_names) {
    ibus_factory_add_engine(factory, engine_name.data(), type);
  }
}
void IbusBusWrapper::RequestName(absl::string_view name) {
  constexpr uint32_t flags = 0;
  ibus_bus_request_name(bus_, name.data(), flags);
}
void IbusBusWrapper::RegisterComponent(IbusComponentWrapper *component) {
  ibus_bus_register_component(bus_, component->GetComponent());
}


// IbusWrapper

// static
void IbusWrapper::Init() { ibus_init(); }

// static
void IbusWrapper::Main() { ibus_main(); }

// static
void IbusWrapper::Quit() { ibus_quit(); }

}  // namespace ibus
}  // namespace mozc
