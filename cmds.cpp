#include "cmds.h"
#include <Windows.h>
#include <codecvt>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>

using namespace cmds;

namespace {
int cmd_launch_suspended(const std::vector<std::string> &cmd, Context &ctx) {
  wchar_t app[4096];
  size_t appLen;
  STARTUPINFOW si{};
  PROCESS_INFORMATION pi{};
  BOOL status;
  si.cb = sizeof(si);
  mbstowcs_s(&appLen, app, cmd[1].c_str(), _TRUNCATE);
  status = CreateProcessW(app, nullptr, nullptr, nullptr, FALSE,
                          DEBUG_ONLY_THIS_PROCESS, nullptr, nullptr, &si, &pi);
  if (!status) {
    std::cerr << "Failed to launch process" << std::endl;
    return Error;
  }
  std::cout << "Process created and running" << std::endl;
  ctx.h_process = pi.hProcess;
  ctx.h_thread = pi.hThread;
  // Run until first exception at LdrDebuggerBreak
  DEBUG_EVENT dbg_ev;
  for (;;) {
    bool exit = false;
    WaitForDebugEvent(&dbg_ev, INFINITE);
    std::cout << dbg_ev.dwDebugEventCode << " PID=" << dbg_ev.dwProcessId
              << " TID=" << dbg_ev.dwThreadId << std::endl;
    if (dbg_ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
      // Suspend until manual resume
      SuspendThread(ctx.h_thread);
      exit = true;
      std::cout << "Process suspended again upon debug event" << std::endl;
    }
    ContinueDebugEvent(dbg_ev.dwProcessId, dbg_ev.dwThreadId, DBG_CONTINUE);
    if (exit)
      break;
  }
  // Detach the debugger
  DebugActiveProcessStop(pi.dwProcessId);
  return Ok;
}

bool is_absolute_path(const std::string &path) {
  if (path.length() >= 2 && path[1] == ':')
    return true;
  return false;
}

bool file_exists(const std::string &path) {
  std::ifstream f(path);
  return f.good();
}

// Returns the index of the last occurance of `c`
// If not found, returns the length of the string
size_t find_last(const std::string &path, char c) {
    size_t last = SIZE_MAX;
    for (size_t i = 0; i < path.length(); ++i) {
        if (path[i] == c) {
            last = i;
        }
    }
    if (last == SIZE_MAX) {
        last = path.length();
    }
    return last;
}

int cmd_inject(const std::vector<std::string> &cmd, Context &ctx) {
  if (!ctx.h_process) {
    std::cerr << "Injection cannot run without open process" << std::endl;
    return Error;
  }
  if (cmd.size() < 2) {
    std::cerr << "Inject takes one argument" << std::endl;
    std::cerr << "Found " << cmd.size() << std::endl;
    return Error;
  }

  // Try to locate the DLL before passing relative path straight in
  std::string dll_path;
  if (!is_absolute_path(cmd[1])) {
    do {
      char cwd_buffer[256];
      GetCurrentDirectoryA(sizeof(cwd_buffer), cwd_buffer);
      dll_path = std::string(cwd_buffer) + "\\" + cmd[1];
      if (file_exists(dll_path))
        break;

      GetModuleFileNameA(NULL, cwd_buffer, sizeof(cwd_buffer));
      size_t last_slash = find_last(cwd_buffer, '\\');
      cwd_buffer[last_slash] = '\0';
      dll_path = std::string(cwd_buffer) + "\\" + cmd[1];
      if (file_exists(dll_path))
        break;

      dll_path = cmd[1];
    } while (false);
  } else {
    dll_path = cmd[1];
  }

  size_t shell_buf_size;
  std::unique_ptr<char[]> shell_buf;
  {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring dll_wpath = converter.from_bytes(dll_path);
    shell_buf_size = (1 + dll_wpath.length()) * sizeof(wchar_t);
    shell_buf = std::make_unique<char[]>(shell_buf_size);
  }

  LPVOID rp_code = VirtualAllocEx(ctx.h_process, NULL, shell_buf_size,
                                  MEM_COMMIT, PAGE_READWRITE);
  if (!rp_code) {
    std::cerr << "Inject couldn't VirtualAllocEx on remote process"
              << std::endl;
    return Error;
  }
  BOOL success = WriteProcessMemory(ctx.h_process, rp_code, shell_buf.get(),
                                    shell_buf_size, NULL);
  if (!success) {
    std::cerr << "Inject couldn't WriteProcessMemory" << std::endl;
    return Error;
  }

  FARPROC rp_load =
      GetProcAddress(GetModuleHandle(TEXT("Kernel32")), TEXT("LoadLibraryW"));
  HANDLE injection_thread_handle =
      CreateRemoteThread(ctx.h_process, NULL, 0,
                         (LPTHREAD_START_ROUTINE)rp_load, rp_code, 0, NULL);

  if (injection_thread_handle == INVALID_HANDLE_VALUE) {
    return Error;
  }
  return Ok;
}

int cmd_resume(const std::vector<std::string> &cmd, Context &ctx) {
  if (!ctx.h_thread) {
    std::cerr << "No process has been launched" << std::endl;
    return Error;
  }
  DWORD status = ResumeThread(ctx.h_thread);
  std::cout << "Resuming, previous suspend count = " << status << std::endl;
  return Ok;
}
} // namespace

int cmds::dispatch_cmd(const std::vector<std::string> &cmd, Context &ctx) {
  if (cmd[0] == "launch_suspended") {
    return cmd_launch_suspended(cmd, ctx);
  } else if (cmd[0] == "resume") {
    return cmd_resume(cmd, ctx);
  } else if (cmd[0] == "inject") {
    return cmd_inject(cmd, ctx);
  } else {
    std::cerr << "Unrecognized command: " << cmd[0] << std::endl;
  }
  return Ok;
}
