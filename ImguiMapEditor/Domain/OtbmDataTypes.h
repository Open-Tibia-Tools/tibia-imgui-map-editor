#pragma once
#include <cstdint>

namespace MapEditor {
namespace Domain {

enum class OtbmReadSource : uint8_t { Otbm = 0, Xml = 1 };

enum class OtbmWriteTarget : uint8_t { Otbm = 0, Xml = 1 };

} // namespace Domain
} // namespace MapEditor
