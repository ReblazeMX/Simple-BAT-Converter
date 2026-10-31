#include "icon.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstring>

namespace sbc {
namespace {

std::uint16_t readU16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t readU32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

void writeU16(std::uint8_t* p, std::uint16_t v) {
  p[0] = static_cast<std::uint8_t>(v & 0xFF);
  p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

struct IcoHeader {
  std::uint16_t reserved;
  std::uint16_t type;
  std::uint16_t count;
};

// ICONDIRENTRY is 16 bytes: width, height, colorCount, reserved, planes,
// bitCount, bytesInRes, imageOffset.
constexpr std::size_t kEntrySize = 16;
// GRPICONDIRENTRY is 14 bytes: the same fields without the trailing imageOffset.
constexpr std::size_t kGroupEntrySize = 14;

bool parseIco(const std::vector<std::uint8_t>& ico, IcoHeader& hdr) {
  if (ico.size() < 6) return false;
  hdr.reserved = readU16(ico.data() + 0);
  hdr.type = readU16(ico.data() + 2);
  hdr.count = readU16(ico.data() + 4);
  if (hdr.reserved != 0 || hdr.type != 1 || hdr.count == 0) return false;
  if (ico.size() < 6 + static_cast<std::size_t>(hdr.count) * kEntrySize) return false;
  return true;
}

} // namespace

bool isValidIco(const std::vector<std::uint8_t>& ico) {
  IcoHeader hdr{};
  if (!parseIco(ico, hdr)) return false;

  for (std::uint16_t i = 0; i < hdr.count; ++i) {
    const std::uint8_t* e = ico.data() + 6 + static_cast<std::size_t>(i) * kEntrySize;
    const std::uint32_t bytesInRes = readU32(e + 8);
    const std::uint32_t offset = readU32(e + 12);
    if (bytesInRes == 0) return false;
    if (static_cast<std::uint64_t>(offset) + bytesInRes > ico.size()) return false;
  }
  return true;
}

bool embedIcon(const std::vector<std::uint8_t>& peImage,
               const std::vector<std::uint8_t>& ico,
               std::vector<std::uint8_t>& out) {
  out.clear();

  IcoHeader hdr{};
  if (!parseIco(ico, hdr) || !isValidIco(ico)) return false;
  if (peImage.empty()) return false;

  // Write the stub image to a unique temporary file so UpdateResource can
  // modify it in place without disturbing any appended payload bytes.
  wchar_t tempPath[MAX_PATH];
  if (GetTempPathW(MAX_PATH, tempPath) == 0) return false;

  wchar_t tempFile[MAX_PATH];
  if (GetTempFileNameW(tempPath, L"sbc", 0, tempFile) == 0) return false;

  {
    HANDLE h = CreateFileW(tempFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
      DeleteFileW(tempFile);
      return false;
    }
    std::size_t off = 0;
    while (off < peImage.size()) {
      const DWORD chunk = static_cast<DWORD>(
          (peImage.size() - off) > 0x100000 ? 0x100000 : (peImage.size() - off));
      DWORD written = 0;
      if (!WriteFile(h, peImage.data() + off, chunk, &written, nullptr) ||
          written != chunk) {
        CloseHandle(h);
        DeleteFileW(tempFile);
        return false;
      }
      off += written;
    }
    CloseHandle(h);
  }

  // Build the RT_GROUP_ICON directory: the ICONDIR header followed by one
  // GRPICONDIRENTRY per image (each image becomes an RT_ICON resource with an
  // id equal to its 1-based index).
  const std::size_t groupSize = 6 + static_cast<std::size_t>(hdr.count) * kGroupEntrySize;
  std::vector<std::uint8_t> group(groupSize);
  std::memcpy(group.data(), ico.data(), 6);
  for (std::uint16_t i = 0; i < hdr.count; ++i) {
    const std::uint8_t* src = ico.data() + 6 + static_cast<std::size_t>(i) * kEntrySize;
    std::uint8_t* dst = group.data() + 6 + static_cast<std::size_t>(i) * kGroupEntrySize;
    std::memcpy(dst, src, 12); // copy all fields except imageOffset
    writeU16(dst + 12, static_cast<std::uint16_t>(i + 1));
  }

  HANDLE update = BeginUpdateResourceW(tempFile, FALSE);
  if (update == nullptr) {
    DeleteFileW(tempFile);
    return false;
  }

  const LANGID lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
  bool ok = true;

  if (!UpdateResourceW(update, RT_GROUP_ICON, MAKEINTRESOURCEW(1), lang,
                       group.data(), static_cast<DWORD>(group.size()))) {
    ok = false;
  }

  for (std::uint16_t i = 0; ok && i < hdr.count; ++i) {
    const std::uint8_t* e = ico.data() + 6 + static_cast<std::size_t>(i) * kEntrySize;
    const std::uint32_t bytesInRes = readU32(e + 8);
    const std::uint32_t offset = readU32(e + 12);
    if (!UpdateResourceW(update, RT_ICON, MAKEINTRESOURCEW(i + 1), lang,
                         const_cast<std::uint8_t*>(ico.data() + offset), bytesInRes)) {
      ok = false;
    }
  }

  if (!EndUpdateResourceW(update, FALSE)) {
    ok = false;
  }

  if (!ok) {
    DeleteFileW(tempFile);
    return false;
  }

  // Read the modified image back and clean up.
  HANDLE h = CreateFileW(tempFile, GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    DeleteFileW(tempFile);
    return false;
  }
  LARGE_INTEGER size{};
  const bool gotSize = GetFileSizeEx(h, &size) && size.QuadPart > 0;
  if (!gotSize) {
    CloseHandle(h);
    DeleteFileW(tempFile);
    return false;
  }
  out.resize(static_cast<std::size_t>(size.QuadPart));
  std::size_t off = 0;
  while (off < out.size()) {
    const DWORD chunk = static_cast<DWORD>(
        (out.size() - off) > 0x100000 ? 0x100000 : (out.size() - off));
    DWORD read = 0;
    if (!ReadFile(h, out.data() + off, chunk, &read, nullptr) || read == 0) {
      CloseHandle(h);
      DeleteFileW(tempFile);
      return false;
    }
    off += read;
  }
  CloseHandle(h);
  DeleteFileW(tempFile);
  return true;
}

} // namespace sbc
