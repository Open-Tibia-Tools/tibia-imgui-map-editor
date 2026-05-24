#include "Rendering/Backend/RingBuffer.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace MapEditor {
namespace Rendering {

RingBuffer::~RingBuffer() {
    cleanup();
}

RingBuffer::RingBuffer(RingBuffer&& other) noexcept
    : buffer_(std::move(other.buffer_))
    , mapped_ptr_(other.mapped_ptr_)
    , element_size_(other.element_size_)
    , max_elements_(other.max_elements_)
    , section_size_(other.section_size_)
    , current_section_(other.current_section_)
    , use_persistent_mapping_(other.use_persistent_mapping_)
    , initialized_(other.initialized_)
{
    for (size_t i = 0; i < BUFFER_COUNT; ++i) {
        fences_[i] = std::move(other.fences_[i]);
    }
    other.mapped_ptr_ = nullptr;
    other.initialized_ = false;
}

RingBuffer& RingBuffer::operator=(RingBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();
        buffer_ = std::move(other.buffer_);
        mapped_ptr_ = other.mapped_ptr_;
        for (size_t i = 0; i < BUFFER_COUNT; ++i) {
            fences_[i] = std::move(other.fences_[i]);
        }
        element_size_ = other.element_size_;
        max_elements_ = other.max_elements_;
        section_size_ = other.section_size_;
        current_section_ = other.current_section_;
        use_persistent_mapping_ = other.use_persistent_mapping_;
        initialized_ = other.initialized_;
        
        other.mapped_ptr_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

bool RingBuffer::initialize(size_t element_size, size_t max_elements) {
    if (initialized_) {
        spdlog::warn("RingBuffer::initialize called on already-initialized buffer");
        return true;
    }

    element_size_ = element_size;
    max_elements_ = max_elements;
    section_size_ = element_size * max_elements;
    
    // Total buffer size: 3 sections
    size_t const total_size = section_size_ * BUFFER_COUNT;

    // Create buffer using RAII handle
    buffer_.create();
    if (!buffer_.isValid()) {
        spdlog::error("RingBuffer: Failed to generate buffer");
        return false;
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffer_.get());

    // GL 4.4+ persistent coherent mapping (guaranteed with GL 4.6 context)
    // No fallback needed - context creation already handles GL version requirements
    GLbitfield const storage_flags = GL_MAP_WRITE_BIT | 
                               GL_MAP_PERSISTENT_BIT | 
                               GL_MAP_COHERENT_BIT;

    glBufferStorage(GL_ARRAY_BUFFER, total_size, nullptr, storage_flags);

    GLbitfield const map_flags = GL_MAP_WRITE_BIT | 
                           GL_MAP_PERSISTENT_BIT | 
                           GL_MAP_COHERENT_BIT;

    mapped_ptr_ = glMapBufferRange(GL_ARRAY_BUFFER, 0, total_size, map_flags);

    if (!mapped_ptr_) {
        spdlog::error("RingBuffer: Persistent mapping failed");
        buffer_.reset();
        return false;
    }

    use_persistent_mapping_ = true;
    spdlog::info("RingBuffer: Using persistent mapping ({} bytes x {} sections)",
                section_size_, BUFFER_COUNT);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Fences are default constructed (null/invalid) - no explicit init needed
    
    initialized_ = true;
    return true;
}

void RingBuffer::cleanup() {
    if (!initialized_) return;

    // Reset fences (SyncHandle destructor handles glDeleteSync)
    for (size_t i = 0; i < BUFFER_COUNT; ++i) {
        fences_[i].reset();
    }

    // Unmap if persistently mapped
    if (mapped_ptr_ && use_persistent_mapping_) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer_.get());
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        mapped_ptr_ = nullptr;
    }

    // Reset buffer (DeferredVBOHandle destructor handles glDeleteBuffers)
    buffer_.reset();

    initialized_ = false;
}

void* RingBuffer::waitAndMap(size_t count) {
    if (!initialized_ || count > max_elements_) {
        spdlog::error("RingBuffer::waitAndMap: not initialized or count too large ({} > {})",
                      count, max_elements_);
        return nullptr;
    }

    // Wait for fence on current section if it exists
    if (fences_[current_section_]) {
        // Wait with timeout - should be very fast in practice
        GLenum const result = fences_[current_section_].clientWait(
            GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);  // 1 second timeout

        if (result == GL_TIMEOUT_EXPIRED || result == GL_WAIT_FAILED) {
            spdlog::warn("RingBuffer: Fence wait timeout/failed on section {} (result: {})",
                        current_section_, (result == GL_TIMEOUT_EXPIRED ? "GL_TIMEOUT_EXPIRED" : "GL_WAIT_FAILED"));

            // [UB FIX] Do NOT proceed if GPU is still reading!
            // Writing to mapped memory while GPU reads it is Undefined Behavior.
            // Return nullptr so the caller skips this batch.
            return nullptr;
        }

        // Reset the fence (SyncHandle handles cleanup)
        fences_[current_section_].reset();
    }

    // Return pointer to current section
    return static_cast<char*>(mapped_ptr_) + (current_section_ * section_size_);
}

void RingBuffer::finishWrite() {
    // Persistent mapping: buffer stays mapped, nothing to unmap
    // This is now a no-op but kept for API compatibility
}

void RingBuffer::signalFinished() {
    if (!initialized_) return;

    // Insert fence for this section (SyncHandle takes ownership)
    fences_[current_section_].reset(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0));

    // Advance to next section
    current_section_ = (current_section_ + 1) % BUFFER_COUNT;
}

size_t RingBuffer::getCurrentSectionOffset() const {
    return current_section_ * section_size_;
}

} // namespace Rendering
} // namespace MapEditor
