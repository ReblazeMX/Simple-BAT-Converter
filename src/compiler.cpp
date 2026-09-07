#include "compiler.hpp"
#include "exit_codes.hpp"
#include "file_utils.hpp"
#include "payload.hpp"
#include "version.hpp"

#include "stub_console.hpp"
#include "stub_noconsole.hpp"

#include <iostream>
#include <vector>

namespace sbc {
namespace {

void info(const Options& opt, const std::wstring& msg) {
  if (!opt.quiet) std::wcout << msg << L"\n";
}

void verbose(const Options& opt, const std::wstring& msg) {
  if (opt.verbose && !opt.quiet) std::wcout << msg << L"\n";
}

void errorLine(const Options& opt, const std::string& msg) {
  if (!opt.quiet) {
    std::wcerr << utf8ToWide(kProductName) << L" " << utf8ToWide(kVersionString) << L"\n\n";
    std::wcerr << L"[!] " << utf8ToWide(msg) << L"\n";
  } else {
    std::wcerr << utf8ToWide(msg) << L"\n";
  }
}

std::vector<std::uint8_t> buildPayload(const std::vector<std::uint8_t>& script,
                                       const std::string& originalNameUtf8,
                                       std::uint32_t flags) {
  // [script][name utf8][trailer]
  std::vector<std::uint8_t> out;
  out.reserve(script.size() + originalNameUtf8.size() + sizeof(PayloadTrailer));
  out.insert(out.end(), script.begin(), script.end());
  out.insert(out.end(),
             reinterpret_cast<const std::uint8_t*>(originalNameUtf8.data()),
             reinterpret_cast<const std::uint8_t*>(originalNameUtf8.data()) +
                 originalNameUtf8.size());

  PayloadTrailer tr{};
  tr.nameLen = static_cast<std::uint16_t>(originalNameUtf8.size());
  tr.flags = flags;
  tr.scriptSize = static_cast<std::uint64_t>(script.size());
  tr.magic[0] = kPayloadMagic[0];
  tr.magic[1] = kPayloadMagic[1];
  tr.magic[2] = kPayloadMagic[2];
  tr.magic[3] = kPayloadMagic[3];

  const auto* p = reinterpret_cast<const std::uint8_t*>(&tr);
  out.insert(out.end(), p, p + sizeof(tr));
  return out;
}

} // namespace

std::wstring resolveOutputPath(const Options& opt) {
  // Priority: -o exact path > (-d and/or -n) > default next to CWD from input stem
  if (!opt.outputPath.empty()) {
    std::wstring out = opt.outputPath;
    const auto ext = fileExtension(out);
    if (ext.empty()) out += L".exe";
    if (!opt.outputDir.empty() && parentPath(out) == L".") {
      out = joinPath(opt.outputDir, fileName(out));
    }
    return absolutePath(out);
  }

  std::wstring name = opt.outputName.empty() ? fileStem(opt.inputPath) : opt.outputName;
  // Strip accidental .exe from -n
  if (fileExtension(name) == L".exe") name = fileStem(name);
  name += L".exe";

  if (!opt.outputDir.empty()) {
    return absolutePath(joinPath(opt.outputDir, name));
  }
  return absolutePath(name);
}

CompileResult compileBatch(const Options& opt) {
  CompileResult result;

  const std::wstring input = absolutePath(opt.inputPath);

  if (!isBatOrCmd(input)) {
    result.exitCode = ExitCode::UnsupportedFileType;
    result.message = "Error: Unsupported input file type.\nOnly .bat and .cmd files are supported.";
    errorLine(opt, result.message);
    return result;
  }

  if (!fileExists(input)) {
    result.exitCode = ExitCode::InputNotFound;
    result.message = "Error: Input file does not exist:\n" + wideToUtf8(opt.inputPath);
    errorLine(opt, result.message);
    return result;
  }

  const std::wstring output = resolveOutputPath(opt);
  result.outputPath = output;

  if (fileExists(output) && !opt.force) {
    result.exitCode = ExitCode::OutputError;
    result.message =
        "Error: Output file already exists.\nUse -f or --force to overwrite it.";
    errorLine(opt, result.message);
    return result;
  }

  if (!opt.quiet) {
    std::wcout << utf8ToWide(kProductName) << L" " << utf8ToWide(kVersionString) << L"\n";
    std::wcout << L"Author: " << utf8ToWide(kAuthor) << L"\n\n";
  }

  const std::wstring outDir = parentPath(output);
  if (!directoryExists(outDir)) {
    if (opt.dryRun) {
      verbose(opt, L"[dry-run] Would create directory: " + outDir);
    } else if (!createDirectories(outDir)) {
      result.exitCode = ExitCode::OutputError;
      result.message = "Error: Failed to create output directory.";
      errorLine(opt, result.message);
      return result;
    }
  }

  info(opt, L"[+] Input:  " + input);
  info(opt, L"[+] Output: " + output);

  std::vector<std::uint8_t> script;
  try {
    script = readFileBytes(input);
  } catch (...) {
    result.exitCode = ExitCode::GeneralError;
    result.message = "Error: Failed to read input file.";
    errorLine(opt, result.message);
    return result;
  }

  verbose(opt, L"[*] Script size: " + std::to_wstring(script.size()) + L" bytes");

  const bool useNoConsole = opt.noConsole;
  const std::uint8_t* stub = useNoConsole ? kStubNoConsole : kStubConsole;
  const std::size_t stubSize = useNoConsole ? kStubNoConsoleSize : kStubConsoleSize;

  info(opt, L"[+] Embedding script...");
  verbose(opt, std::wstring(L"[*] Stub: ") + (useNoConsole ? L"no-console" : L"console") +
                   L" (" + std::to_wstring(stubSize) + L" bytes)");

  std::uint32_t flags = 0;
  if (useNoConsole) flags |= kFlagNoConsole;
  if (opt.pauseMode == 1) flags |= kFlagAlwaysPause;
  if (opt.pauseMode == 2) flags |= kFlagNoPause;

  // Prefer original filename so %~nx0-style usage is closer to real batch
  const std::string origName = wideToUtf8(fileName(input));
  if (origName.size() > 0xFFFF) {
    result.exitCode = ExitCode::GeneralError;
    result.message = "Error: Input file name is too long.";
    errorLine(opt, result.message);
    return result;
  }

  auto payload = buildPayload(script, origName, flags);

  std::vector<std::uint8_t> exe;
  exe.reserve(stubSize + payload.size());
  exe.insert(exe.end(), stub, stub + stubSize);
  exe.insert(exe.end(), payload.begin(), payload.end());

  info(opt, L"[+] Building executable...");
  verbose(opt, L"[*] Total size: " + std::to_wstring(exe.size()) + L" bytes");

  if (opt.dryRun) {
    info(opt, L"[+] Dry run — no file written.");
    info(opt, L"[+] Done!");
    result.ok = true;
    result.exitCode = ExitCode::Success;
    result.message = "Dry run completed.";
    return result;
  }

  if (!writeFileBytes(output, exe)) {
    result.exitCode = ExitCode::BuildError;
    result.message = "Error: Failed to build executable.";
    errorLine(opt, result.message);
    return result;
  }

  info(opt, L"[+] Done!\n");
  info(opt, L"Successfully created:\n" + output);

  result.ok = true;
  result.exitCode = ExitCode::Success;
  result.message = "OK";
  return result;
}

int showInfo(const Options& opt) {
  if (!opt.quiet) {
    std::wcout << utf8ToWide(kProductName) << L" " << utf8ToWide(kVersionString) << L"\n\n";
  }

  const std::wstring input = absolutePath(opt.inputPath);
  if (!isBatOrCmd(input)) {
    errorLine(opt, "Error: Unsupported file extension.\nExpected .bat or .cmd.");
    return ExitCode::UnsupportedFileType;
  }
  if (!fileExists(input)) {
    errorLine(opt, "Error: Input file does not exist:\n" + wideToUtf8(opt.inputPath));
    return ExitCode::InputNotFound;
  }

  const auto size = fileSize(input);
  const auto out = resolveOutputPath(opt);

  std::wcout << L"Input path:    " << input << L"\n";
  std::wcout << L"File name:     " << fileName(input) << L"\n";
  std::wcout << L"Extension:     " << fileExtension(input) << L"\n";
  std::wcout << L"Size:          " << size << L" bytes\n";
  std::wcout << L"Default out:   " << out << L"\n";
  std::wcout << L"Subsystem:     " << (opt.noConsole ? L"WINDOWS (no console)" : L"CONSOLE")
             << L"\n";
  return ExitCode::Success;
}

int cleanTempFiles(const Options& opt) {
  // Launcher uses %TEMP%\sbc_* folders cleaned at runtime.
  // This command is a no-op placeholder that reports success.
  if (!opt.quiet) {
    std::wcout << utf8ToWide(kProductName) << L" " << utf8ToWide(kVersionString) << L"\n\n";
    std::wcout << L"[+] No leftover build temporary files to clean.\n";
    std::wcout << L"    Runtime temps are removed by generated executables after each run.\n";
  }
  return ExitCode::Success;
}

} // namespace sbc
