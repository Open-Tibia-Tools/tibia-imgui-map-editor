#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace MapEditor::Utils {

template <size_t N>
void copyTruncate(char (&destination)[N], std::string_view source) {
  static_assert(N > 0, "Destination buffer must not be empty.");

  const auto copyLength = std::min(source.size(), N - 1);
  if (copyLength > 0) {
    std::memcpy(destination, source.data(), copyLength);
  }
  destination[copyLength] = '\0';
}

} // namespace MapEditor::Utils
