#include "cmds.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

std::vector<std::string> split_line(const char *line) {
  std::vector<std::string> result;
  enum class State { White, Text, Quote };
  State state = State::White;
  std::string arg;
  for (; *line; line++) {
    char c = *line;
    switch (state) {
    case State::White:
      if (c == '"') {
        state = State::Quote;
      } else if (!isblank(c)) {
        arg.push_back(c);
        state = State::Text;
      }
      break;
    case State::Text:
      if (!isblank(c)) {
        arg.push_back(c);
      } else {
        result.push_back(arg);
        arg.clear();
        state = State::White;
      }
      break;
    case State::Quote:
      if (c == '"') {
        result.push_back(arg);
        arg.clear();
        state = State::White;
      } else {
        arg.push_back(c);
      }
      break;
    }
  }
  if (!arg.empty()) {
    result.push_back(arg);
  }
  return result;
}

int main(int argc, char **argv) {
  std::ifstream ifs("injector_script.txt");
  if (!ifs.is_open()) {
    std::cerr << "Failed to open injector_script.txt" << std::endl;
    return 1;
  }
  char line[4096];
  cmds::Context ctx = cmds::Context::cons_default();
  while (true) {
    ifs.getline(line, sizeof(line));
    if (ifs.eof()) {
      break;
    }
    if (ifs.fail()) {
      std::cerr << "Line is too long, bailing" << std::endl;
      return 1;
    }
    auto args = split_line(line);
    if (cmds::dispatch_cmd(args, ctx) != cmds::Ok) {
      break;
    }
  }
  return 0;
}