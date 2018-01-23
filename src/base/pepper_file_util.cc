// Copyright 2010-2018, Google Inc.
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

// TODO(horo): Write tests.

#ifdef OS_NACL

#include "base/pepper_file_util.h"

#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/cpp/file_io.h>
#include <ppapi/cpp/file_ref.h>
#include <ppapi/cpp/file_system.h>
#include <ppapi/utility/completion_callback_factory.h>

#include <set>

#include "base/logging.h"
#include "base/mmap_sync_interface.h"
#include "base/mutex.h"
#include "base/pepper_scoped_obj.h"
#include "base/singleton.h"
#include "base/unnamed_event.h"

namespace mozc {
namespace {

// Base class of Pepper FileIO operator classes.
class PepperFileOperator {
 public:
  PepperFileOperator(pp::Instance *instance, pp::FileSystem *file_system)
      : instance_(instance),
        file_system_(file_system),
        result_(PP_ERROR_FAILED) {
  }
  virtual ~PepperFileOperator() {}

 protected:
  pp::Instance *instance_;
  pp::FileSystem *file_system_;
  int32_t result_;
  UnnamedEvent event_;
};

class PepperFileReader : public PepperFileOperator {
 public:
  PepperFileReader(pp::Instance *instance, pp::FileSystem *file_system);
  virtual ~PepperFileReader() {}
  int32_t Read(const string &filename, string *buffer);

 private:
  // Called in Read()
  void ReadImpl(int32_t result);
  void OnFileOpen(int32_t result);
  void OnQuery(int32_t result);
  void OnRead(int32_t result);

  string filename_;
  string *buffer_;
  int32_t bytes_to_read_;
  int64_t offset_;
  PP_FileInfo file_info_;
  scoped_main_thread_destructed_object<pp::FileIO> file_io_;
  scoped_main_thread_destructed_object<pp::FileRef> file_ref_;
  pp::CompletionCallbackFactory<PepperFileReader> cc_factory_;
  DISALLOW_COPY_AND_ASSIGN(PepperFileReader);
};

PepperFileReader::PepperFileReader(pp::Instance *instance,
                                   pp::FileSystem *file_system)
    : PepperFileOperator(instance, file_system),
      buffer_(NULL),
      bytes_to_read_(0),
      offset_(0) {
  cc_factory_.Initialize(this);
}

int32_t PepperFileReader::Read(const string &filename, string *buffer) {
  VLOG(2) << "PepperFileReader::Read \"" << filename << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread())
      << "PepperFileReader::Read() can't be called in the main thread.";
  filename_ = filename;
  buffer_ = buffer;
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      cc_factory_.NewCallback(&PepperFileReader::ReadImpl));
  event_.Wait(-1);
  return result_;
}

void PepperFileReader::ReadImpl(int32_t result) {
  VLOG(2) << "PepperFileReader::ReadImpl: " << result;
  file_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  file_io_.reset(new pp::FileIO(instance_));
  const int32_t ret = file_io_->Open(
      *file_ref_, PP_FILEOPENFLAG_READ,
      cc_factory_.NewCallback(&PepperFileReader::OnFileOpen));
  VLOG(2) << "file_io_->Open ret: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    LOG(ERROR) << "file_io_->Open error. ret: " << ret
               << " [" << filename_ << "]";
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperFileReader::OnFileOpen(int32_t result) {
  VLOG(2) << "PepperFileReader::OnFileOpen: " << result;
  if (result != PP_OK) {
    LOG(ERROR) << "PepperFileReader::OnFileOpen error. ret: " << result
               << " [" << filename_ << "]";
    result_ = result;
    event_.Notify();
    return;
  }
  CHECK(file_io_.get());
  const int32_t ret = file_io_->Query(&file_info_,
      cc_factory_.NewCallback(&PepperFileReader::OnQuery));
  VLOG(2) << "file_io_->Query ret: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    LOG(ERROR) << "file_io_->Query error. ret: " << ret
               << " [" << filename_ << "]";
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperFileReader::OnQuery(int32_t result) {
  VLOG(2) << "PepperFileReader::OnQuery: " << result;
  if (result != PP_OK) {
    LOG(ERROR) << "PepperFileReader::OnQuery error. ret: " << result
               << " [" << filename_ << "]";
    result_ = result;
    event_.Notify();
    return;
  }
  bytes_to_read_ = file_info_.size;
  VLOG(2) << "  bytes_to_read_: " << bytes_to_read_;
  offset_ = 0;
  buffer_->resize(bytes_to_read_);
  OnRead(0);
}

void PepperFileReader::OnRead(int32_t bytes_read) {
  VLOG(2) << "PepperFileReader::OnRead: " << bytes_read;
  if (bytes_read < 0) {
    LOG(ERROR) << "OnRead error. [" << filename_ << "]";
    result_ = bytes_read;
    event_.Notify();
    return;
  }
  bytes_to_read_ -= bytes_read;
  VLOG(2) << "  bytes_to_read_: " << bytes_to_read_;
  if (bytes_to_read_ == 0) {
    result_ = PP_OK;
    event_.Notify();
    return;
  } else {
    CHECK(file_io_.get());
    offset_ += bytes_read;
    const int32_t ret = file_io_->Read(
        offset_, &(*buffer_)[offset_], bytes_to_read_,
        cc_factory_.NewCallback(&PepperFileReader::OnRead));
    VLOG(2) << "file_io_->Read ret: " << ret;
    if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
      LOG(ERROR) << "file_io_->Read error. ret: " << ret
                 << " [" << filename_ << "]";
      result_ = ret;
      event_.Notify();
      return;
    }
  }
}

class PepperFileWriter : public PepperFileOperator {
 public:
  PepperFileWriter(pp::Instance *instance, pp::FileSystem *file_system);
  virtual ~PepperFileWriter() {}
  int32_t Write(const string &filename, const string &buffer);

 private:
  // Called in Write()
  void WriteImpl(int32_t result);
  void OnFileOpen(int32_t result);
  void OnWrite(int32_t result);
  void OnFlush(int32_t result);
  void OnReset(int32_t result);

  string filename_;
  const string *buffer_;
  int64_t offset_;
  scoped_main_thread_destructed_object<pp::FileIO> file_io_;
  scoped_main_thread_destructed_object<pp::FileRef> file_ref_;
  pp::CompletionCallbackFactory<PepperFileWriter> cc_factory_;
  DISALLOW_COPY_AND_ASSIGN(PepperFileWriter);
};

PepperFileWriter::PepperFileWriter(pp::Instance *instance,
                                   pp::FileSystem *file_system)
    : PepperFileOperator(instance, file_system),
      buffer_(NULL),
      offset_(0) {
  cc_factory_.Initialize(this);
}

int32_t PepperFileWriter::Write(const string &filename, const string &buffer) {
  VLOG(2) << "PepperFileWriter::Write \"" << filename << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread())
      << "PepperFileWriter::Write() can't be called in the main thread.";
  filename_ = filename;
  buffer_ = &buffer;
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      cc_factory_.NewCallback(&PepperFileWriter::WriteImpl));
  event_.Wait(-1);
  return result_;
}

void PepperFileWriter::WriteImpl(int32_t result) {
  VLOG(2) << "PepperFileWriter::WriteImpl: " << result;
  file_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  file_io_.reset(new pp::FileIO(instance_));
  const int32_t ret = file_io_->Open(
      *file_ref_, PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE,
      cc_factory_.NewCallback(&PepperFileWriter::OnFileOpen));
  VLOG(2) << "file_io_->Open ret: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    LOG(ERROR) << "file_io_->Open error. ret: " << ret
               << " [" << filename_ << "]";
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperFileWriter::OnFileOpen(int32_t result) {
  VLOG(2) << "PepperFileWriter::OnFileOpen: " << result;
  if (result != PP_OK) {
    LOG(ERROR) << "PepperFileWriter::OnFileOpen error. ret: " << result
               << " [" << filename_ << "]";
    result_ = result;
    event_.Notify();
    return;
  }
  CHECK(file_io_.get());
  const int32_t ret = file_io_->SetLength(
      0, cc_factory_.NewCallback(&PepperFileWriter::OnReset));
  VLOG(2) << "file_io_->SetLength ret: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    LOG(ERROR) << "file_io_->SetLength error. ret: " << ret
               << " [" << filename_ << "]";
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperFileWriter::OnReset(int32_t result) {
  VLOG(2) << "PepperFileWriter::OnReset: " << result;
  if (result != PP_OK) {
    LOG(ERROR) << "PepperFileWriter::OnReset error. ret: " << result
               << " [" << filename_ << "]";
    result_ = result;
    event_.Notify();
    return;
  }
  CHECK(file_io_.get());
  offset_ = 0;
  OnWrite(0);
}

void PepperFileWriter::OnWrite(int32_t bytes_written) {
  VLOG(2) << "PepperFileWriter::OnWrite: " << bytes_written;
  if (bytes_written < 0) {
    LOG(ERROR) << "WriteCallback error. [" << filename_ << "]";
    result_ = bytes_written;
    event_.Notify();
    return;
  }
  CHECK(file_io_.get());
  offset_ += bytes_written;
  if (offset_ == buffer_->length()) {
    const int32_t ret = file_io_->Flush(
        cc_factory_.NewCallback(&PepperFileWriter::OnFlush));
    VLOG(2) << "file_io_->Flush ret: " << ret;
    if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
      LOG(ERROR) << "file_io_->Flush error. ret: " << ret
                 << " [" << filename_ << "]";
      result_ = ret;
      event_.Notify();
      return;
    }
  } else {
    const int32_t ret = file_io_->Write(
        offset_, &(*buffer_)[offset_],
        buffer_->length() - offset_,
        cc_factory_.NewCallback(&PepperFileWriter::OnWrite));
    VLOG(2) << "file_io_->Write ret: " << ret;
    if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
      LOG(ERROR) << "file_io_->Write error. ret: " << ret
                 << " [" << filename_ << "]";
      result_ = ret;
      event_.Notify();
      return;
    }
  }
}

void PepperFileWriter::OnFlush(int32_t result) {
  VLOG(2) << "PepperFileWriter::OnFlush: " << result;
  if (result < 0) {
    LOG(ERROR) << "FlushCallback error. ret: " << result
               << " [" << filename_ << "]";
    result_ = result;
    event_.Notify();
    return;
  }
  result_ = PP_OK;
  event_.Notify();
  return;
}

class PepperDirectoryCreator : public PepperFileOperator {
 public:
  PepperDirectoryCreator(pp::Instance *instance, pp::FileSystem *file_system);
  virtual ~PepperDirectoryCreator() {}
  int32_t CreateDirectory(const string &path);

 private:
  void CreateDirectoryImpl(int32_t result);
  void OnCreateDirectory(int32_t result);

  string path_;
  scoped_main_thread_destructed_object<pp::FileRef> file_ref_;
  pp::CompletionCallbackFactory<PepperDirectoryCreator> cc_factory_;
  DISALLOW_COPY_AND_ASSIGN(PepperDirectoryCreator);
};

PepperDirectoryCreator::PepperDirectoryCreator(pp::Instance *instance,
                                               pp::FileSystem *file_system)
    : PepperFileOperator(instance, file_system) {
  cc_factory_.Initialize(this);
}

int32_t PepperDirectoryCreator::CreateDirectory(const string &path) {
  VLOG(2) << "PepperDirectoryCreator::CreateDirectory \"" << path << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread())
      << ("PepperDirectoryCreator::CreateDirectory() can't be called"
          " in the main thread.");
  path_ = path;
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      cc_factory_.NewCallback(&PepperDirectoryCreator::CreateDirectoryImpl));
  event_.Wait(-1);
  return result_;
}

void PepperDirectoryCreator::CreateDirectoryImpl(int32_t result) {
  VLOG(2) << "PepperDirectoryCreator::CreateDirectoryImpl: " << result;
  file_ref_.reset(new pp::FileRef(*file_system_, path_.c_str()));
  const int32_t ret = file_ref_->MakeDirectory(
      PP_MAKEDIRECTORYFLAG_EXCLUSIVE,
      cc_factory_.NewCallback(&PepperDirectoryCreator::OnCreateDirectory));
  VLOG(2) << "file_ref_->CreateDirectory ret: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperDirectoryCreator::OnCreateDirectory(int32_t result) {
  VLOG(2) << "PepperDirectoryCreator::OnCreateDirectory: " << result;
  result_ = result;
  event_.Notify();
  return;
}

class PepperFileQuerer : public PepperFileOperator {
 public:
  PepperFileQuerer(pp::Instance *instance, pp::FileSystem *file_system);
  virtual ~PepperFileQuerer() {}
  int32_t Query(const string &filename, PP_FileInfo *info);

 private:
  // Called in Query()
  void QueryImpl(int32_t result);
  void OnFileOpen(int32_t result);
  void OnQuery(int32_t result);

  string filename_;
  PP_FileInfo file_info_;
  scoped_main_thread_destructed_object<pp::FileIO> file_io_;
  scoped_main_thread_destructed_object<pp::FileRef> file_ref_;
  pp::CompletionCallbackFactory<PepperFileQuerer> cc_factory_;
  DISALLOW_COPY_AND_ASSIGN(PepperFileQuerer);
};

PepperFileQuerer::PepperFileQuerer(pp::Instance *instance,
                                   pp::FileSystem *file_system)
    : PepperFileOperator(instance, file_system) {
  cc_factory_.Initialize(this);
}

int32_t PepperFileQuerer::Query(const string &filename, PP_FileInfo *info) {
  VLOG(2) << "PepperFileQuerer::Query \"" << filename << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread())
      << "PepperFileQuerer::Query() can't be called in the main thread.";
  filename_ = filename;
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      cc_factory_.NewCallback(&PepperFileQuerer::QueryImpl));
  event_.Wait(-1);
  if (result_ == PP_OK) {
    *info = file_info_;
  }
  return result_;
}

void PepperFileQuerer::QueryImpl(int32_t result) {
  VLOG(2) << "PepperFileQuerer::QueryImpl: " << result;
  file_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  file_io_.reset(new pp::FileIO(instance_));
  const int32_t ret = file_io_->Open(
      *file_ref_, PP_FILEOPENFLAG_READ,
      cc_factory_.NewCallback(&PepperFileQuerer::OnFileOpen));
  VLOG(2) << "file_io_->Open ret: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    LOG(ERROR) << "file_io_->Open error. ret: " << ret
               << " [" << filename_ << "]";
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperFileQuerer::OnFileOpen(int32_t result) {
  VLOG(2) << "PepperFileQuerer::OnFileOpen: " << result;
  if (result != PP_OK) {
    LOG(ERROR) << "PepperFileQuerer::OnFileOpen error. ret: " << result
               << " [" << filename_ << "]";
    result_ = result;
    event_.Notify();
    return;
  }
  CHECK(file_io_.get());
  const int32_t ret = file_io_->Query(&file_info_,
      cc_factory_.NewCallback(&PepperFileQuerer::OnQuery));
  VLOG(2) << "file_io_->Query: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    LOG(ERROR) << "file_io_->Query error. ret: " << ret
               << " [" << filename_ << "]";
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperFileQuerer::OnQuery(int32_t result) {
  VLOG(2) << "PepperFileQuerer::OnQuery: " << result;
  result_ = result;
  event_.Notify();
  return;
}

class PepperFileRenamer : public PepperFileOperator {
 public:
  PepperFileRenamer(pp::Instance *instance, pp::FileSystem *file_system);
  virtual ~PepperFileRenamer() {}
  int32_t Rename(const string &filename, const string &new_filename);

 private:
  // Called in Rename()
  void RenameImpl(int32_t result);
  void OnRename(int32_t result);

  string filename_;
  string new_filename_;
  scoped_main_thread_destructed_object<pp::FileRef> file_ref_;
  scoped_main_thread_destructed_object<pp::FileRef> new_file_ref_;
  pp::CompletionCallbackFactory<PepperFileRenamer> cc_factory_;
  DISALLOW_COPY_AND_ASSIGN(PepperFileRenamer);
};

PepperFileRenamer::PepperFileRenamer(pp::Instance *instance,
                                     pp::FileSystem *file_system)
    : PepperFileOperator(instance, file_system) {
  cc_factory_.Initialize(this);
}

int32_t PepperFileRenamer::Rename(const string &filename,
                                  const string &new_filename) {
  VLOG(2) << "PepperFileRenamer::Rename from \"" << filename << "\""
          << " to \"" << new_filename << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread())
      << "PepperFileRenamer::Rename() can't be called in the main thread.";
  filename_ = filename;
  new_filename_ = new_filename;
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      cc_factory_.NewCallback(&PepperFileRenamer::RenameImpl));
  event_.Wait(-1);
  return result_;
}

void PepperFileRenamer::RenameImpl(int32_t result) {
  VLOG(2) << "PepperFileRenamer::RenameImpl: " << result;
  file_ref_.reset(new pp::FileRef(*file_system_, filename_.c_str()));
  new_file_ref_.reset(new pp::FileRef(*file_system_, new_filename_.c_str()));
  const int32_t ret = file_ref_->Rename(
      *new_file_ref_,
      cc_factory_.NewCallback(&PepperFileRenamer::OnRename));
  VLOG(2) << "file_ref_->Rename ret: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperFileRenamer::OnRename(int32_t result) {
  VLOG(2) << "PepperFileRenamer::OnRename: " << result;
  result_ = result;
  event_.Notify();
  return;
}

class PepperDeleter : public PepperFileOperator {
 public:
  PepperDeleter(pp::Instance *instance, pp::FileSystem *file_system);
  virtual ~PepperDeleter() {}
  int32_t Delete(const string &path);

 private:
  // Called in Delete()
  void DeleteImpl(int32_t result);
  void OnDelete(int32_t result);

  string path_;
  scoped_main_thread_destructed_object<pp::FileRef> file_ref_;
  pp::CompletionCallbackFactory<PepperDeleter> cc_factory_;
  DISALLOW_COPY_AND_ASSIGN(PepperDeleter);
};

PepperDeleter::PepperDeleter(pp::Instance *instance,
                             pp::FileSystem *file_system)
    : PepperFileOperator(instance, file_system) {
  cc_factory_.Initialize(this);
}

int32_t PepperDeleter::Delete(const string &path) {
  VLOG(2) << "PepperDeleter::Delete \"" << path << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread())
      << "PepperDeleter::Delete() can't be called in the main thread.";
  path_ = path;
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      cc_factory_.NewCallback(&PepperDeleter::DeleteImpl));
  event_.Wait(-1);
  return result_;
}

void PepperDeleter::DeleteImpl(int32_t result) {
  VLOG(2) << "PepperDeleter::DeleteImpl: " << result;
  file_ref_.reset(new pp::FileRef(*file_system_, path_.c_str()));
  const int32_t ret = file_ref_->Delete(
      cc_factory_.NewCallback(&PepperDeleter::OnDelete));
  VLOG(2) << "file_ref_->Delete ret: " << ret;
  if ((ret != PP_OK_COMPLETIONPENDING) && (ret != PP_OK)) {
    result_ = ret;
    event_.Notify();
    return;
  }
}

void PepperDeleter::OnDelete(int32_t result) {
  VLOG(2) << "PepperDeleter::OnDelete: " << result;
  result_ = result;
  event_.Notify();
  return;
}

class PepperFileSystem : public PepperFileSystemInterface {
 public:
  PepperFileSystem();
  virtual ~PepperFileSystem();
  virtual bool Open(pp::Instance *instance, int64 expected_size);
  virtual bool FileExists(const string &filename);
  virtual bool DirectoryExists(const string &dirname);
  virtual bool ReadBinaryFile(const string &filename, string *buffer);
  virtual bool WriteBinaryFile(const string &filename, const string &buffer);
  virtual bool CreateDirectory(const string &dirname);
  virtual bool Delete(const string &filename);
  virtual bool Rename(const string &from, const string &to);
  virtual bool RegisterMmap(MmapSyncInterface *mmap);
  virtual bool UnRegisterMmap(MmapSyncInterface *mmap);
  virtual bool SyncMmapToFile();
  virtual bool Query(const string &path, PP_FileInfo *file_info);

 private:
  // Called in Open()
  void OpenImpl(int32_t result, int64_t expected_size, int32_t *ret_result);
  void OnOpen(int32_t result, int32_t *ret_result);

  scoped_main_thread_destructed_object<pp::FileSystem> file_system_;
  pp::CompletionCallbackFactory<PepperFileSystem> cc_factory_;
  UnnamedEvent event_;
  pp::Instance *instance_;
  std::set<MmapSyncInterface*> mmap_set_;
  Mutex mutex_;
  DISALLOW_COPY_AND_ASSIGN(PepperFileSystem);
};

PepperFileSystem::PepperFileSystem() {
  VLOG(2) << "PepperFileSystem::PepperFileSystem";
  cc_factory_.Initialize(this);
}

PepperFileSystem::~PepperFileSystem() {
  VLOG(2) << "PepperFileSystem::~PepperFileSystem";
}

bool PepperFileSystem::Query(const string &filename, PP_FileInfo *file_info) {
  VLOG(2) << "PepperFileSystem::Query \"" << filename << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  CHECK(file_system_.get()) << "PepperFileSystem is not initialized yet";
  PepperFileQuerer querer(instance_, file_system_.get());
  return querer.Query(filename, file_info) == PP_OK;
}

bool PepperFileSystem::Open(pp::Instance *instance, int64 expected_size) {
  VLOG(2) << "PepperFileSystem::Open";
  CHECK(!pp::Module::Get()->core()->IsMainThread())
      << "PepperFileSystem::Open() can't be called in the main thread.";
  CHECK(instance);
  instance_ = instance;
  int32_t ret_result = PP_ERROR_FAILED;
  pp::Module::Get()->core()->CallOnMainThread(
      0,
      cc_factory_.NewCallback(&PepperFileSystem::OpenImpl,
                              expected_size,
                              &ret_result));
  event_.Wait(-1);
  return ret_result == PP_OK;
}

void PepperFileSystem::OpenImpl(int32_t result,
                                int64_t expected_size,
                                int32_t *ret_result) {
  VLOG(2) << "PepperFileSystem::OpenImpl";
  file_system_.reset(
      new pp::FileSystem(instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT));
  const int32_t open_result = file_system_->Open(
      expected_size,
      cc_factory_.NewCallback(&PepperFileSystem::OnOpen, ret_result));
  VLOG(2) << "file_system_->Open ret:" << open_result;
  if ((open_result != PP_OK_COMPLETIONPENDING) && (open_result != PP_OK)) {
    *ret_result = open_result;
    event_.Notify();
  }
}

void PepperFileSystem::OnOpen(int32_t result, int32_t *ret_result) {
  VLOG(2) << "PepperFileSystem::OnOpen: " << result;
  CHECK(ret_result);
  *ret_result = result;
  event_.Notify();
}

bool PepperFileSystem::FileExists(const string &filename) {
  VLOG(2) << "PepperFileSystem::FileExists \"" << filename << "\"";
  PP_FileInfo info;
  return Query(filename, &info);
}

bool PepperFileSystem::DirectoryExists(const string &dirname) {
  VLOG(2) << "PepperFileSystem::DirectoryExists \"" << dirname << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  CHECK(file_system_.get()) << "PepperFileSystem is not initialized yet";
  PP_FileInfo info;
  PepperFileQuerer querer(instance_, file_system_.get());
  const int32_t ret = querer.Query(dirname, &info);
  if (ret != PP_OK) {
    return false;
  }
  return info.type == PP_FILETYPE_DIRECTORY;
}

bool PepperFileSystem::ReadBinaryFile(const string &filename, string *buffer) {
  VLOG(2) << "PepperFileSystem::ReadBinaryFile \"" << filename << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  CHECK(file_system_.get()) << "PepperFileSystem is not initialized yet";
  CHECK(buffer);
  PepperFileReader reader(instance_, file_system_.get());
  return reader.Read(filename, buffer) == PP_OK;
}

bool PepperFileSystem::WriteBinaryFile(const string &filename,
                                       const string &buffer) {
  VLOG(2) << "PepperFileSystem::WriteBinaryFile \"" << filename << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  CHECK(file_system_.get()) << "PepperFileSystem is not initialized yet";
  PepperFileWriter writer(instance_, file_system_.get());
  return writer.Write(filename, buffer) == PP_OK;
}

bool PepperFileSystem::CreateDirectory(const string &dirname) {
  VLOG(2) << "PepperFileSystem::CreateDirectory \"" << dirname << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  CHECK(file_system_.get()) << "PepperFileSystem is not initialized yet";
  PepperDirectoryCreator creator(instance_, file_system_.get());
  return creator.CreateDirectory(dirname) == PP_OK;
}

bool PepperFileSystem::Delete(const string &path) {
  VLOG(2) << "PepperFileSystem::Delete \"" << path << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  CHECK(file_system_.get()) << "PepperFileSystem is not initialized yet";
  PepperDeleter deleter(instance_, file_system_.get());
  return deleter.Delete(path) == PP_OK;
}

bool PepperFileSystem::Rename(const string &from, const string &to) {
  VLOG(2) << "PepperFileSystem::Rename from \"" << from << "\""
          << " to \"" << to << "\"";
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  CHECK(file_system_.get()) << "PepperFileSystem is not initialized yet";
  PepperFileRenamer renamer(instance_, file_system_.get());
  return renamer.Rename(from, to) == PP_OK;
}

bool PepperFileSystem::RegisterMmap(MmapSyncInterface *mmap) {
  scoped_lock lock(&mutex_);
  return mmap_set_.insert(mmap).second;
}

bool PepperFileSystem::UnRegisterMmap(MmapSyncInterface *mmap) {
  scoped_lock lock(&mutex_);
  return mmap_set_.erase(mmap);
}

bool PepperFileSystem::SyncMmapToFile() {
  scoped_lock lock(&mutex_);
  for (std::set<MmapSyncInterface*>::iterator it = mmap_set_.begin();
       it != mmap_set_.end(); ++it) {
    (*it)->SyncToFile();
  }
  return true;
}

PepperFileSystemInterface *g_pepper_file_system = NULL;

PepperFileSystemInterface *GetPepperFileSystem() {
  if (g_pepper_file_system != NULL) {
    return g_pepper_file_system;
  } else {
    return Singleton<PepperFileSystem>::get();
  }
}
}  // namespace

void PepperFileUtil::SetPepperFileSystemInterfaceForTest(
      PepperFileSystemInterface *mock_interface) {
  g_pepper_file_system = mock_interface;
}

bool PepperFileUtil::Initialize(pp::Instance *instance, int64 expected_size) {
  const bool result = GetPepperFileSystem()->Open(instance, expected_size);
  if (!result) {
    VLOG(2) << "PepperFileSystem::Open error";
  }
  return result;
}

bool PepperFileUtil::FileExists(const string &filename) {
  return GetPepperFileSystem()->FileExists(filename);
}

bool PepperFileUtil::DirectoryExists(const string &dirname) {
  return GetPepperFileSystem()->DirectoryExists(dirname);
}

bool PepperFileUtil::ReadBinaryFile(const string &filename, string *buffer) {
  return GetPepperFileSystem()->ReadBinaryFile(filename, buffer);
}

bool PepperFileUtil::WriteBinaryFile(const string &filename,
                                     const string &buffer) {
  Delete(filename);
  return GetPepperFileSystem()->WriteBinaryFile(filename, buffer);
}

bool PepperFileUtil::CreateDirectory(const string &dirname) {
  return GetPepperFileSystem()->CreateDirectory(dirname);
}

bool PepperFileUtil::Delete(const string &filename) {
  return GetPepperFileSystem()->Delete(filename);
}

bool PepperFileUtil::Rename(const string &from, const string &to) {
  return GetPepperFileSystem()->Rename(from, to);
}

bool PepperFileUtil::RegisterMmap(MmapSyncInterface *mmap) {
  return GetPepperFileSystem()->RegisterMmap(mmap);
}

bool PepperFileUtil::UnRegisterMmap(MmapSyncInterface *mmap) {
  return GetPepperFileSystem()->UnRegisterMmap(mmap);
}

bool PepperFileUtil::SyncMmapToFile() {
  return GetPepperFileSystem()->SyncMmapToFile();
}

bool PepperFileUtil::Query(const string &path, PP_FileInfo *file_info) {
  return GetPepperFileSystem()->Query(path, file_info);
}

}  // namespace mozc

#endif  // OS_NACL
