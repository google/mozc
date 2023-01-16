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

#include <string>
#include <vector>

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
  explicit IbusPropertyWrapper(IBusProperty *property);
  virtual ~IbusPropertyWrapper() = default;

  IbusPropertyWrapper(const char *key, IBusPropType type,
                      const std::string &label, const char *icon,
                      IBusPropState state, IBusPropList *prop_list);

  GObject *GetGobject() override;

  void Initialize(const char *key, IBusPropType type, const std::string &label,
                  const char *icon, IBusPropState state,
                  IBusPropList *prop_list);

  IBusProperty *GetProperty();

  bool IsInitialized();

  const char *GetKey();
  IbusPropertyWrapper GetSubProp(uint index);

  void SetIcon(const char *icon);
  void SetLabel(const char *label);
  void SetSymbol(const char *symbol);
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
  IbusTextWrapper(const std::string &text);
  ~IbusTextWrapper() = default;

  IBusText *GetText();

  // `end_index` is `int` by following the base function.
  // https://ibus.github.io/docs/ibus-1.5/IBusText.html#ibus-text-append-attribute
  void AppendAttribute(uint type, uint value, uint start_index, int end_index);

 private:
  IBusText *text_;  // Does not take the ownership.
};

class IbusEngineWrapper {
 public:
  explicit IbusEngineWrapper(IBusEngine *engine);
  ~IbusEngineWrapper() = default;

  IBusEngine *GetEngine();

  const char *GetName();

  void GetContentType(uint *purpose, uint *hints);

  void CommitText(const std::string &text);

  void UpdatePreeditTextWithMode(IbusTextWrapper *text, int cursor);

  void HidePreeditText();

  void RegisterProperties(IbusPropListWrapper *properties);

  void UpdateProperty(IbusPropertyWrapper *property);

  void EnableSurroundingText();

  const char *GetSurroundingText(uint *cursor_pos, uint *anchor_pos);

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

 private:
  IBusEngine *engine_;  // Does not take the ownership.
};

class IbusComponentWrapper : public GobjectWrapper {
 public:
  IbusComponentWrapper(const char *name, const char *description,
                    const char *version, const char *license,
                    const char *author, const char *homepage,
                    const char *command_line, const char *textdomain);
  ~IbusComponentWrapper() = default;

  GObject *GetGobject() override;

  IBusComponent *GetComponent();

  void AddEngine(const char *name, const char *longname,
                 const char *description, const char *language,
                 const char *license, const char *author, const char *icon,
                 const char *layout);

  std::vector<const char *> GetEngineNames();

 private:
  IBusComponent *component_;  // Does not take the ownership.
};

class IbusBusWrapper {
 public:
  IbusBusWrapper();
  ~IbusBusWrapper() = default;

  IBusBus *GetBus();

  void AddEngines(const std::vector<const char *> engine_names, GType type);
  void RequestName(const char *name);
  void RegisterComponent(IbusComponentWrapper *component);

 private:
  IBusBus *bus_;  // Does not take the ownership.
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_IBUS_WRAPPER_H_
