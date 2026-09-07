#include "cli.hpp"
#include "compiler.hpp"
#include "exit_codes.hpp"
#include "file_utils.hpp"
#include "version.hpp"

#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

void enableUtf8Console() {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
  enableUtf8Console();

  const sbc::ParseResult parsed = sbc::parseArguments(argc, argv);
  if (!parsed.ok) {
    std::wcerr << sbc::utf8ToWide(sbc::kProductName) << L" " << sbc::utf8ToWide(sbc::kVersionString)
               << L"\n\n[!] " << sbc::utf8ToWide(parsed.errorMessage) << L"\n";
    return parsed.exitCode != 0 ? parsed.exitCode : sbc::ExitCode::InvalidArguments;
  }

  const sbc::Options& opt = parsed.options;

  switch (opt.action) {
  case sbc::Action::Help:
    sbc::printHelp();
    return sbc::ExitCode::Success;
  case sbc::Action::Version:
    sbc::printVersion();
    return sbc::ExitCode::Success;
  case sbc::Action::License:
    sbc::printLicense();
    return sbc::ExitCode::Success;
  case sbc::Action::Info:
    return sbc::showInfo(opt);
  case sbc::Action::Clean:
    return sbc::cleanTempFiles(opt);
  case sbc::Action::Compile: {
    const sbc::CompileResult cr = sbc::compileBatch(opt);
    return cr.exitCode;
  }
  case sbc::Action::None:
  default:
    sbc::printHelp();
    return sbc::ExitCode::Success;
  }
}
