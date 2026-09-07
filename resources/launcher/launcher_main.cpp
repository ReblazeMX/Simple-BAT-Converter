// SBC runtime launcher — embedded into generated executables.
// Reads the appended payload (script + trailer) from its own image and runs it via cmd.exe.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Duplicated lightly so the stub has no dependency on the sbc headers package.
namespace {

constexpr char kMagic[4] = {'S', 'B', 'C', '1'};

#pragma pack(push, 1)
struct Trailer {
  std::uint16_t nameLen;
  std::uint32_t flags;
  std::uint64_t scriptSize;
  char magic[4];
};
#pragma pack(pop)

struct Payload {
  std::vector<std::uint8_t> script;
  std::string originalName; // UTF-8
  std::uint32_t flags = 0;
};

std::wstring getSelfPath() {
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (n == 0) return {};
    if (n < path.size()) {
      path.resize(n);
      return path;
    }
    path.resize(path.size() * 2);
  }
}

bool readPayload(const std::wstring& selfPath, Payload& out) {
  HANDLE h = CreateFileW(selfPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;

  LARGE_INTEGER fileSize{};
  if (!GetFileSizeEx(h, &fileSize) || fileSize.QuadPart < static_cast<LONGLONG>(sizeof(Trailer))) {
    CloseHandle(h);
    return false;
  }

  LARGE_INTEGER seek{};
  seek.QuadPart = fileSize.QuadPart - static_cast<LONGLONG>(sizeof(Trailer));
  if (!SetFilePointerEx(h, seek, nullptr, FILE_BEGIN)) {
    CloseHandle(h);
    return false;
  }

  Trailer tr{};
  DWORD got = 0;
  if (!ReadFile(h, &tr, sizeof(tr), &got, nullptr) || got != sizeof(tr)) {
    CloseHandle(h);
    return false;
  }

  if (std::memcmp(tr.magic, kMagic, 4) != 0) {
    CloseHandle(h);
    return false;
  }

  const std::uint64_t totalTail = tr.scriptSize + tr.nameLen + sizeof(Trailer);
  if (totalTail > static_cast<std::uint64_t>(fileSize.QuadPart)) {
    CloseHandle(h);
    return false;
  }

  seek.QuadPart = fileSize.QuadPart - static_cast<LONGLONG>(totalTail);
  if (!SetFilePointerEx(h, seek, nullptr, FILE_BEGIN)) {
    CloseHandle(h);
    return false;
  }

  out.script.resize(static_cast<std::size_t>(tr.scriptSize));
  if (tr.scriptSize > 0) {
    got = 0;
    std::size_t off = 0;
    while (off < out.script.size()) {
      const DWORD chunk = static_cast<DWORD>(
          (out.script.size() - off) > 0x100000 ? 0x100000 : (out.script.size() - off));
      if (!ReadFile(h, out.script.data() + off, chunk, &got, nullptr) || got == 0) {
        CloseHandle(h);
        return false;
      }
      off += got;
    }
  }

  out.originalName.resize(tr.nameLen);
  if (tr.nameLen > 0) {
    got = 0;
    if (!ReadFile(h, out.originalName.data(), tr.nameLen, &got, nullptr) || got != tr.nameLen) {
      CloseHandle(h);
      return false;
    }
  }

  out.flags = tr.flags;
  CloseHandle(h);
  return true;
}

std::wstring utf8ToWide(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return {};
  std::wstring w(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

std::wstring sanitizeBaseName(std::wstring name) {
  // Prevent path traversal: keep only the filename component and strip bad chars.
  const auto slash = name.find_last_of(L"\\/");
  if (slash != std::wstring::npos) name = name.substr(slash + 1);
  for (auto& c : name) {
    if (c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'|' || c == L'?' || c == L'*') {
      c = L'_';
    }
  }
  if (name.empty()) name = L"script.bat";
  // Ensure .bat/.cmd extension for cmd association
  const auto dot = name.find_last_of(L'.');
  if (dot == std::wstring::npos) {
    name += L".bat";
  }
  return name;
}

std::wstring makeTempDir() {
  wchar_t tempRoot[MAX_PATH];
  const DWORD n = GetTempPathW(MAX_PATH, tempRoot);
  if (n == 0 || n >= MAX_PATH) return {};

  GUID guid{};
  if (FAILED(CoCreateGuid(&guid))) {
    // Fallback without COM
    const DWORD tick = GetTickCount();
    const DWORD pid = GetCurrentProcessId();
    std::wstring dir = std::wstring(tempRoot) + L"sbc_" + std::to_wstring(pid) + L"_" +
                       std::to_wstring(tick);
    if (CreateDirectoryW(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) {
      return dir;
    }
    return {};
  }

  wchar_t guidStr[64];
  swprintf_s(guidStr, L"%08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
             static_cast<unsigned long>(guid.Data1), guid.Data2, guid.Data3,
             guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
             guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

  std::wstring dir = std::wstring(tempRoot) + L"sbc_" + guidStr;
  if (!CreateDirectoryW(dir.c_str(), nullptr)) {
    return {};
  }
  return dir;
}

bool writeAll(const std::wstring& path, const std::uint8_t* data, std::size_t size) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  std::size_t off = 0;
  while (off < size) {
    DWORD chunk = static_cast<DWORD>((size - off) > 0x100000 ? 0x100000 : (size - off));
    DWORD written = 0;
    if (!WriteFile(h, data + off, chunk, &written, nullptr) || written != chunk) {
      CloseHandle(h);
      return false;
    }
    off += written;
  }
  CloseHandle(h);
  return true;
}

void deleteTree(const std::wstring& dir) {
  std::wstring pattern = dir + L"\\*";
  WIN32_FIND_DATAW fd{};
  HANDLE find = FindFirstFileW(pattern.c_str(), &fd);
  if (find != INVALID_HANDLE_VALUE) {
    do {
      if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
      std::wstring child = dir + L"\\" + fd.cFileName;
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        deleteTree(child);
      } else {
        SetFileAttributesW(child.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(child.c_str());
      }
    } while (FindNextFileW(find, &fd));
    FindClose(find);
  }
  RemoveDirectoryW(dir.c_str());
}

std::wstring quoteArg(const std::wstring& s) {
  // Windows command-line escaping for CreateProcess
  if (s.find_first_of(L" \t\"") == std::wstring::npos) return s;
  std::wstring out;
  out.push_back(L'"');
  int backslashes = 0;
  for (wchar_t c : s) {
    if (c == L'\\') {
      ++backslashes;
    } else if (c == L'"') {
      out.append(static_cast<std::size_t>(backslashes) * 2 + 1, L'\\');
      out.push_back(L'"');
      backslashes = 0;
    } else {
      if (backslashes) {
        out.append(static_cast<std::size_t>(backslashes), L'\\');
        backslashes = 0;
      }
      out.push_back(c);
    }
  }
  if (backslashes) out.append(static_cast<std::size_t>(backslashes) * 2, L'\\');
  out.push_back(L'"');
  return out;
}

std::wstring buildCmdLine(const std::wstring& scriptPath, int argc, wchar_t** argv) {
  // cmd.exe /D /S /C ""script" arg1 arg2 ..."
  // /D disables AutoRun, /S modifies /C quote handling.
  std::wstring cmd = L"cmd.exe /D /S /C \"";
  cmd += quoteArg(scriptPath);
  for (int i = 1; i < argc; ++i) {
    cmd.push_back(L' ');
    cmd += quoteArg(argv[i]);
  }
  cmd.push_back(L'"');
  return cmd;
}

int runScript(const std::wstring& scriptPath, int argc, wchar_t** argv) {
  std::wstring cmdLine = buildCmdLine(scriptPath, argc, argv);

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};

  // CreateProcessW may modify the command line buffer.
  std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
  mutableCmd.push_back(L'\0');

  const BOOL ok = CreateProcessW(
      nullptr,
      mutableCmd.data(),
      nullptr,
      nullptr,
      FALSE,
      0,
      nullptr,
      nullptr, // inherit current working directory
      &si,
      &pi);

  if (!ok) {
    return 7;
  }

  CloseHandle(pi.hThread);
  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode = 1;
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hProcess);
  return static_cast<int>(exitCode);
}

// True when this process is the only one attached to the console.
// That usually means the EXE was double-clicked (Explorer allocated the console).
bool ownsConsoleAlone() {
  DWORD pids[16];
  DWORD n = GetConsoleProcessList(pids, static_cast<DWORD>(std::size(pids)));
  if (n == 0) {
    // Buffer too small or no console — query required size.
    n = GetConsoleProcessList(pids, 0);
  }
  return n == 1;
}

void waitForKeypress() {
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  if (hIn == nullptr || hIn == INVALID_HANDLE_VALUE) return;

  DWORD mode = 0;
  const bool hasConsole = GetConsoleMode(hIn, &mode) != 0;

  fwprintf(stdout, L"\nPress any key to continue . . . ");
  fflush(stdout);

  if (hasConsole) {
    FlushConsoleInputBuffer(hIn);
    INPUT_RECORD rec{};
    DWORD read = 0;
    while (ReadConsoleInputW(hIn, &rec, 1, &read) && read == 1) {
      if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
        const WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
            vk == VK_LWIN || vk == VK_RWIN || vk == VK_CAPITAL ||
            vk == VK_NUMLOCK || vk == VK_SCROLL) {
          continue;
        }
        break;
      }
    }
    fwprintf(stdout, L"\n");
  } else {
    // Fallback if stdin is redirected
    wint_t ch = fgetwc(stdin);
    (void)ch;
  }
}

void maybePauseAfterScript(std::uint32_t flags) {
#if !SBC_CONSOLE
  (void)flags;
  return;
#else
  constexpr std::uint32_t kNoPause = 1u << 1;
  constexpr std::uint32_t kAlwaysPause = 1u << 2;

  if (flags & kNoPause) return;

  const bool pause = (flags & kAlwaysPause) || ownsConsoleAlone();
  if (pause) waitForKeypress();
#endif
}

int launcherMain(int argc, wchar_t** argv) {
  // COM for CoCreateGuid (optional failure handled)
  const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool comOk = SUCCEEDED(co);

  const std::wstring self = getSelfPath();
  if (self.empty()) {
    if (comOk) CoUninitialize();
    return 7;
  }

  Payload payload;
  if (!readPayload(self, payload)) {
    if (comOk) CoUninitialize();
#if SBC_CONSOLE
    fwprintf(stderr, L"SBC launcher: invalid or missing embedded script payload.\n");
#endif
    return 7;
  }

  const std::wstring tempDir = makeTempDir();
  if (tempDir.empty()) {
    if (comOk) CoUninitialize();
#if SBC_CONSOLE
    fwprintf(stderr, L"SBC launcher: failed to create temporary directory.\n");
#endif
    return 7;
  }

  std::wstring baseName = sanitizeBaseName(utf8ToWide(payload.originalName));
  const std::wstring scriptPath = tempDir + L"\\" + baseName;

  if (!writeAll(scriptPath, payload.script.data(), payload.script.size())) {
    deleteTree(tempDir);
    if (comOk) CoUninitialize();
#if SBC_CONSOLE
    fwprintf(stderr, L"SBC launcher: failed to write temporary script.\n");
#endif
    return 7;
  }

  const int code = runScript(scriptPath, argc, argv);

  // Best-effort cleanup before pause so temp files are not left if the user
  // leaves the window open.
  SetFileAttributesW(scriptPath.c_str(), FILE_ATTRIBUTE_NORMAL);
  DeleteFileW(scriptPath.c_str());
  deleteTree(tempDir);

  maybePauseAfterScript(payload.flags);

  if (comOk) CoUninitialize();
  return code;
}

} // namespace

#if SBC_CONSOLE
int wmain(int argc, wchar_t** argv) {
  return launcherMain(argc, argv);
}
#else
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv) return 7;
  const int code = launcherMain(argc, argv);
  LocalFree(argv);
  return code;
}
#endif
