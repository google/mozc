// Copyright 2010-2013, Google Inc.
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

// The IPC implementation using core Mach APIs.
#ifdef OS_MACOSX

#include "ipc/ipc.h"

#include <map>
#include <vector>

#include <launch.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>

#include "base/logging.h"
#include "base/mac_util.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "ipc/ipc_path_manager.h"

namespace mozc {
namespace {
// Verify the path name resides under the appropriate directory.
bool VerifyBinary(const string &binary_path) {
  const string server_directory = MacUtil::GetServerDirectory();
  return binary_path.find(server_directory) == 0;
}

// The callback function to get all keys from LAUNCH_DATA_DICTIONARY.
void LaunchDataDictKeys(
    launch_data_t unued_data, const char *key, void *keys_vector) {
  vector<string> *keys = static_cast<vector<string> *>(keys_vector);
  DCHECK(keys);
  keys->push_back(string(key));
}

// The callback function to update the mapping.  This function is
// called from RefreshPortBinaryMap().  Each job_data holds the
// launchd data for a job (like a file in /Library/LaunchAgents), and
// key is the id (like com.google.inputmethod.Japanese.Converter).
void UpdateMap(launch_data_t job_data, const char *key, void *map_data) {
  if (launch_data_get_type(job_data) != LAUNCH_DATA_DICTIONARY) {
    LOG(ERROR) << "job data is not a dictionary";
    return;
  }

  launch_data_t program_name = launch_data_dict_lookup(
      job_data, LAUNCH_JOBKEY_PROGRAM);
  launch_data_t services = launch_data_dict_lookup(
      job_data, LAUNCH_JOBKEY_MACHSERVICES);
  if (program_name == NULL || services == NULL ||
      launch_data_get_type(program_name) != LAUNCH_DATA_STRING ||
      launch_data_get_type(services) != LAUNCH_DATA_DICTIONARY) {
    return;
  }

  map<string, vector<string> > *new_map =
      static_cast<map<string, vector<string> > *>(map_data);
  string program(launch_data_get_string(program_name));
  vector<string> service_names;
  const string port_name_prefix = MacUtil::GetLabelForSuffix("");
  launch_data_dict_iterate(services, LaunchDataDictKeys, &service_names);
  for (vector<string>::const_iterator it = service_names.begin();
       it != service_names.end(); ++it) {
    DLOG(INFO) << *it;
    if (it->find(port_name_prefix) == 0) {
      // Port name relevant to our task
      (*new_map)[*it].push_back(program);
    }
  }
}

const int kRefreshTimes = 1000;
map<string, vector<string> > *kPortBinaryMap = NULL;

void RefreshPortBinaryMap() {
  map<string, vector<string> > *new_map = new map<string, vector<string> >;
  // Get the all job data
  launch_data_t request = launch_data_new_string(LAUNCH_KEY_GETJOBS);
  launch_data_t jobs = launch_msg(request);
  launch_data_free(request);
  if (jobs == NULL) {
    LOG(ERROR) << "jobs is NULL";
    return;
  }
  if (launch_data_get_type(jobs) == LAUNCH_DATA_ERRNO) {
    LOG(ERROR) << "Cannot get jobs data";
    launch_data_free(jobs);
    return;
  }

  // Scan the all jobs
  launch_data_dict_iterate(jobs, UpdateMap, new_map);

  map<string, vector<string> > *old_map = kPortBinaryMap;
  kPortBinaryMap = new_map;
  delete old_map;
}

// Get the Mach port name for the specified "name".  Returns false
// something goes wrong during obtaining port name.
bool GetMachPortName(const string &name, string *port_name) {
  // refresh_counter is not guarded by any locking algorithm but it's
  // ok right now because it doesn't damage the processing itself.
  static int refresh_counter = 0;
  if (kPortBinaryMap == NULL || refresh_counter >= kRefreshTimes) {
    RefreshPortBinaryMap();
    refresh_counter = 0;
  } else {
    ++refresh_counter;
  }

  if (kPortBinaryMap == NULL) {
    return false;
  }

  for (map<string, vector<string> >::const_iterator it =
           kPortBinaryMap->begin(); it != kPortBinaryMap->end(); ++it) {
    if (it->first.rfind(name) == it->first.size() - name.size()) {
      // The port name has the suffix of the name
      if (it->second.size() != 1) {
        LOG(ERROR) << "Too many programs are waiting for the port";
        return false;
      }
      if (!VerifyBinary(it->second[0])) {
        LOG(ERROR) << "The server binary resides in a invalid directory: "
                   << it->second[0];
        return false;
      }

      port_name->assign(it->first);
      return true;
    }
  }

  LOG(ERROR) << "Port not found";
  return false;
}

// The default port manager for clients: using bootstrap_look_up.
// Please take care of calling this manager because bootstrap_look_up
// automatically starts the server processes.  We want to delay
// starting server as far as possible.
class DefaultClientMachPortManager : public MachPortManagerInterface {
 public:
  virtual bool GetMachPort(const string &name, mach_port_t *port) {
    string port_name;
    if (!GetMachPortName(name, &port_name)) {
      LOG(ERROR) << "Failed to get the port name";
      return false;
    }

    kern_return_t kr = bootstrap_look_up(
        bootstrap_port, const_cast<char *>(port_name.c_str()), port);

    return kr == BOOTSTRAP_SUCCESS;
  }

  virtual bool IsServerRunning(const string &name) const {
    string server_label = MacUtil::GetLabelForSuffix("");
    if (name == "session") {
      server_label += "Converter";
    } else if (name == "renderer") {
      server_label += "Renderer";
    } else {
      LOG(ERROR) << "Unknown server name: " << name;
      server_label.assign(MacUtil::GetLabelForSuffix(name));
    }

    launch_data_t request = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
    launch_data_dict_insert(request,
                            launch_data_new_string(server_label.c_str()),
                            LAUNCH_KEY_GETJOB);
    launch_data_t job = launch_msg(request);
    launch_data_free(request);
    if (job == NULL) {
      LOG(ERROR) << "Server job not found";
      return false;
    }
    if (launch_data_get_type(job) != LAUNCH_DATA_DICTIONARY) {
      LOG(ERROR) << "Something goes wrong with getting server information: "
                 << launch_data_get_type(job);
      launch_data_free(job);
      return false;
    }

    launch_data_t pid_data = launch_data_dict_lookup(job, LAUNCH_JOBKEY_PID);
    if (pid_data == NULL ||
        launch_data_get_type(pid_data) != LAUNCH_DATA_INTEGER) {
      // PID information is unavailable, which means server is not running.
      VLOG(2) << "Returned job is formatted wrongly: cannot find PID data.";
      launch_data_free(job);
      return false;
    }

    VLOG(2) << "Server is running with PID "
            << launch_data_get_integer(pid_data);
    launch_data_free(job);
    return true;
  }
};

// The default port manager for servers: using bootstrap_check_in.  It
// won't succeed if the port name is not registered by launchd or the
// process is not invoked by launchd.
class DefaultServerMachPortManager : public MachPortManagerInterface {
 public:
  ~DefaultServerMachPortManager() {
    for (map<string, mach_port_t>::const_iterator it = mach_ports_.begin();
         it != mach_ports_.end(); ++it) {
      mach_port_destroy(mach_task_self(), it->second);
    }
    mach_ports_.clear();
  }

  virtual bool GetMachPort(const string &name, mach_port_t *port) {
    string port_name;
    if (!GetMachPortName(name, &port_name)) {
      LOG(ERROR) << "Failed to get the port name";
      return false;
    }

    DLOG(INFO) << "\"" << port_name << "\"";

    map<string, mach_port_t>::const_iterator it = mach_ports_.find(port_name);
    if (it != mach_ports_.end()) {
      *port = it->second;
      return true;
    }

    kern_return_t kr = bootstrap_check_in(
        bootstrap_port, const_cast<char *>(port_name.c_str()), port);
    mach_ports_[port_name] = *port;
    return kr == BOOTSTRAP_SUCCESS;
  }

  // In the server side, it always return "true" because the caller
  // itself is the server.
  virtual bool IsServerRunning(const string &name) const {
    return true;
  }

 private:
  map<string, mach_port_t> mach_ports_;
};

struct mach_ipc_send_message {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_ool_descriptor_t data;
  mach_msg_type_number_t count;
};

struct mach_ipc_receive_message {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_ool_descriptor_t data;
  mach_msg_type_number_t count;
  mach_msg_trailer_t trailer;
};

}  // anonymous namespace

// Client implementation
IPCClient::IPCClient(const string &name)
    : name_(name), mach_port_manager_(NULL), ipc_path_manager_(NULL) {
  Init(name, "");
}

IPCClient::IPCClient(const string &name, const string &server_path)
    : name_(name), mach_port_manager_(NULL), ipc_path_manager_(NULL) {
  Init(name, server_path);
}

IPCClient::~IPCClient() {
  // Do nothing
}

void IPCClient::Init(const string &name, const string & /*server_path*/) {
  ipc_path_manager_ = IPCPathManager::GetIPCPathManager(name);
  if (!ipc_path_manager_->LoadPathName()) {
    LOG(ERROR) << "Cannot load IPC path name";
  }
}

bool IPCClient::Call(const char *request_,
                     size_t input_length,
                     char *response_,
                     size_t *response_size,
                     int32 timeout) {
  last_ipc_error_ = IPC_NO_ERROR;
  MachPortManagerInterface *manager = mach_port_manager_;
  if (manager == NULL) {
    manager = Singleton<DefaultClientMachPortManager>::get();
  }

  // Obtain the server port
  mach_port_t server_port;
  if (manager == NULL || !manager->GetMachPort(name_, &server_port)) {
    last_ipc_error_ = IPC_NO_CONNECTION;
    LOG(ERROR) << "Cannot connect to the server";
    return false;
  }

  // Creating sending port
  kern_return_t kr;
  mach_port_t client_port = MACH_PORT_NULL;
  kr = mach_port_allocate(
      mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &client_port);
  if (kr != KERN_SUCCESS) {
    last_ipc_error_ = IPC_WRITE_ERROR;
    LOG(ERROR) << "Cannot allocate the client port: " << kr;
    return false;
  }

  // Prepare the sending message with OOL.  Out-of-Line is a message
  // sending which doesn't copy the message data but share the memory
  // area between client and server.
  mach_ipc_send_message send_message;
  mach_msg_header_t *send_header = &(send_message.header);
  send_header->msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND) |
      MACH_MSGH_BITS_COMPLEX;  // To enable OOL message
  send_header->msgh_size = sizeof(send_message);
  send_header->msgh_remote_port = server_port;
  send_header->msgh_local_port = client_port;
  send_header->msgh_reserved = 0;
  send_header->msgh_id = IPC_PROTOCOL_VERSION;
  send_message.body.msgh_descriptor_count = 1;
  send_message.data.address = const_cast<char *>(request_);
  send_message.data.size = input_length;
  send_message.data.deallocate = false;  // Doesn't deallocate data immediately
  send_message.data.copy = MACH_MSG_VIRTUAL_COPY;  // Copy on write
  send_message.data.type = MACH_MSG_OOL_DESCRIPTOR;
  send_message.count = send_message.data.size;

  // Actually send the message
  kr = mach_msg(send_header,
                MACH_SEND_MSG | MACH_SEND_TIMEOUT,  // send with timeout
                send_header->msgh_size,  // send size
                0,  // receive size is 0 because sending
                MACH_PORT_NULL,  // receive port
                timeout,  // timeout in msec
                MACH_PORT_NULL);  // notoicication port in case of error
  if (kr == MACH_SEND_TIMED_OUT) {
    LOG(ERROR) << "sending message timeout";
    last_ipc_error_ = IPC_TIMEOUT_ERROR;
    mach_port_destroy(mach_task_self(), client_port);
    return false;
  } else if (kr != MACH_MSG_SUCCESS) {
    LOG(ERROR) << "unknown error on sending request: " << kr;
    last_ipc_error_ = IPC_WRITE_ERROR;
    mach_port_destroy(mach_task_self(), client_port);
    return false;
  }

  // Receive server response
  mach_ipc_receive_message receive_message;
  mach_msg_header_t *receive_header;
  // try to receive multiple messages because more than one processes
  // send responses.
  const int trial = 2;
  for (int i = 0; i < trial; ++i) {
    receive_header = &(receive_message.header);
    receive_header->msgh_remote_port = server_port;
    receive_header->msgh_local_port = client_port;
    receive_header->msgh_size = sizeof(receive_message);
    kr = mach_msg(receive_header,
                  MACH_RCV_MSG | MACH_RCV_TIMEOUT,  // receive with timeout
                  0,  // send size is 0 because receiving
                  receive_header->msgh_size,  // receive size
                  client_port,  // receive port
                  timeout,  // timeout in msec
                  MACH_PORT_NULL);  // notification port in case of error
    if (kr == MACH_RCV_TIMED_OUT) {
      LOG(ERROR) << "receiving message timeout";
      last_ipc_error_ = IPC_TIMEOUT_ERROR;
      break;
    } else if (kr != MACH_MSG_SUCCESS) {
      LOG(ERROR) << "unknown error on receiving response: " << kr;
      last_ipc_error_ = IPC_READ_ERROR;
      // This can be a wrong message.  Try to receive again.
      continue;
    }

    if (receive_header->msgh_id == IPC_PROTOCOL_VERSION) {
      last_ipc_error_ = IPC_NO_ERROR;
      if (*response_size > receive_message.data.size) {
        *response_size = receive_message.data.size;
      }
      ::memcpy(response_, receive_message.data.address, *response_size);
      mach_port_destroy(mach_task_self(), client_port);
      vm_deallocate(mach_task_self(),
                    (vm_address_t)receive_message.data.address,
                    receive_message.data.size);
      return true;
    }
  }

  LOG(ERROR) << "Receiving message failed";
  if (last_ipc_error_ == IPC_NO_ERROR) {
    last_ipc_error_ = IPC_READ_ERROR;
  }
  mach_port_destroy(mach_task_self(), client_port);
  return false;
}

bool IPCClient::Connected() const {
  if (!ipc_path_manager_->LoadPathName()) {
    // No server files found: not running server or not initialized yet.
    return false;
  }

  MachPortManagerInterface *manager = mach_port_manager_;
  if (manager == NULL) {
    manager = Singleton<DefaultClientMachPortManager>::get();
  }

  return manager->IsServerRunning(name_);
}

// Server implementation
IPCServer::IPCServer(const string &name,
                     int32 num_connections,
                     int32 timeout)
    : name_(name), mach_port_manager_(NULL), timeout_(timeout) {
  // This is a fake IPC path manager: it just stores the server
  // version and IPC name but we don't use the stored IPC name itself.
  // It's just for compatibility.
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager(name);
  // The IPC server uses old path name at this time.
  // TODO(mukai): Switch to new path name when the new IPC client is
  // widely distributed.
  LOG(INFO) << "Server created";
  if (!manager->CreateNewPathName()) {
    LOG(ERROR) << "Cannot make IPC path name";
    return;
  }

  if (!manager->SavePathName()) {
    LOG(ERROR) << "Cannot save IPC path name";
  }
}

IPCServer::~IPCServer() {
  // Do nothing
}

bool IPCServer::Connected() const {
  MachPortManagerInterface *manager = mach_port_manager_;
  if (manager == NULL) {
    manager = Singleton<DefaultServerMachPortManager>::get();
  }

  return manager->IsServerRunning(name_);
}

void IPCServer::Loop() {
  MachPortManagerInterface *manager = mach_port_manager_;
  if (manager == NULL) {
    manager = Singleton<DefaultServerMachPortManager>::get();
  }

  // Obtain the server port
  mach_port_t server_port;
  if (manager == NULL || !manager->GetMachPort(name_, &server_port)) {
    LOG(ERROR) << "Failed to reserve the port.";
    return;
  }

  mach_ipc_send_message send_message;
  mach_ipc_receive_message receive_message;
  mach_msg_header_t *send_header, *receive_header;
  kern_return_t kr;
  bool finished = false;
  char response[IPC_RESPONSESIZE];
  while (!finished) {
    // Receive request
    receive_header = &(receive_message.header);
    receive_header->msgh_local_port = server_port;
    receive_header->msgh_size = sizeof(receive_message);
    kr = mach_msg(receive_header,
                  MACH_RCV_MSG,  // no timeout when receiving clients
                  0,  // send size is 0 because receiving
                  receive_header->msgh_size,  // receive size
                  server_port,  // receive port
                  MACH_MSG_TIMEOUT_NONE,  // no timeout
                  MACH_PORT_NULL);  // notification port in case of error

    if (kr != MACH_MSG_SUCCESS) {
      LOG(ERROR) << "Something around mach ports goes wrong: " << kr;
      break;
    }
    if (receive_header->msgh_id != IPC_PROTOCOL_VERSION) {
      LOG(ERROR) << "Invalid message";
      continue;
    }

    size_t response_size = IPC_RESPONSESIZE;
    if (!Process(static_cast<char *>(receive_message.data.address),
                 receive_message.data.size,
                 response, &response_size)) {
      LOG(INFO) << "Process() returns false.  Quit the wait loop.";
      finished = true;
    }

    vm_deallocate(mach_task_self(),
                  (vm_address_t)receive_message.data.address,
                  receive_message.data.size);
    // Send response
    send_header = &(send_message.header);
    send_header->msgh_bits = MACH_MSGH_BITS_LOCAL(receive_header->msgh_bits) |
      MACH_MSGH_BITS_COMPLEX;  // To enable OOL message
    send_header->msgh_size = sizeof(send_message);
    send_header->msgh_local_port = MACH_PORT_NULL;
    send_header->msgh_remote_port = receive_header->msgh_remote_port;
    send_header->msgh_id = receive_header->msgh_id;
    send_message.body.msgh_descriptor_count = 1;
    send_message.data.address = response;
    send_message.data.size = response_size;
    // Doesn't deallocate data immediately
    send_message.data.deallocate = false;
    send_message.data.copy = MACH_MSG_VIRTUAL_COPY;  // Copy on write
    send_message.data.type = MACH_MSG_OOL_DESCRIPTOR;
    send_message.count = send_message.data.size;

    kr = mach_msg(send_header,
                  MACH_SEND_MSG | MACH_SEND_TIMEOUT,  // send with timeout
                  send_header->msgh_size,  // send size
                  0,  // receive size is 0 because sending
                  MACH_PORT_NULL,  // receive port
                  timeout_,  // timeout
                  MACH_PORT_NULL);  // notification port in case of error
    if (kr != MACH_MSG_SUCCESS) {
      LOG(ERROR) << "Something around mach ports goes wrong: " << kr;
      continue;
    }
  }
}

void IPCServer::Terminate() {
  server_thread_->Terminate();
}

}  // namespace mozc

#endif  // OS_MACOSX
