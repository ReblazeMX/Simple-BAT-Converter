#pragma once

#include <cstdint>
#include <vector>

namespace sbc {

// Returns true when `ico` is a structurally valid Windows icon (.ico) file.
bool isValidIco(const std::vector<std::uint8_t>& ico);

// Injects the icon stored in `ico` into a copy of the PE image `peImage` and
// returns the result in `out`. Uses the Windows UpdateResource API on a
// temporary file, so the trailing payload is appended afterwards by the
// caller. Returns false on any failure.
bool embedIcon(const std::vector<std::uint8_t>& peImage,
               const std::vector<std::uint8_t>& ico,
               std::vector<std::uint8_t>& out);

} // namespace sbc
