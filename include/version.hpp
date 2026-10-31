#pragma once

#include <string>

namespace sbc {

constexpr const char* kProductName = "Simple BAT Converter";
constexpr const char* kInternalName = "sbc";
constexpr const char* kAuthor = "ReblazeMX";
constexpr int kVersionMajor = 1;
constexpr int kVersionMinor = 1;
constexpr int kVersionPatch = 0;
constexpr const char* kVersionString = "1.1.0";

std::string versionBanner();
std::string licenseText();

} // namespace sbc
