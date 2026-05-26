#pragma once
#include <cstdint>
#include <vector>

namespace MapEditor::IO {

struct OpaqueTileAttribute {
    uint8_t attributeId;
    std::vector<uint8_t> rawBytes;
};

struct OpaqueChildNode {
    uint8_t nodeType;
    std::vector<uint8_t> rawBytes;
};

struct InvalidZoneState {
    std::vector<OpaqueTileAttribute> opaqueAttributes;
    std::vector<OpaqueChildNode> opaqueChildNodes;
    uint32_t unknownMapFlags = 0;

    bool hasContent() const {
        return !opaqueAttributes.empty() || !opaqueChildNodes.empty() || unknownMapFlags != 0;
    }
};

} // namespace MapEditor::IO
