// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_SYNC_CONFIG_ADAPTER_H_
#define MOZC_SYNC_CONFIG_ADAPTER_H_

#include <string>

#include "config/config.pb.h"
#include "sync/adapter_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace sync {
class ConfigAdapterTest;

class ConfigAdapter : public AdapterInterface {
 public:
  ConfigAdapter();
  virtual ~ConfigAdapter();
  virtual bool SetDownloadedItems(const ime_sync::SyncItems &items);
  virtual bool GetItemsToUpload(ime_sync::SyncItems *items);
  virtual bool MarkUploaded(
      const ime_sync::SyncItem &item, bool uploaded);
  virtual bool Clear();
  virtual ime_sync::Component component_id() const;

  // Set the base name for downloaded/uploaded config files.  This
  // should be used to store uploaded/downloaded data into a real
  // filesystem even if config itself is stored onmemory (memory://)
  // like ChromeOS.
  void SetConfigFileNameBase(const string &filename);

 private:
  // Calling SetConfigFileName() during SetUp()
  friend class ConfigAdapterTest;
  // Calling IsSameConfig() during the test
  FRIEND_TEST(ConfigAdapterTest, IsSameConfig);
  FRIEND_TEST(ConfigAdapterTest, IsSameConfigWithConfigHandler);
  // Test case for private methods for FileName()
  FRIEND_TEST(ConfigAdapterTest, FileNames);
  // Calling FileName() methods to verify the result in the following
  // test cases.
  FRIEND_TEST(ConfigAdapterTest, SetDownloadedItems);
  FRIEND_TEST(ConfigAdapterTest, SetDownloadedItemsConflict);
  FRIEND_TEST(ConfigAdapterTest, SetDownloadedItemsIgnorableFields);
  FRIEND_TEST(ConfigAdapterTest, InitialImports);
  FRIEND_TEST(ConfigAdapterTest, InitialImportsConflict);
  FRIEND_TEST(ConfigAdapterTest, GetItemsToUpload);
  FRIEND_TEST(ConfigAdapterTest, GetItemsToUploadIgnorableFields);
  FRIEND_TEST(ConfigAdapterTest, GetItemsToUploadFailure);
  FRIEND_TEST(ConfigAdapterTest, InitialUpload);
  FRIEND_TEST(ConfigAdapterTest, GetItemsToUploadDuringConflict);

  // Compares two specified configs and returns true if they are same
  // from the sync's point of view.
  bool IsSameConfig(const mozc::config::Config &config1,
                    const mozc::config::Config &config2);

  // Load the config from the file.  Returns true when succeeded.
  bool LoadConfigFromFile(
      const string &filename, mozc::config::Config *config);

  // Returns the base name for downloaded/uploaded config files.  Default
  // is the config file name.
  string GetConfigFileNameBase() const;

  // Returns the last downloaded config file name.
  string GetLastDownloadedConfigFileName() const;

  // Returns the last uploaded config file name.
  string GetLastUploadedConfigFileName() const;

  // Returns a file name to achieve atomic update of last
  // downloaded/uploaded config files.
  string GetTempConfigFileName() const;

  string config_filename_;
};

}  // namespace mozc::sync
}  // namespace mozc

#endif  // MOZC_SYNC_CONFIG_ADAPTER_H_
