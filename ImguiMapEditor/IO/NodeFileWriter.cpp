#include "NodeFileWriter.h"
#include <cstring>
#include <bit>
#include <array>
#include <algorithm>

namespace MapEditor::IO {

NodeFileWriteHandle::NodeFileWriteHandle(
    const std::filesystem::path& path,
    const std::string& identifier
) {
#ifdef _WIN32
    _wfopen_s(&file_, path.c_str(), L"wb");
#else
    file_ = std::fopen(path.c_str(), "wb");
#endif
    if (!file_) {
        error_ = true;
        return;
    }
    
    buffer_.reserve(BUFFER_SIZE);
    
    // Write 4-byte identifier (not escaped)
    if (identifier.size() >= 4) {
        fwrite(identifier.data(), 1, 4, file_);
    } else {
        char id[4] = {0};
        memcpy(id, identifier.data(), identifier.size());
        fwrite(id, 1, 4, file_);
    }
}

NodeFileWriteHandle::~NodeFileWriteHandle() {
    close();
}

bool NodeFileWriteHandle::startNode(uint8_t type) {
    if (error_) return false;
    
    if (!writeRawByte(NODE_START)) return false;
    if (!writeEscaped(type)) return false;
    
    node_depth_++;
    return true;
}

bool NodeFileWriteHandle::endNode() {
    if (error_ || node_depth_ <= 0) return false;
    
    if (!writeRawByte(NODE_END)) return false;
    
    node_depth_--;
    return true;
}

bool NodeFileWriteHandle::writeU8(uint8_t value) {
    return writeEscaped(value);
}

bool NodeFileWriteHandle::writeU16(uint16_t value) {
    auto bytes = std::bit_cast<std::array<uint8_t, 2>>(value);
    return std::ranges::all_of(bytes, [this](uint8_t b) { return writeEscaped(b); });
}

bool NodeFileWriteHandle::writeU32(uint32_t value) {
    auto bytes = std::bit_cast<std::array<uint8_t, 4>>(value);
    return std::ranges::all_of(bytes, [this](uint8_t b) { return writeEscaped(b); });
}

bool NodeFileWriteHandle::writeU64(uint64_t value) {
    auto bytes = std::bit_cast<std::array<uint8_t, 8>>(value);
    return std::ranges::all_of(bytes, [this](uint8_t b) { return writeEscaped(b); });
}

bool NodeFileWriteHandle::writeString(const std::string& str) {
    if (str.size() > 0xFFFF) return false;
    
    if (!writeU16(static_cast<uint16_t>(str.size()))) return false;
    return writeRAW(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

bool NodeFileWriteHandle::writeLongString(const std::string& str) {
    if (!writeU32(static_cast<uint32_t>(str.size()))) return false;
    return writeRAW(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

bool NodeFileWriteHandle::writeRAW(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (!writeEscaped(data[i])) return false;
    }
    return true;
}

bool NodeFileWriteHandle::writeRAW(const std::string& data) {
    return writeRAW(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool NodeFileWriteHandle::writeRawByte(uint8_t byte) {
    buffer_.push_back(byte);
    if (buffer_.size() >= BUFFER_SIZE) {
        return flush();
    }
    return true;
}

bool NodeFileWriteHandle::writeEscaped(uint8_t byte) {
    // Escape special bytes
    if (byte == NODE_START || byte == NODE_END || byte == NODE_ESCAPE) {
        if (!writeRawByte(NODE_ESCAPE)) return false;
    }
    return writeRawByte(byte);
}

bool NodeFileWriteHandle::flush() {
    if (!file_ || buffer_.empty()) return !error_;
    
    size_t written = fwrite(buffer_.data(), 1, buffer_.size(), file_);
    if (written != buffer_.size()) {
        error_ = true;
        return false;
    }
    
    buffer_.clear();
    return true;
}

void NodeFileWriteHandle::close() {
    if (file_) {
        flush();
        fclose(file_);
        file_ = nullptr;
        if (!error_) {
            closed_successfully_ = true;
        }
    }
}

} // namespace MapEditor::IO
