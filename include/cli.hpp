#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sbc {

enum class Action {
  None,
  Compile,
  Help,
  Version,
  License,
  Info,
  Clean
};

struct Options {
  Action action = Action::None;

  std::wstring inputPath;
  std::wstring outputPath;      // -o / --output
  std::wstring outputName;      // -n / --name
  std::wstring outputDir;       // -d / --output-dir

  bool force = false;
  bool quiet = false;
  bool verbose = false;
  bool noConsole = false;
  bool console = false;         // explicit console (default)
  bool keepTemp = false;
  bool dryRun = false;
  // Pause after script when double-clicked (default: auto).
  // pauseMode: 0 = auto, 1 = always, 2 = never
  int pauseMode = 0;
};

struct ParseResult {
  bool ok = false;
  int exitCode = 0;
  Options options;
  std::string errorMessage;
};

ParseResult parseArguments(int argc, wchar_t** argv);
void printHelp();
void printVersion();
void printLicense();

} // namespace sbc
