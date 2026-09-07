#pragma once

#include "cli.hpp"

#include <string>

namespace sbc {

struct CompileResult {
  bool ok = false;
  int exitCode = 0;
  std::wstring outputPath;
  std::string message;
};

// Resolve final output path from options + input.
std::wstring resolveOutputPath(const Options& opt);

CompileResult compileBatch(const Options& opt);
int showInfo(const Options& opt);
int cleanTempFiles(const Options& opt);

} // namespace sbc
