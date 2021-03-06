#pragma once
#include <string>
#include <vector>

namespace cmds {
enum Result { Ok, Error };

struct Context {
  void *h_process;
  void *h_thread;

  static Context cons_default() { return {0}; }
};

int dispatch_cmd(const std::vector<std::string> &cmd, Context &ctx);
} // namespace cmds
