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

#ifndef MOZC_UNIX_IBUS_IBUS_WRAPPER_H_
#define MOZC_UNIX_IBUS_IBUS_WRAPPER_H_

#include <ibus.h>

#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "absl/strings/string_view.h"

namespace mozc {
namespace ibus {

class GobjectWrapper {
 public:
  explicit GobjectWrapper() = default;
  virtual ~GobjectWrapper() = default;
  virtual GObject *GetGobject() = 0;

  void Unref();
  void RefSink();

  // https://docs.gtk.org/gobject/method.Object.get_data.html
  template <typename T>
  const T *GetData(absl::string_view key) {
    void *data = g_object_get_data(GetGobject(), key.data());
    return reinterpret_cast<const T *>(data);
  }

  template <typename T>
  void SetData(absl::string_view key, const T &data) {
    g_object_set_data(GetGobject(), key.data(),
                      reinterpret_cast<void *>(const_cast<T *>(&data)));
  }

  template <typename C, typename D>
  ulong SignalConnect(absl::string_view signal, C callback, D *user_data) {
    GObject *obj = GetGobject();
    if (obj == nullptr) {
      return 0;
    }
    return g_signal_connect(obj, signal.data(),
                            reinterpret_cast<GCallback>(callback),
                            reinterpret_cast<gpointer>(user_data));
  }

  void SignalHandlerDisconnect(ulong id);
};

class GsettingsWrapper : public GobjectWrapper {
 public:
  explicit GsettingsWrapper(GSettings *settings);
  explicit GsettingsWrapper(absl::string_view schema_name);
  virtual ~GsettingsWrapper() = default;

  GObject *GetGobject() override;
  GSettings *GetGsettings();

  bool IsInitialized();

  using Variant = std::variant<bool, std::string>;
  Variant GetVariant(std::string_view key);

 private:
  GSettings *settings_;  // Does not take the ownership.
};

class IbusPropertyWrapper : public GobjectWrapper {
 public:
  explicit IbusPropertyWrapper(IBusProperty *property);
  virtual ~IbusPropertyWrapper() = default;

  IbusPropertyWrapper(absl::string_view key, IBusPropType type,
                      absl::string_view label, absl::string_view icon,
                      IBusPropState state, IBusPropList *prop_list);

  GObject *GetGobject() override;

  void Initialize(absl::string_view key, IBusPropType type,
                  absl::string_view label, absl::string_view icon,
                  IBusPropState state, IBusPropList *prop_list);

  IBusProperty *GetProperty();

  bool IsInitialized();

  absl::string_view GetKey();
  IbusPropertyWrapper GetSubProp(uint index);

  void SetIcon(absl::string_view icon);
  void SetLabel(absl::string_view label);
  void SetSymbol(absl::string_view symbol);
  void SetState(IBusPropState state);

 private:
  IBusProperty *property_;  // Does not take the ownership.
};

class IbusPropListWrapper : public GobjectWrapper {
 public:
  IbusPropListWrapper();
  virtual ~IbusPropListWrapper() = default;

  GObject *GetGobject() override;

  IBusPropList *GetPropList();

  void Append(IbusPropertyWrapper* property);

 private:
  IBusPropList *prop_list_;  // Does not take the ownership.
};

class IbusTextWrapper {
 public:
  explicit IbusTextWrapper(IBusText *text);
  explicit IbusTextWrapper(absl::string_view text);
  ~IbusTextWrapper() = default;

  IBusText *GetText();
  bool IsInitialized();

  // `end_index` is `int` by following the base function.
  // https://ibus.github.io/docs/ibus-1.5/IBusText.html#ibus-text-append-attribute
  void AppendAttribute(uint type, uint value, uint start_index, int end_index);

 private:
  IBusText *text_;  // Does not take the ownership.
};

class IbusLookupTableWrapper {
 public:
  IbusLookupTableWrapper(size_t page_size, int cursor_pos, bool cursor_visible);
  ~IbusLookupTableWrapper() = default;

  IBusLookupTable *GetLookupTable();

  void AppendCandidate(absl::string_view candidate);
  void AppendLabel(absl::string_view label);

  // https://ibus.github.io/docs/ibus-1.5/ibus-ibustypes.html#IBusOrientation-enum
  // IBUS_ORIENTATION_HORIZONTAL = 0,
  // IBUS_ORIENTATION_VERTICAL   = 1,
  // IBUS_ORIENTATION_SYSTEM     = 2,
  void SetOrientation(IBusOrientation orientation);

 private:
  IBusLookupTable *table_;  // Does not take the ownership.
};


class IbusEngineWrapper {
 public:
  explicit IbusEngineWrapper(IBusEngine *engine);
  ~IbusEngineWrapper() = default;

  IBusEngine *GetEngine();

  absl::string_view GetName();

  void GetContentType(uint *purpose, uint *hints);

  void CommitText(absl::string_view text);

  void UpdatePreeditTextWithMode(IbusTextWrapper *text, int cursor);
  void ClearPreeditText();
  void HidePreeditText();

  void RegisterProperties(IbusPropListWrapper *properties);
  void UpdateProperty(IbusPropertyWrapper *property);

  void EnableSurroundingText();
  absl::string_view GetSurroundingText(uint *cursor_pos, uint *anchor_pos);
  void DeleteSurroundingText(int offset, uint size);

  uint GetCapabilities();
  bool CheckCapabilities(uint capabilities);

  struct Rectangle {
    int x;
    int y;
    int width;
    int height;
  };
  Rectangle GetCursorArea();

  void ShowLookupTable();
  void HideLookupTable();
  void UpdateLookupTable(IbusLookupTableWrapper *table);

  void ShowAuxiliaryText();
  void HideAuxiliaryText();
  void UpdateAuxiliaryText(absl::string_view auxiliary_text);

 private:
  IBusEngine *engine_;  // Does not take the ownership.
};

class IbusComponentWrapper : public GobjectWrapper {
 public:
  IbusComponentWrapper(absl::string_view name, absl::string_view description,
                       absl::string_view version, absl::string_view license,
                       absl::string_view author, absl::string_view homepage,
                       absl::string_view command_line,
                       absl::string_view textdomain);
  ~IbusComponentWrapper() = default;

  GObject *GetGobject() override;

  IBusComponent *GetComponent();

  void AddEngine(absl::string_view name, absl::string_view longname,
                 absl::string_view description, absl::string_view language,
                 absl::string_view license, absl::string_view author,
                 absl::string_view icon, absl::string_view layout);

  std::vector<absl::string_view> GetEngineNames();

 private:
  IBusComponent *component_;  // Does not take the ownership.
};

class IbusBusWrapper : public GobjectWrapper {
 public:
  IbusBusWrapper();
  ~IbusBusWrapper() = default;

  GObject *GetGobject() override;
  IBusBus *GetBus();

  void AddEngines(const std::vector<absl::string_view> engine_names,
                  GType type);
  void RequestName(absl::string_view name);
  void RegisterComponent(IbusComponentWrapper *component);

 private:
  IBusBus *bus_;  // Does not take the ownership.
};

class IbusWrapper {
 public:
  static void Init();
  static void Main();
  static void Quit();
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_IBUS_WRAPPER_H_
