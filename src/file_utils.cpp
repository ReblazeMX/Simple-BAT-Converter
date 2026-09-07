#include "file_utils.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlwapi.h>

#include <algorithm>
#include <stdexcept>
#include <cwctype>

#pragma comment(lib, "shlwapi.lib")

namespace sbc {
namespace {

std::wstring normalizeSlashes(std::wstring p) {
  for (auto& c : p) {
    if (c == L'/') c = L'\\';
  }
  return p;
}

} // namespace

std::wstring utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                         static_cast<int>(utf8.size()), nullptr, 0);
  if (needed <= 0) return {};
  std::wstring out(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                      out.data(), needed);
  return out;
}

std::string wideToUtf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                         static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
  if (needed <= 0) return {};
  std::string out(static_cast<std::size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                      out.data(), needed, nullptr, nullptr);
  return out;
}

bool fileExists(const std::wstring& path) {
  const DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool directoryExists(const std::wstring& path) {
  const DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool createDirectories(const std::wstring& path) {
  if (path.empty()) return false;
  if (directoryExists(path)) return true;

  std::wstring cur;
  std::size_t i = 0;
  // Preserve drive root (e.g. C:\)
  if (path.size() >= 2 && path[1] == L':') {
    cur = path.substr(0, 2);
    i = 2;
    if (i < path.size() && (path[i] == L'\\' || path[i] == L'/')) {
      cur.push_back(L'\\');
      ++i;
    }
  } else if (!path.empty() && (path[0] == L'\\' || path[0] == L'/')) {
    cur = L"\\";
    i = 1;
  }

  while (i < path.size()) {
    while (i < path.size() && (path[i] == L'\\' || path[i] == L'/')) ++i;
    if (i >= path.size()) break;
    const std::size_t start = i;
    while (i < path.size() && path[i] != L'\\' && path[i] != L'/') ++i;
    if (!cur.empty() && cur.back() != L'\\') cur.push_back(L'\\');
    cur.append(path, start, i - start);
    if (!directoryExists(cur)) {
      if (!CreateDirectoryW(cur.c_str(), nullptr)) {
        const DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) return false;
      }
    }
  }
  return directoryExists(path);
}

std::wstring absolutePath(const std::wstring& path) {
  wchar_t buf[MAX_PATH * 4];
  const DWORD n = GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(buf)), buf, nullptr);
  if (n == 0 || n >= std::size(buf)) return normalizeSlashes(path);
  return normalizeSlashes(std::wstring(buf, n));
}

std::wstring fileName(const std::wstring& path) {
  const auto p = normalizeSlashes(path);
  const auto pos = p.find_last_of(L'\\');
  if (pos == std::wstring::npos) return p;
  return p.substr(pos + 1);
}

std::wstring fileStem(const std::wstring& path) {
  auto name = fileName(path);
  const auto dot = name.find_last_of(L'.');
  if (dot == std::wstring::npos || dot == 0) return name;
  return name.substr(0, dot);
}

std::wstring fileExtension(const std::wstring& path) {
  auto name = fileName(path);
  const auto dot = name.find_last_of(L'.');
  if (dot == std::wstring::npos) return {};
  std::wstring ext = name.substr(dot);
  for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
  return ext;
}

std::wstring parentPath(const std::wstring& path) {
  const auto p = normalizeSlashes(path);
  const auto pos = p.find_last_of(L'\\');
  if (pos == std::wstring::npos) return L".";
  if (pos == 0) return L"\\";
  if (pos == 2 && p[1] == L':') return p.substr(0, 3); // drive root
  return p.substr(0, pos);
}

std::wstring joinPath(const std::wstring& a, const std::wstring& b) {
  if (a.empty()) return normalizeSlashes(b);
  if (b.empty()) return normalizeSlashes(a);
  std::wstring left = normalizeSlashes(a);
  std::wstring right = normalizeSlashes(b);
  if (!left.empty() && left.back() == L'\\') left.pop_back();
  while (!right.empty() && right.front() == L'\\') right.erase(right.begin());
  return left + L'\\' + right;
}

std::vector<std::uint8_t> readFileBytes(const std::wstring& path) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("Failed to open file for reading");
  }

  LARGE_INTEGER sz{};
  if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0) {
    CloseHandle(h);
    throw std::runtime_error("Failed to get file size");
  }

  std::vector<std::uint8_t> data(static_cast<std::size_t>(sz.QuadPart));
  if (!data.empty()) {
    std::size_t offset = 0;
    while (offset < data.size()) {
      DWORD toRead = static_cast<DWORD>(
          (std::min)(data.size() - offset, static_cast<std::size_t>(1 << 20)));
      DWORD got = 0;
      if (!ReadFile(h, data.data() + offset, toRead, &got, nullptr) || got == 0) {
        CloseHandle(h);
        throw std::runtime_error("Failed to read file");
      }
      offset += got;
    }
  }
  CloseHandle(h);
  return data;
}

bool writeFileBytes(const std::wstring& path, const std::uint8_t* data, std::size_t size) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;

  std::size_t offset = 0;
  while (offset < size) {
    DWORD toWrite = static_cast<DWORD>(
        (std::min)(size - offset, static_cast<std::size_t>(1 << 20)));
    DWORD written = 0;
    if (!WriteFile(h, data + offset, toWrite, &written, nullptr) || written != toWrite) {
      CloseHandle(h);
      return false;
    }
    offset += written;
  }
  CloseHandle(h);
  return true;
}

bool writeFileBytes(const std::wstring& path, const std::vector<std::uint8_t>& data) {
  return writeFileBytes(path, data.data(), data.size());
}

std::uint64_t fileSize(const std::wstring& path) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return 0;
  LARGE_INTEGER sz{};
  const BOOL ok = GetFileSizeEx(h, &sz);
  CloseHandle(h);
  return ok ? static_cast<std::uint64_t>(sz.QuadPart) : 0;
}

bool isBatOrCmd(const std::wstring& path) {
  const auto ext = fileExtension(path);
  return ext == L".bat" || ext == L".cmd";
}

} // namespace sbc
