#include "cli.hpp"
#include "exit_codes.hpp"
#include "file_utils.hpp"
#include "version.hpp"

#include <iostream>
#include <string>

namespace sbc {
namespace {

bool eq(const std::wstring& a, const wchar_t* b) {
  return a == b;
}

bool startsWith(const std::wstring& s, const wchar_t* pfx) {
  const std::size_t n = wcslen(pfx);
  return s.size() >= n && s.compare(0, n, pfx) == 0;
}

void setAction(Options& opt, Action a, std::string& err, bool& ok) {
  if (opt.action != Action::None && opt.action != a) {
    err = "Error: Conflicting commands specified.";
    ok = false;
    return;
  }
  opt.action = a;
}

} // namespace

void printVersion() {
  std::wcout << utf8ToWide(versionBanner()) << L"\n";
}

void printLicense() {
  std::wcout << utf8ToWide(licenseText());
}

void printHelp() {
  std::wcout
      << L"Simple BAT Converter " << utf8ToWide(kVersionString) << L"\n"
      << L"Author: " << utf8ToWide(kAuthor) << L"\n\n"
      << L"Usage:\n"
      << L"  sbc [command] [options] [arguments]\n\n"
      << L"Commands / options:\n"
      << L"  -c, --compile <file>       Compile a .bat or .cmd file into an .exe\n"
      << L"  -o, --output <file>        Output executable path\n"
      << L"  -n, --name <name>          Output executable base name\n"
      << L"  -d, --output-dir <dir>     Output directory\n"
      << L"  -i, --icon <file.ico>      Embed an icon into the generated executable\n"
      << L"  -f, --force                Overwrite an existing output file\n"
      << L"  -q, --quiet                Suppress non-essential output\n"
      << L"  -v, --verbose              Enable verbose diagnostics\n"
      << L"      --version              Show version information\n"
      << L"  -h, --help                 Show this help\n"
      << L"      --license              Show license text\n"
      << L"      --no-console           Build a GUI-subsystem executable (no console)\n"
      << L"      --console              Build a console executable (default)\n"
      << L"      --pause                Always pause after the script finishes\n"
      << L"      --no-pause             Never pause after the script finishes\n"
      << L"      --keep-temp            Keep the extracted script after running\n"
      << L"      --clean                Remove leftover sbc temporary directories\n"
      << L"      --dry-run              Show actions without writing the executable\n"
      << L"      --info                 Show information about the input file\n\n"
      << L"Notes:\n"
      << L"  By default, generated console EXEs pause when double-clicked from\n"
      << L"  Explorer so the window does not close immediately. They do not pause\n"
      << L"  when launched from an existing Command Prompt.\n\n"
      << L"Examples:\n"
      << L"  sbc -c example.bat\n"
      << L"  sbc -c example.bat -o example.exe\n"
      << L"  sbc --compile script.cmd --output myapp.exe\n"
      << L"  sbc -c script.bat -d build\n"
      << L"  sbc -c script.bat --icon app.ico\n"
      << L"  sbc -c script.bat -f\n"
      << L"  sbc -c script.bat --verbose\n"
      << L"  sbc -c script.bat --no-console\n"
      << L"  sbc -c script.bat --no-pause\n\n"
      << L"Exit codes:\n"
      << L"  0 success  1 general  2 bad args  3 not found  4 bad type\n"
      << L"  5 output   6 build  7 runtime\n";
}

ParseResult parseArguments(int argc, wchar_t** argv) {
  ParseResult result;
  result.ok = true;
  Options& opt = result.options;

  if (argc <= 1) {
    opt.action = Action::Help;
    result.ok = true;
    return result;
  }

  auto requireValue = [&](int& i, const wchar_t* name) -> std::wstring {
    if (i + 1 >= argc) {
      result.ok = false;
      result.exitCode = ExitCode::InvalidArguments;
      result.errorMessage = std::string("Error: Missing value for ") + wideToUtf8(name) + ".";
      return {};
    }
    ++i;
    return argv[i];
  };

  bool positionalOnly = false;

  for (int i = 1; i < argc && result.ok; ++i) {
    const std::wstring arg = argv[i];

    if (!positionalOnly && eq(arg, L"--")) {
      // Everything after "--" is treated as a positional argument.
      positionalOnly = true;
    } else if (!positionalOnly && (eq(arg, L"-h") || eq(arg, L"--help") || eq(arg, L"/?"))) {
      setAction(opt, Action::Help, result.errorMessage, result.ok);
    } else if (!positionalOnly && eq(arg, L"--version")) {
      setAction(opt, Action::Version, result.errorMessage, result.ok);
    } else if (!positionalOnly && eq(arg, L"--license")) {
      setAction(opt, Action::License, result.errorMessage, result.ok);
    } else if (!positionalOnly && eq(arg, L"--info")) {
      setAction(opt, Action::Info, result.errorMessage, result.ok);
    } else if (!positionalOnly && eq(arg, L"--clean")) {
      setAction(opt, Action::Clean, result.errorMessage, result.ok);
    } else if (!positionalOnly && (eq(arg, L"-c") || eq(arg, L"--compile"))) {
      setAction(opt, Action::Compile, result.errorMessage, result.ok);
      opt.inputPath = requireValue(i, arg.c_str());
    } else if (!positionalOnly && (eq(arg, L"-o") || eq(arg, L"--output"))) {
      opt.outputPath = requireValue(i, arg.c_str());
    } else if (!positionalOnly && (eq(arg, L"-n") || eq(arg, L"--name"))) {
      opt.outputName = requireValue(i, arg.c_str());
    } else if (!positionalOnly && (eq(arg, L"-d") || eq(arg, L"--output-dir"))) {
      opt.outputDir = requireValue(i, arg.c_str());
    } else if (!positionalOnly && (eq(arg, L"-i") || eq(arg, L"--icon"))) {
      opt.iconPath = requireValue(i, arg.c_str());
    } else if (!positionalOnly && (eq(arg, L"-f") || eq(arg, L"--force"))) {
      opt.force = true;
    } else if (!positionalOnly && (eq(arg, L"-q") || eq(arg, L"--quiet"))) {
      opt.quiet = true;
    } else if (!positionalOnly && (eq(arg, L"-v") || eq(arg, L"--verbose"))) {
      opt.verbose = true;
    } else if (!positionalOnly && eq(arg, L"--no-console")) {
      opt.noConsole = true;
    } else if (!positionalOnly && eq(arg, L"--console")) {
      opt.console = true;
      opt.noConsole = false;
    } else if (!positionalOnly && eq(arg, L"--pause")) {
      opt.pauseMode = 1;
    } else if (!positionalOnly && eq(arg, L"--no-pause")) {
      opt.pauseMode = 2;
    } else if (!positionalOnly && eq(arg, L"--keep-temp")) {
      opt.keepTemp = true;
    } else if (!positionalOnly && eq(arg, L"--dry-run")) {
      opt.dryRun = true;
    } else if (!positionalOnly && startsWith(arg, L"-")) {
      result.ok = false;
      result.exitCode = ExitCode::InvalidArguments;
      result.errorMessage = "Error: Unknown option: " + wideToUtf8(arg);
    } else {
      // Positional input if compile/info implied
      if (opt.inputPath.empty()) {
        opt.inputPath = arg;
        if (opt.action == Action::None) opt.action = Action::Compile;
      } else {
        result.ok = false;
        result.exitCode = ExitCode::InvalidArguments;
        result.errorMessage = "Error: Unexpected argument: " + wideToUtf8(arg);
      }
    }
  }

  if (!result.ok) return result;

  if (opt.action == Action::None) {
    if (!opt.inputPath.empty()) {
      opt.action = Action::Compile;
    } else {
      opt.action = Action::Help;
    }
  }

  if ((opt.action == Action::Compile || opt.action == Action::Info) &&
      opt.inputPath.empty()) {
    result.ok = false;
    result.exitCode = ExitCode::InvalidArguments;
    result.errorMessage = "Error: No input file specified.";
    return result;
  }

  if (opt.quiet && opt.verbose) {
    opt.verbose = false; // quiet wins
  }

  return result;
}

} // namespace sbc
