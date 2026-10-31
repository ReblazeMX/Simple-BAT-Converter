#include "compiler.hpp"
#include "exit_codes.hpp"
#include "file_utils.hpp"
#include "icon.hpp"
#include "payload.hpp"
#include "version.hpp"

#include "stub_console.hpp"
#include "stub_noconsole.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cwchar>
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

  // Guard against overwriting the input script when -o points at it.
  if (iequalPath(input, output)) {
    result.exitCode = ExitCode::OutputError;
    result.message = "Error: Output path is the same as the input file.";
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

  if (script.empty()) {
    result.exitCode = ExitCode::GeneralError;
    result.message = "Error: Input file is empty.";
    errorLine(opt, result.message);
    return result;
  }

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
  if (opt.keepTemp) flags |= kFlagKeepTemp;

  // Prefer original filename so %~nx0-style usage is closer to real batch
  const std::string origName = wideToUtf8(fileName(input));
  if (origName.size() > 0xFFFF) {
    result.exitCode = ExitCode::GeneralError;
    result.message = "Error: Input file name is too long.";
    errorLine(opt, result.message);
    return result;
  }

  auto payload = buildPayload(script, origName, flags);

  // Start from the stub bytes, injecting the icon first (before the payload is
  // appended) so the icon resource never disturbs the trailing payload data.
  std::vector<std::uint8_t> base;
  if (!opt.iconPath.empty()) {
    const std::wstring iconInput = absolutePath(opt.iconPath);
    if (fileExtension(iconInput) != L".ico") {
      result.exitCode = ExitCode::UnsupportedFileType;
      result.message = "Error: Icon file must have a .ico extension.";
      errorLine(opt, result.message);
      return result;
    }
    if (!fileExists(iconInput)) {
      result.exitCode = ExitCode::InputNotFound;
      result.message = "Error: Icon file does not exist:\n" + wideToUtf8(opt.iconPath);
      errorLine(opt, result.message);
      return result;
    }

    std::vector<std::uint8_t> ico;
    try {
      ico = readFileBytes(iconInput);
    } catch (...) {
      result.exitCode = ExitCode::GeneralError;
      result.message = "Error: Failed to read icon file.";
      errorLine(opt, result.message);
      return result;
    }

    if (!isValidIco(ico)) {
      result.exitCode = ExitCode::UnsupportedFileType;
      result.message = "Error: Invalid .ico file.";
      errorLine(opt, result.message);
      return result;
    }

    info(opt, L"[+] Embedding icon...");
    if (!embedIcon(std::vector<std::uint8_t>(stub, stub + stubSize), ico, base)) {
      result.exitCode = ExitCode::BuildError;
      result.message = "Error: Failed to embed icon into the executable.";
      errorLine(opt, result.message);
      return result;
    }
  } else {
    base.assign(stub, stub + stubSize);
  }

  std::vector<std::uint8_t> exe;
  exe.reserve(base.size() + payload.size());
  exe.insert(exe.end(), base.begin(), base.end());
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
  if (!opt.iconPath.empty()) {
    std::wcout << L"Icon:          " << absolutePath(opt.iconPath) << L"\n";
  }
  return ExitCode::Success;
}

int cleanTempFiles(const Options& opt) {
  // The launcher extracts scripts into %TEMP%\sbc_* directories and normally
  // removes them at runtime. This command removes any leftovers (for example
  // from executables built with --keep-temp or interrupted runs).
  wchar_t tempRoot[MAX_PATH];
  const DWORD tempLen = GetTempPathW(MAX_PATH, tempRoot);
  if (tempLen == 0 || tempLen >= MAX_PATH) {
    errorLine(opt, "Error: Failed to resolve the temporary directory.");
    return ExitCode::GeneralError;
  }

  const std::wstring pattern = std::wstring(tempRoot) + L"sbc_*";
  WIN32_FIND_DATAW fd{};
  HANDLE find = FindFirstFileW(pattern.c_str(), &fd);
  int removed = 0;
  if (find != INVALID_HANDLE_VALUE) {
    do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
      if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
      const std::wstring dir = std::wstring(tempRoot) + fd.cFileName;
      if (removeTree(dir)) ++removed;
    } while (FindNextFileW(find, &fd));
    FindClose(find);
  }

  if (!opt.quiet) {
    std::wcout << utf8ToWide(kProductName) << L" " << utf8ToWide(kVersionString) << L"\n\n";
    std::wcout << L"[+] Removed " << removed << L" temporary director"
               << (removed == 1 ? L"y" : L"ies") << L".\n";
  }
  return ExitCode::Success;
}

} // namespace sbc
