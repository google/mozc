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

#ifndef MOZC_BASE_WIN32_COM_IMPLEMENTS_H_
#define MOZC_BASE_WIN32_COM_IMPLEMENTS_H_

#include <guiddef.h>
#include <objbase.h>
#include <unknwn.h>
#include <windows.h>

#include <atomic>
#include <type_traits>

#include "absl/base/casts.h"

namespace mozc::win32 {
namespace com_implements_internal {

// Reference counter for the COM module. Use CanComModuleUnloadNow() to
// determine if the COM module can unload safely.
constinit extern std::atomic<int> com_module_ref_count;

// Placeholder to prevent classes from deriving another ComImplement.
struct ComImplementsBaseTag {};

}  // namespace com_implements_internal

// Returns true if the COM module doesn't have any active objects.
// Simply call this function to implement DllCanUnloadNow().
// Note that the return value is HRESULT, so S_FALSE is 1.
HRESULT CanComModuleUnloadNow();

// Default traits for ComImplements.
struct ComImplementsTraits {
  // OnObjectRelease is called when each COM instance is released.
  // The remaining object count is passed as ref.
  static inline void OnObjectRelease(ULONG ref) {}
};

// This is the default implementation of IsIIDOf. If the COM interface derives
// another COM interface (not IUnknown), explicitly define an overload of
// IsIIDOf.
// For example, ITfLangBarItemButton derives from ITfLangBarItem. In order to
// answer QueryInterface() for IID_ITfLangBarItem, define:
//
// template<>
// IsIIDOf<ITfLangBarItemButton>(REFIID riid) {
//   return IsIIDOf<ITfLangBarItemButton, ITfLangBarItem>(riid);
// }
//
// This way, IsIIDOf<ITfLangBarItemButton>() will check if
// `riid` is for one of the specified interface types (ITfLangBarItemButton and
// ITfLangBarItem in this example).
template <typename... Interfaces>
bool IsIIDOf(REFIID riid) {
  return (... || IsEqualIID(riid, __uuidof(Interfaces)));
}

// ComImplements is the base class for COM implementation classes and implements
// the IUnknown methods.
//
// class FooBar : public ComImplements<ComImplementsTraits, IFoo, IBar> {
//  ...
// }
//
// Note that you need to define a specialization of IsIIDOf<IFoo>() if IFoo
// is not an immediate derived interface of IUnknown.
template <typename Traits, typename... Interfaces>
class ComImplements : public com_implements_internal::ComImplementsBaseTag,
                      public Interfaces... {
 public:
  static_assert(
      std::conjunction_v<std::is_convertible<Interfaces *, IUnknown *>...>,
      "COM interfaces must derive from IUnknown.");
  static_assert(
      !std::disjunction_v<std::is_base_of<
          Interfaces, com_implements_internal::ComImplementsBaseTag>...>,
      "Do not derive from ComImplements multiple times.");

  ComImplements() { ++com_implements_internal::com_module_ref_count; }
  // Disallow copies and movies.
  ComImplements(const ComImplements &) = delete;
  ComImplements &operator=(const ComImplements &) = delete;
  virtual ~ComImplements() {
    Traits::OnObjectRelease(--com_implements_internal::com_module_ref_count);
  }

  // IUnknown methods.
  // ComImplements provides standard implementations.
  // TODO(yuryu): Make these final by reorganizing implementation classes.
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP QueryInterface(REFIID riid, void **out) override;

 protected:
  template <typename T, typename... Rest>
  void *QueryInterfaceImpl(REFIID riid);

 private:
  std::atomic<ULONG> ref_count_{0};
};

template <typename Traits, typename... Interfaces>
STDMETHODIMP_(ULONG)
ComImplements<Traits, Interfaces...>::AddRef() {
  // AddRef() can occur in any order.
  return ref_count_.fetch_add(1, std::memory_order_relaxed) + 1;
}

template <typename Traits, typename... Interfaces>
STDMETHODIMP_(ULONG)
ComImplements<Traits, Interfaces...>::Release() {
  // We need to make sure that all `Release()`s happens-before the final
  // `Release()` that actually deletes `this`.
  // This can be achieved by using acquire-release in `fetch_sub` below, but
  // we slightly optimize it by just `release`-ing in `fetch_sub` and adding a
  // fence in case we observe `ref_count_` down to 0.
  const ULONG new_value =
      ref_count_.fetch_sub(1, std::memory_order_release) - 1;
  if (new_value == 0) {
    std::atomic_thread_fence(std::memory_order_acquire);
    delete this;
  }
  return new_value;
}

template <typename Traits, typename... Interfaces>
STDMETHODIMP ComImplements<Traits, Interfaces...>::QueryInterface(REFIID riid,
                                                                  void **out) {
  if (out == nullptr) {
    return E_POINTER;
  }
  *out = QueryInterfaceImpl<Interfaces...>(riid);
  if (*out == nullptr) {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

template <typename Traits, typename... Interfaces>
template <typename T, typename... Rest>
void *ComImplements<Traits, Interfaces...>::QueryInterfaceImpl(REFIID riid) {
  if (IsIIDOf<T>(riid)) {
    return absl::implicit_cast<T *>(this);
  }
  if constexpr (sizeof...(Rest) == 0) {
    // This is the last QueryInterfaceImpl in the list. Check for IUnknown.
    if (IsIIDOf<IUnknown>(riid)) {
      return absl::implicit_cast<IUnknown *>(absl::implicit_cast<T *>(this));
    }
    return nullptr;
  } else {
    // Continue the lookup.
    return QueryInterfaceImpl<Rest...>(riid);
  }
}

}  // namespace mozc::win32

#endif  // MOZC_BASE_WIN32_COM_IMPLEMENTS_H_
