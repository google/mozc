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

#ifdef OS_NACL

#include "base/pepper_file_system_mock.h"

#include <algorithm>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap_sync_interface.h"
#include "base/mutex.h"
#include "base/util.h"

using std::unique_ptr;
using mozc::internal::MockFileNode;

namespace mozc {
namespace {
const char *kFileDelimiter = "/";
}  // namespace

namespace internal {

MockFileNode::MockFileNode() : is_directory_(false) {}

MockFileNode::MockFileNode(MockFileNode *parent_node, const string &name,
                           bool is_directory)
    : parent_node_(parent_node), name_(name), is_directory_(is_directory) {}

MockFileNode::~MockFileNode() {}

bool MockFileNode::FileExists(const string &filename) const {
  return child_nodes_.find(filename) != child_nodes_.end();
}

bool MockFileNode::DirectoryExists(const string &dirname) const {
  auto iter = child_nodes_.find(dirname);
  return iter != child_nodes_.end() && iter->second->is_directory_;
}

bool MockFileNode::IsDirectory() const {
  return is_directory_;
}

bool MockFileNode::GetFileContent(string *buffer) const {
  if (is_directory_) {
    return false;
  }
  *buffer = content_;
  return true;
}

bool MockFileNode::SetFile(const string &filename, const string &content) {
  if (!is_directory_ || DirectoryExists(filename)) {
    return false;
  }
  MockFileNode *node = new MockFileNode(this, filename, false);
  node->content_ = content;
  child_nodes_[filename].reset(node);
  return true;
}

bool MockFileNode::AddDirectory(const string &dirname) {
  if (!is_directory_ || FileExists(dirname)) {
    return false;
  }
  child_nodes_[dirname] =
      unique_ptr<MockFileNode>(new MockFileNode(this, dirname, true));
  return true;
}

bool MockFileNode::Rename(const string &filename, MockFileNode *parent_node) {
  if (!parent_node || !parent_node->is_directory_) {
    LOG(ERROR) << "Invalid parent node.";
    return false;
  }
  if (filename == name_ && parent_node == parent_node_) {
    return true;
  }
  if (parent_node->DirectoryExists(filename) ||
      (parent_node->FileExists(filename) && is_directory_)) {
    LOG(ERROR) << "Cannot overwrite the destination file or directory.";
    return false;
  }

  auto iter = parent_node_->child_nodes_.find(name_);
  CHECK_EQ(iter->second.get(), this);
  parent_node->child_nodes_[filename].reset(iter->second.release());
  parent_node_->child_nodes_.erase(iter);
  name_ = filename;
  parent_node_ = parent_node;
  return true;
}

bool MockFileNode::Delete() {
  if (!parent_node_) {
    LOG(ERROR) << "Cannot delete a root directory.";
    return false;
  }
  return parent_node_->child_nodes_.erase(name_) > 0;
}

bool MockFileNode::Query(PP_FileInfo *file_info) const {
  file_info->size = content_.size();
  file_info->type = is_directory_ ? PP_FILETYPE_DIRECTORY : PP_FILETYPE_REGULAR;
  file_info->system_type = PP_FILESYSTEMTYPE_ISOLATED;
  // Fill dummy value for time stamp.
  file_info->creation_time = content_.size() + 1;
  file_info->last_access_time = content_.size() + 1;
  file_info->last_modified_time = content_.size() + 1;
  return true;
}

MockFileNode *MockFileNode::GetNode(const string &path) {
  if (path.empty() || path == kFileDelimiter) {
    return this;
  }
  MockFileNode *current_node = this;
  for (SplitIterator<SingleDelimiter> iter(path, kFileDelimiter);
       !iter.Done(); iter.Next()) {
    const string &name = iter.Get().as_string();
    auto child_node_iter = current_node->child_nodes_.find(name);
    if (child_node_iter == child_nodes_.end()) {
      return nullptr;
    }
    current_node = child_node_iter->second.get();
  }
  return current_node;
}

string MockFileNode::DebugMessage() const {
  string message = "\n";

  string path;
  {
    std::vector<string> nodes;
    const MockFileNode *node = this;
    while (node != nullptr) {
      nodes.push_back(node->name_);
      node = node->parent_node_;
    }
    reverse(nodes.begin(), nodes.end());
    Util::JoinStrings(nodes, "/", &path);
  }

  std::vector<const MockFileNode*> sub_directories;
  if (is_directory_) {
    message += Util::StringPrintf("directory: %s\n", path.c_str());
    for (const auto &elem : child_nodes_) {
      const MockFileNode *child_node = elem.second.get();
      const char node_type = child_node->is_directory_ ? 'D' : 'F';
      message += Util::StringPrintf(
          "  %c %s\n", node_type, child_node->name_.c_str());
      sub_directories.push_back(child_node);
    }
  } else {
    message += Util::StringPrintf("file: %s\n", path.c_str());
    message += Util::StringPrintf(
        "  size: %d\n", static_cast<int>(content_.size()));
    message += "  content: [" + content_ + "]\n";
  }
  if (!sub_directories.empty()) {
    for (const MockFileNode *node : sub_directories) {
      message += node->DebugMessage();
    }
  }
  return message;
}

}  // namespace internal

PepperFileSystemMock::PepperFileSystemMock()
    : root_directory_(nullptr, "", true) {}

PepperFileSystemMock::~PepperFileSystemMock() {}

bool PepperFileSystemMock::Open(pp::Instance *instance, int64 expected_size) {
  scoped_lock l(&mutex_);
  return true;
}

bool PepperFileSystemMock::FileExists(const string &path) {
  scoped_lock l(&mutex_);
  MockFileNode *node = root_directory_.GetNode(FileUtil::Dirname(path));
  return node && node->FileExists(FileUtil::Basename(path));
}

bool PepperFileSystemMock::DirectoryExists(const string &path) {
  scoped_lock l(&mutex_);
  MockFileNode *node = root_directory_.GetNode(FileUtil::Dirname(path));
  const string &basename = FileUtil::Basename(path);
  return node && (basename.empty() || node->DirectoryExists(basename));
}

bool PepperFileSystemMock::ReadBinaryFile(const string &path, string *buffer) {
  scoped_lock l(&mutex_);
  MockFileNode *node = root_directory_.GetNode(path);
  return node && node->GetFileContent(buffer);
}

bool PepperFileSystemMock::WriteBinaryFile(const string &path,
                                           const string &buffer) {
  scoped_lock l(&mutex_);
  const string &filename = FileUtil::Basename(path);
  MockFileNode *node = root_directory_.GetNode(FileUtil::Dirname(path));
  bool result = node && node->SetFile(filename, buffer);
  return result;
}

bool PepperFileSystemMock::CreateDirectory(const string &path) {
  scoped_lock l(&mutex_);
  MockFileNode *node = root_directory_.GetNode(FileUtil::Dirname(path));
  return node && node->AddDirectory(FileUtil::Basename(path));
}

bool PepperFileSystemMock::Delete(const string &path) {
  scoped_lock l(&mutex_);
  MockFileNode *node = root_directory_.GetNode(path);
  return node && node->Delete();
}

bool PepperFileSystemMock::Rename(const string &from, const string &to) {
  scoped_lock l(&mutex_);
  MockFileNode *from_file = root_directory_.GetNode(from);
  MockFileNode *to_directory = root_directory_.GetNode(FileUtil::Dirname(to));
  return from_file && from_file->Rename(FileUtil::Basename(to), to_directory);
}

bool PepperFileSystemMock::RegisterMmap(MmapSyncInterface *mmap) {
  scoped_lock l(&mutex_);
  return mmap_set_.insert(mmap).second;
}

bool PepperFileSystemMock::UnRegisterMmap(MmapSyncInterface *mmap) {
  scoped_lock l(&mutex_);
  return mmap_set_.erase(mmap);
}

bool PepperFileSystemMock::SyncMmapToFile() {
  scoped_lock l(&mutex_);
  for (std::set<MmapSyncInterface*>::iterator it = mmap_set_.begin();
       it != mmap_set_.end(); ++it) {
    (*it)->SyncToFile();
  }
  return true;
}

bool PepperFileSystemMock::Query(const string &path, PP_FileInfo *file_info) {
  scoped_lock l(&mutex_);
  MockFileNode *node = root_directory_.GetNode(path);
  return node && node->Query(file_info);
}

}  // namespace mozc

#endif  // OS_NACL
