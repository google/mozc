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

#include "base/win32/win_api_test_helper.h"

#include <windows.h>
#include <winnt.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "base/util.h"

namespace mozc {
namespace {
typedef WinAPITestHelper::FunctionPointer FunctionPointer;

void WINAPI dummy_func() {}
static_assert(sizeof(&dummy_func) == sizeof(FunctionPointer),
              "check function pointer size failed.");

struct Thunk {
  FunctionPointer proc;
};

class ThunkRewriter {
 public:
  ThunkRewriter(const Thunk* thunk, FunctionPointer proc)
      : thunk_(thunk), proc_(proc) {}

  bool Rewrite() const {
    // Note: There is a race condition between the first VirtualProtect and
    // second VirtualProtect.

    auto* writable_thunk = const_cast<Thunk*>(thunk_);

    DWORD original_protect = 0;
    auto result = ::VirtualProtect(writable_thunk, sizeof(*writable_thunk),
                                   PAGE_READWRITE, &original_protect);
    if (result == 0) {
      const auto error = ::GetLastError();
      LOG(FATAL) << "VirtualProtect failed. error = " << error;
      return false;
    }

    // Here we have write access to the |writable_thunk|.
    writable_thunk->proc = proc_;

    DWORD dummy = 0;
    result = ::VirtualProtect(writable_thunk, sizeof(*writable_thunk),
                              original_protect, &dummy);
    if (result == 0) {
      const auto error = ::GetLastError();
      LOG(FATAL) << "VirtualProtect failed. error = " << error;
      return false;
    }
    return true;
  }

 private:
  // Represents the memory address of API thunk.
  const Thunk* thunk_;
  // Represents the true address of API implementation.
  FunctionPointer proc_;
};

class HookTargetInfo {
 public:
  explicit HookTargetInfo(
      absl::Span<const WinAPITestHelper::HookRequest> requests) {
    for (size_t i = 0; i < requests.size(); ++i) {
      const auto& request = requests[i];
      HMODULE module_handle = nullptr;
      const auto result =
          ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN,
                               request.module_name.c_str(), &module_handle);
      if (result == 0) {
        const auto error = ::GetLastError();
        LOG(FATAL) << "GetModuleHandleExA failed. error = " << error;
        continue;
      }
      const FunctionPointer original_proc_address =
          ::GetProcAddress(module_handle, request.proc_name.c_str());
      if (original_proc_address == nullptr) {
        LOG(FATAL) << "GetProcAddress returned nullptr.";
        continue;
      }

      std::string module_name = request.module_name;
      Util::LowerString(&module_name);
      info_[module_name][original_proc_address] = request.new_proc_address;
    }
  }

  bool IsTargetModule(const std::string& module_name) const {
    std::string lower_module_name(module_name);
    Util::LowerString(&lower_module_name);
    return info_.find(lower_module_name) != info_.end();
  }

  FunctionPointer GetNewProc(const std::string& module_name,
                             FunctionPointer original_proc) const {
    std::string lower_module_name(module_name);
    Util::LowerString(&lower_module_name);
    const auto module_iterator = info_.find(lower_module_name);
    if (module_iterator == info_.end()) {
      return nullptr;
    }
    const auto& proc_map = module_iterator->second;
    const auto proc_iterator = proc_map.find(original_proc);
    if (proc_iterator == proc_map.end()) {
      return nullptr;
    }
    return proc_iterator->second;
  }

 private:
  std::map<std::string, std::map<FunctionPointer, FunctionPointer>> info_;
};

class PortableExecutableImage {
 public:
  explicit PortableExecutableImage(HMODULE module_handle)
      : module_handle_(module_handle), is_invalid_image_(false) {
    if (module_handle_ == nullptr) {
      is_invalid_image_ = true;
      return;
    }
    const auto* dos_header = At<IMAGE_DOS_HEADER>(0);
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
      is_invalid_image_ = true;
      return;
    }
    const auto* nt_header = At<IMAGE_NT_HEADERS>(dos_header->e_lfanew);
    if (nt_header->Signature != IMAGE_NT_SIGNATURE) {
      is_invalid_image_ = true;
      return;
    }
  }

  template <typename T>
  const T* At(DWORD offset) const {
    static_assert(std::is_pod<T>::value, "T should be POD.");

    CHECK(!is_invalid_image_);

    // TODO(yukawa): Validate if this memory range is safe to be accessed.
    return reinterpret_cast<const T*>(
        reinterpret_cast<const uint8_t*>(module_handle_) + offset);
  }

  bool IsValid() const { return !is_invalid_image_; }

 private:
  HMODULE module_handle_;
  bool is_invalid_image_;
};

class ImageImportDescriptorIterator {
 public:
  explicit ImageImportDescriptorIterator(const PortableExecutableImage& image)
      : image_(image),
        import_directory_(nullptr),
        descriptor_index_(0),
        descriptor_index_max_(0) {
    if (!image_.IsValid()) {
      return;
    }
    const auto* dos_header = image_.At<IMAGE_DOS_HEADER>(0);
    const auto* nt_header = image_.At<IMAGE_NT_HEADERS>(dos_header->e_lfanew);
    import_directory_ =
        &nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    descriptor_index_max_ =
        import_directory_->Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
  }

  const IMAGE_IMPORT_DESCRIPTOR& Get() const {
    const auto* desc = GetInternal();
    CHECK_NE(0, desc->Name);
    return *desc;
  }

  void Next() { ++descriptor_index_; }

  bool Done() const {
    if (!image_.IsValid()) {
      return true;
    }
    if (descriptor_index_ >= descriptor_index_max_) {
      return true;
    }
    if (GetInternal()->Name == 0) {
      return true;
    }
    return false;
  }

 private:
  const IMAGE_IMPORT_DESCRIPTOR* GetInternal() const {
    CHECK_LT(descriptor_index_, descriptor_index_max_);
    const DWORD import_descriptor_offset =
        import_directory_->VirtualAddress +
        descriptor_index_ * sizeof(IMAGE_IMPORT_DESCRIPTOR);
    return image_.At<IMAGE_IMPORT_DESCRIPTOR>(import_descriptor_offset);
  }

  const PortableExecutableImage& image_;
  const IMAGE_DATA_DIRECTORY* import_directory_;
  size_t descriptor_index_;
  size_t descriptor_index_max_;
};

class ImageThunkDataIterator {
 public:
  ImageThunkDataIterator(const PortableExecutableImage& image,
                         const IMAGE_IMPORT_DESCRIPTOR& import_descriptor)
      : image_(image), import_descriptor_(import_descriptor), thunk_index_(0) {}

  const Thunk* Get() const {
    CHECK(!Done());
    const auto* raw_thunk = GetInternal();
    return reinterpret_cast<const Thunk*>(&raw_thunk->u1.Function);
  }

  void Next() { ++thunk_index_; }

  bool Done() const { return GetInternal()->u1.Function == 0; }

 private:
  const IMAGE_THUNK_DATA* GetInternal() const {
    const DWORD thunk_offset =
        import_descriptor_.FirstThunk + thunk_index_ * sizeof(IMAGE_THUNK_DATA);
    return image_.At<IMAGE_THUNK_DATA>(thunk_offset);
  }

  const PortableExecutableImage& image_;
  const IMAGE_IMPORT_DESCRIPTOR& import_descriptor_;
  size_t thunk_index_;
};

}  // namespace

class WinAPITestHelper::RestoreInfo {
 public:
  std::vector<ThunkRewriter> rewrites;
};

WinAPITestHelper::HookRequest::HookRequest(const std::string& src_module,
                                           const std::string& src_proc_name,
                                           FunctionPointer new_proc_addr)
    : module_name(src_module),
      proc_name(src_proc_name),
      new_proc_address(new_proc_addr) {}

// static
WinAPITestHelper::RestoreInfoHandle WinAPITestHelper::DoHook(
    HMODULE target_module, absl::Span<const HookRequest> requests) {
  const HookTargetInfo target_info(requests);

  // Following code skips some data validations as this code is only used in
  // unit tests.

  PortableExecutableImage image(target_module);
  CHECK(image.IsValid());

  std::unique_ptr<RestoreInfo> restore_info(new RestoreInfo());
  for (ImageImportDescriptorIterator descriptor_iterator(image);
       !descriptor_iterator.Done(); descriptor_iterator.Next()) {
    const auto& descriptor = descriptor_iterator.Get();
    const std::string module_name(image.At<char>(descriptor.Name));
    if (!target_info.IsTargetModule(module_name)) {
      continue;
    }
    for (ImageThunkDataIterator thunk_iterator(image, descriptor);
         !thunk_iterator.Done(); thunk_iterator.Next()) {
      const auto* thunk = thunk_iterator.Get();
      const auto original_proc_address = thunk->proc;
      const auto target_proc_address =
          target_info.GetNewProc(module_name, original_proc_address);
      if (target_proc_address == nullptr) {
        continue;
      }
      // Rewrite rule to do hook.
      ThunkRewriter hook_rewriter(thunk, target_proc_address);
      // Rewrite rule to restore hook.
      ThunkRewriter backup_rewriter(thunk, original_proc_address);
      CHECK(hook_rewriter.Rewrite());
      restore_info->rewrites.push_back(backup_rewriter);
    }
  }
  return restore_info.release();
}

void WinAPITestHelper::RestoreHook(
    WinAPITestHelper::RestoreInfoHandle restore_info) {
  std::unique_ptr<RestoreInfo> info(restore_info);  // takes ownership

  for (size_t i = 0; i < info->rewrites.size(); ++i) {
    const auto& rewrite = info->rewrites[i];
    CHECK(rewrite.Rewrite());
  }
}

}  // namespace mozc
