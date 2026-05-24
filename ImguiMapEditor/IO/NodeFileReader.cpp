#include "NodeFileReader.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <cassert>

namespace MapEditor {
namespace IO {

//=============================================================================
// BinaryNode Implementation
//=============================================================================

BinaryNode::BinaryNode(NodeFileReadHandle* file, BinaryNode* parent)
    : file_(file)
    , parent_(parent) {
}

BinaryNode::~BinaryNode() {
    // We cannot call file_->freeNode(child_) here recursively because
    // it would cause stack overflow for deep trees.
    // The cleanup logic is now handled iteratively in NodeFileReadHandle::freeNode.
    // However, if a BinaryNode is destroyed manually (outside freeNode),
    // we must ensure child is freed.
    // But BinaryNode is designed to be managed by NodeFileReadHandle.
    // So we rely on freeNode.

    // Safety fallback: if we are destroyed and still have a child,
    // we must free it. But we can't be sure if we are in recursive context.
    // Ideally, freeNode clears child_ before calling destructor.
    if (child_) {
        file_->freeNode(child_);
    }
}

bool BinaryNode::getU8(uint8_t& value) { return getType(value); }
bool BinaryNode::getU16(uint16_t& value) { return getType(value); }
bool BinaryNode::getU32(uint32_t& value) { return getType(value); }
bool BinaryNode::getU64(uint64_t& value) { return getType(value); }

bool BinaryNode::peekU8(uint8_t& value) const {
    if (read_offset_ >= data_.size()) {
        return false;
    }
    // Access data directly without advancing offset
    value = static_cast<uint8_t>(data_[read_offset_]);
    return true;
}

bool BinaryNode::skip(size_t size) {
    if (size > bytesRemaining()) {
        read_offset_ = data_.size();
        return false;
    }
    read_offset_ += size;
    return true;
}

bool BinaryNode::getRAW(uint8_t* ptr, size_t size) {
    if (size > bytesRemaining()) {
        read_offset_ = data_.size();
        return false;
    }
    std::memcpy(ptr, data_.data() + read_offset_, size);
    read_offset_ += size;
    return true;
}

bool BinaryNode::getRAW(std::string& str, size_t size) {
    if (size > bytesRemaining()) {
        read_offset_ = data_.size();
        return false;
    }
    str.assign(data_.data() + read_offset_, size);
    read_offset_ += size;
    return true;
}

bool BinaryNode::getString(std::string& str) {
    uint16_t len;
    if (!getU16(len)) {
        return false;
    }
    return getRAW(str, len);
}

bool BinaryNode::getLongString(std::string& str) {
    uint32_t len;
    if (!getU32(len)) {
        return false;
    }
    return getRAW(str, len);
}

BinaryNode* BinaryNode::getChild() {
    assert(file_);
    assert(child_ == nullptr);
    
    if (file_->last_was_start_) {
        child_ = file_->getAndLoadNode(this);
        return child_;
    }
    return nullptr;
}

BinaryNode* BinaryNode::advance() {
    assert(file_);
    
    // Safety check: if error occurred, detach and stop
    if (file_->error_code_ != NodeFileError::None) {
        if (parent_) {
            parent_->child_ = nullptr;
        } else if (file_->root_node_ == this) {
            file_->root_node_ = nullptr;
        }
        file_->freeNode(this);
        return nullptr;
    }
    
    // Ensure we've processed all children first
    if (child_ == nullptr) {
        getChild();
    }
    
    // Skip through remaining child tree
    while (child_) {
        child_->getChild();
        // [UB FIX] Must update child_ with result!
        // child_->advance() frees the node if it returns nullptr (end of siblings).
        // Ignoring the return value leaves child_ pointing to a freed/pooled object (zombie).
        child_ = child_->advance();
    }
    
    if (file_->last_was_start_) {
        // The next thing is a child node, not a sibling
        return nullptr;
    }
    
    // Last was NODE_END (0xFF)
    // Read next byte to see if there's another sibling
    uint8_t*& cache = file_->cache_;
    size_t const& cache_length = file_->cache_length_;
    size_t& local_read_index = file_->local_read_index_;
    
    if (local_read_index >= cache_length) {
        if (!file_->renewCache()) {
            if (parent_) {
                parent_->child_ = nullptr;
            } else if (file_->root_node_ == this) {
                file_->root_node_ = nullptr;
            }
            file_->freeNode(this);
            return nullptr;
        }
    }
    
    uint8_t const op = cache[local_read_index];
    ++local_read_index;
    
    if (op == static_cast<uint8_t>(NodeMarker::Start)) {
        // Another sibling follows - reuse this node
        read_offset_ = 0;
        data_.clear();
        load();
        return this;
    } else if (op == static_cast<uint8_t>(NodeMarker::End)) {
        // End of siblings - return to parent level
        if (parent_) {
            parent_->child_ = nullptr;
        } else if (file_->root_node_ == this) {
            file_->root_node_ = nullptr;
        }
        file_->last_was_start_ = false;
        file_->freeNode(this);
        return nullptr;
    } else {
        file_->error_code_ = NodeFileError::SyntaxError;
        return nullptr;
    }
}

void BinaryNode::load() {
    assert(file_);
    
    // Pre-reserve to reduce reallocations (average node ~100-200 bytes)
    data_.reserve(256);
    
    uint8_t*& cache = file_->cache_;
    size_t const& cache_length = file_->cache_length_;
    size_t& local_read_index = file_->local_read_index_;
    
    while (true) {
        if (local_read_index >= cache_length) {
            if (!file_->renewCache()) {
                file_->error_code_ = NodeFileError::PrematureEnd;
                return;
            }
        }
        
        uint8_t op = cache[local_read_index];
        ++local_read_index;
        
        switch (op) {
            case static_cast<uint8_t>(NodeMarker::Start):
                file_->last_was_start_ = true;
                return;
                
            case static_cast<uint8_t>(NodeMarker::End):
                file_->last_was_start_ = false;
                return;
                
            case static_cast<uint8_t>(NodeMarker::Escape):
                if (local_read_index >= cache_length) {
                    if (!file_->renewCache()) {
                        file_->error_code_ = NodeFileError::PrematureEnd;
                        return;
                    }
                }
                op = cache[local_read_index];
                ++local_read_index;
                break;
                
            default:
                break;
        }
        
        data_.push_back(static_cast<char>(op));
    }
}

//=============================================================================
// NodeFileReadHandle Implementation
//=============================================================================

NodeFileReadHandle::NodeFileReadHandle() {
}

NodeFileReadHandle::~NodeFileReadHandle() {
    while (!unused_.empty()) {
        std::free(unused_.top());
        unused_.pop();
    }
}

std::string NodeFileReadHandle::getErrorMessage() const {
    switch (error_code_) {
        case NodeFileError::None: return "No error";
        case NodeFileError::CouldNotOpen: return "Could not open file";
        case NodeFileError::InvalidIdentifier: return "File magic number not recognized";
        case NodeFileError::StringTooLong: return "Too long string encountered";
        case NodeFileError::ReadError: return "Failed to read from file";
        case NodeFileError::WriteError: return "Failed to write to file";
        case NodeFileError::SyntaxError: return "Node file syntax error";
        case NodeFileError::PrematureEnd: return "File end encountered unexpectedly";
        case NodeFileError::OutOfMemory: return "Out of memory";
    }
    return "Unknown error";
}

BinaryNode* NodeFileReadHandle::getNode(BinaryNode* parent) {
    void* mem;
    if (unused_.empty()) {
        mem = std::malloc(sizeof(BinaryNode));
        if (mem == nullptr) {
            error_code_ = NodeFileError::OutOfMemory;
            return nullptr;
        }
    } else {
        mem = unused_.top();
        unused_.pop();
    }
    return new (mem) BinaryNode(this, parent);
}

BinaryNode* NodeFileReadHandle::getAndLoadNode(BinaryNode* parent) {
    BinaryNode* node = getNode(parent);
    if (node != nullptr) {
        node->load();
    }
    return node;
}

void NodeFileReadHandle::freeNode(BinaryNode* node) {
    // Implement iterative destruction to prevent stack overflow
    // on deep trees (O(1) stack space instead of O(N)).
    while (node) {
        // Save child pointer before destruction
        BinaryNode* child = node->child_;

        // Detach child so destructor doesn't try to free it recursively
        node->child_ = nullptr;

        // Call destructor to clean up resources (std::string data_, etc.)
        node->~BinaryNode();

        // Return memory to the pool
        unused_.push(node);

        // Continue with child
        node = child;
    }
}

//=============================================================================
// DiskNodeFileReadHandle Implementation
//=============================================================================

DiskNodeFileReadHandle::DiskNodeFileReadHandle(
    const std::filesystem::path& path,
    const std::vector<std::string>& acceptable_identifiers) {
    
    #ifdef _WIN32
    file_ = _wfopen(path.c_str(), L"rb");
    #else
    file_ = std::fopen(path.c_str(), "rb");
    #endif

    if (!file_ || std::ferror(file_)) {
        error_code_ = NodeFileError::CouldNotOpen;
        return;
    }
    
    // Read identifier (first 4 bytes)
    char identifier[4];
    if (std::fread(identifier, 1, 4, file_) != 4) {
        std::fclose(file_);
        file_ = nullptr;
        error_code_ = NodeFileError::SyntaxError;
        return;
    }
    
    // Check if identifier is valid (0x00000000 is wildcard)
    if (identifier[0] != 0 || identifier[1] != 0 || 
        identifier[2] != 0 || identifier[3] != 0) {
        
        bool accepted = acceptable_identifiers.empty();
        for (const auto& valid_id : acceptable_identifiers) {
            if (valid_id.size() == 4 && std::memcmp(identifier, valid_id.c_str(), 4) == 0) {
                accepted = true;
                break;
            }
        }
        
        if (!accepted) {
            std::fclose(file_);
            file_ = nullptr;
            error_code_ = NodeFileError::InvalidIdentifier;
            return;
        }
    }
    
    // Get file size
    std::fseek(file_, 0, SEEK_END);
    file_size_ = std::ftell(file_);
    std::fseek(file_, 4, SEEK_SET);  // Position after identifier
}

DiskNodeFileReadHandle::~DiskNodeFileReadHandle() {
    close();
}

void DiskNodeFileReadHandle::close() {
    freeNode(root_node_);
    root_node_ = nullptr;
    file_size_ = 0;
    
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
    
    std::free(cache_);
    cache_ = nullptr;
}

size_t DiskNodeFileReadHandle::tell() const {
    if (file_) {
        return std::ftell(file_);
    }
    return 0;
}

bool DiskNodeFileReadHandle::isOk() const {
    return file_ != nullptr && error_code_ == NodeFileError::None && !std::ferror(file_);
}

bool DiskNodeFileReadHandle::renewCache() {
    if (!cache_) {
        cache_ = static_cast<uint8_t*>(std::malloc(cache_size_));
        if (!cache_) {
            spdlog::error("DiskNodeFileReadHandle: Failed to allocate {} bytes for cache", cache_size_);
            return false;
        }
    }
    
    cache_length_ = std::fread(cache_, 1, cache_size_, file_);
    
    if (cache_length_ == 0 || std::ferror(file_)) {
        return false;
    }
    
    local_read_index_ = 0;
    return true;
}

BinaryNode* DiskNodeFileReadHandle::getRootNode() {
    assert(root_node_ == nullptr);  // Should only be called once
    
    uint8_t first;
    if (std::fread(&first, 1, 1, file_) != 1) {
        error_code_ = NodeFileError::ReadError;
        return nullptr;
    }
    
    if (first == static_cast<uint8_t>(NodeMarker::Start)) {
        root_node_ = getAndLoadNode(nullptr);
        return root_node_;
    } else {
        error_code_ = NodeFileError::SyntaxError;
        return nullptr;
    }
}

//=============================================================================
// MemoryNodeFileReadHandle Implementation
//=============================================================================

MemoryNodeFileReadHandle::MemoryNodeFileReadHandle(const uint8_t* data, size_t size) {
    assign(data, size);
}

MemoryNodeFileReadHandle::~MemoryNodeFileReadHandle() {
    freeNode(root_node_);
}

void MemoryNodeFileReadHandle::assign(const uint8_t* data, size_t size) {
    freeNode(root_node_);
    root_node_ = nullptr;
    
    // We don't own this memory, but we need to cast for the cache pointer
    cache_ = const_cast<uint8_t*>(data);
    cache_size_ = size;
    cache_length_ = size;
    local_read_index_ = 0;
}

void MemoryNodeFileReadHandle::close() {
    assign(nullptr, 0);
}

bool MemoryNodeFileReadHandle::renewCache() {
    // Memory buffer doesn't renew - if we've read it all, we're done
    return false;
}

BinaryNode* MemoryNodeFileReadHandle::getRootNode() {
    assert(root_node_ == nullptr);
    
    local_read_index_++;  // Skip first NODE_START
    last_was_start_ = true;
    root_node_ = getAndLoadNode(nullptr);
    return root_node_;
}

} // namespace IO
} // namespace MapEditor
