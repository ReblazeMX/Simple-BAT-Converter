#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sbc {

std::wstring utf8ToWide(const std::string& utf8);
std::string wideToUtf8(const std::wstring& wide);

bool fileExists(const std::wstring& path);
bool directoryExists(const std::wstring& path);
bool createDirectories(const std::wstring& path);

std::wstring absolutePath(const std::wstring& path);
std::wstring fileName(const std::wstring& path);
std::wstring fileStem(const std::wstring& path);
std::wstring fileExtension(const std::wstring& path);
std::wstring parentPath(const std::wstring& path);
std::wstring joinPath(const std::wstring& a, const std::wstring& b);

// Case-insensitive comparison of two Windows paths (both already normalized).
bool iequalPath(const std::wstring& a, const std::wstring& b);

// Recursively delete a directory tree; returns false if it could not be
// fully removed.
bool removeTree(const std::wstring& dir);

std::vector<std::uint8_t> readFileBytes(const std::wstring& path);
bool writeFileBytes(const std::wstring& path, const std::uint8_t* data, std::size_t size);
bool writeFileBytes(const std::wstring& path, const std::vector<std::uint8_t>& data);

std::uint64_t fileSize(const std::wstring& path);

bool isBatOrCmd(const std::wstring& path);

} // namespace sbc
