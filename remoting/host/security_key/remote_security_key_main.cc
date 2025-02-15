// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_main.h"

#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"
#include "remoting/host/security_key/remote_security_key_ipc_client.h"
#include "remoting/host/security_key/remote_security_key_message_handler.h"

namespace remoting {

int StartRemoteSecurityKey() {
#if defined(OS_WIN)
  // GetStdHandle() returns pseudo-handles for stdin and stdout even if
  // the hosting executable specifies "Windows" subsystem. However the returned
  // handles are invalid in that case unless standard input and output are
  // redirected to a pipe or file.
  base::File read_file(GetStdHandle(STD_INPUT_HANDLE));
  base::File write_file(GetStdHandle(STD_OUTPUT_HANDLE));

  // After the message handler starts, the remote security key message reader
  // will keep doing blocking read operations on the input named pipe.
  // If any other thread tries to perform any operation on STDIN, it will also
  // block because the input named pipe is synchronous (non-overlapped).
  // It is pretty common for a DLL to query the device info (GetFileType) of
  // the STD* handles at startup. So any LoadLibrary request can potentially
  // be blocked. To prevent that from happening we close STDIN and STDOUT
  // handles as soon as we retrieve the corresponding file handles.
  SetStdHandle(STD_INPUT_HANDLE, nullptr);
  SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
#elif defined(OS_POSIX)
  // The files are automatically closed.
  base::File read_file(STDIN_FILENO);
  base::File write_file(STDOUT_FILENO);
#else
#error Not implemented.
#endif

  base::RunLoop run_loop;

  std::unique_ptr<RemoteSecurityKeyIpcClient> ipc_client(
      new RemoteSecurityKeyIpcClient());

  RemoteSecurityKeyMessageHandler message_handler;
  message_handler.Start(std::move(read_file), std::move(write_file),
                        std::move(ipc_client), run_loop.QuitClosure());

  run_loop.Run();

  return kSuccessExitCode;
}

int RemoteSecurityKeyMain(int argc, char** argv) {
  // This object instance is required by Chrome classes (such as MessageLoop).
  base::AtExitManager exit_manager;
  base::MessageLoopForUI message_loop;

  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

  return StartRemoteSecurityKey();
}

}  // namespace remoting
