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

#ifndef MOZC_UNIX_IBUS_IBUS_HEADER_H_
#define MOZC_UNIX_IBUS_IBUS_HEADER_H_

#include <ibus.h>  // IWYU pragma: export

#include <string>
#include <type_traits>

static_assert(std::is_same<gint, int>::value, "gint must be int.");
static_assert(std::is_same<guint, uint>::value, "guint must be uint.");
static_assert(std::is_same<gchar, char>::value, "gchar must be char.");
static_assert(std::is_same<gboolean, int>::value, "gboolean must be int.");

#if !IBUS_CHECK_VERSION(1, 5, 4)
#error "ibus-mozc requires IBus>=1.5.4"
#endif  // libibus (<1.5.4)

class IbusEngineWrapper {
 public:
  explicit IbusEngineWrapper(IBusEngine *engine): engine_(engine) {}
  ~IbusEngineWrapper() = default;

  IBusEngine *GetEngine() { return engine_; }

  const char *GetName() { return ibus_engine_get_name(engine_); }

  void GetContentType(uint *purpose, uint *hints) {
    ibus_engine_get_content_type(engine_, purpose, hints);
  }

  void CommitText(const std::string &text) {
    IBusText *ibus_text = ibus_text_new_from_string(text.c_str());
    ibus_engine_commit_text(engine_, ibus_text);
    // `ibus_text` is released by ibus_engine_commit_text.
  }

  void RegisterProperties(IBusPropList *properties) {
    ibus_engine_register_properties(engine_, properties);
  }

  void UpdateProperty(IBusProperty *property) {
    ibus_engine_update_property(engine_, property);
  }

  void EnableSurroundingText() {
    // If engine wants to use surrounding text, we should call
    // ibus_engine_get_surrounding_text once when the engine enabled.
    // https://ibus.github.io/docs/ibus-1.5/IBusEngine.html#ibus-engine-get-surrounding-text
    ibus_engine_get_surrounding_text(engine_, nullptr, nullptr, nullptr);
  }

  const char *GetSurroundingText(uint *cursor_pos, uint *anchor_pos) {
    // DO NOT call g_object_unref against this.
    // http://developer.gnome.org/gobject/stable/gobject-The-Base-Object-Type.html#gobject-The-Base-Object-Type.description
    IBusText *text = nullptr;
    ibus_engine_get_surrounding_text(engine_, &text, cursor_pos, anchor_pos);
    return ibus_text_get_text(text);
  }

  void DeleteSurroundingText(int offset, uint size) {
    // Nowadays 'ibus_engine_delete_surrounding_text' becomes functional on
    // many of the major applications.  Confirmed that it works on
    // Firefox 10.0, LibreOffice 3.3.4 and GEdit 3.2.3.
    ibus_engine_delete_surrounding_text(engine_, offset, size);
  }

  uint GetCapabilities() { return engine_->client_capabilities; }

  bool CheckCapabilities(uint capabilities) {
    return (engine_->client_capabilities & capabilities) == capabilities;
  }

 private:
  IBusEngine *engine_;  // Does not take the ownership.
};

class GobjectWrapper {
 public:
  explicit GobjectWrapper() = default;
  virtual ~GobjectWrapper() = default;
  virtual GObject *GetGobject() = 0;

  void Unref() {
    GObject *obj = GetGobject();
    if (obj) {
      g_object_unref(obj);
    }
  }
  void RefSink() {
    GObject *obj = GetGobject();
    if (obj) {
      g_object_ref_sink(obj);
    }
  }

  // https://docs.gtk.org/gobject/method.Object.get_data.html
  template <typename T>
  const T *GetData(const char *key) {
    void *data = g_object_get_data(GetGobject(), key);
    return reinterpret_cast<const T *>(data);
  }

  template <typename T>
  void SetData(const char *key, const T &data) {
    g_object_set_data(GetGobject(), key,
                      reinterpret_cast<void *>(const_cast<T *>(&data)));
  }
};

class IbusPropertyWrapper : public GobjectWrapper {
 public:
  explicit IbusPropertyWrapper(IBusProperty *property): property_(property) {}
  virtual ~IbusPropertyWrapper() = default;

  IbusPropertyWrapper(const char *key, IBusPropType type,
                      const std::string &label, const char *icon,
                      IBusPropState state, IBusPropList *prop_list) {
    property_ = New(key, type, label, icon, state, prop_list);
  }

  GObject *GetGobject() override { return G_OBJECT(property_); }

  static IBusProperty *New(const char *key, IBusPropType type,
                           const std::string &label, const char *icon,
                           IBusPropState state, IBusPropList *prop_list) {
    IBusText *ibus_label = ibus_text_new_from_string(label.c_str());

    constexpr IBusText *tooltip = nullptr;
    constexpr bool sensitive = true;
    constexpr bool visible = true;

    return ibus_property_new(key, type, ibus_label, icon, tooltip, sensitive,
                             visible, state, prop_list);
  }

  void Initialize(const char *key, IBusPropType type, const std::string &label,
                  const char *icon, IBusPropState state,
                  IBusPropList *prop_list) {
    property_ = New(key, type, label, icon, state, prop_list);
  }

  IBusProperty *GetProperty() { return property_; }

  bool IsInitialized() { return property_ != nullptr; }

  const char *GetKey() {
    return ibus_property_get_key(property_);
  }
  IbusPropertyWrapper GetSubProp(uint index) {
    IBusProperty *sub_prop =
        ibus_prop_list_get(ibus_property_get_sub_props(property_), index);
    return IbusPropertyWrapper(sub_prop);
  }

  void SetIcon(const char *icon) {
    ibus_property_set_icon(property_, icon);
  }
  void SetLabel(const char *label) {
    IBusText *ibus_label = ibus_text_new_from_string(label);
    ibus_property_set_label(property_, ibus_label);
  }
  void SetSymbol(const char *symbol) {
    IBusText *ibus_symbol = ibus_text_new_from_string(symbol);
    ibus_property_set_symbol(property_, ibus_symbol);
  }
  void SetState(IBusPropState state) {
    ibus_property_set_state(property_, state);
  }

 private:
  IBusProperty *property_;  // Does not take ownership.
};

class IbusPropListWrapper : public GobjectWrapper {
 public:
  IbusPropListWrapper() {
    prop_list_ = ibus_prop_list_new();
  }
  virtual ~IbusPropListWrapper() = default;

  GObject *GetGobject() override { return G_OBJECT(prop_list_); }

  IBusPropList *GetPropList() { return prop_list_; }

  void Append(IbusPropertyWrapper* property) {
    // `prop_list_` owns appended item.
    // `g_object_ref_sink` is internally called.
    ibus_prop_list_append(prop_list_, property->GetProperty());
  }

 private:
  IBusPropList *prop_list_;  // Does not take ownership.
};

#endif  // MOZC_UNIX_IBUS_IBUS_HEADER_H_
