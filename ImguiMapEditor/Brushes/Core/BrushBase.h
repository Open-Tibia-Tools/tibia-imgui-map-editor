#pragma once
#include "IBrush.h"
#include <string>
#include <utility>

namespace MapEditor::Brushes {

/**
 * Common base implementation for all brush types.
 * 
 * Provides shared fields and default implementations:
 * - name: Brush name for lookup and display
 * - lookId: Preview sprite ID
 * - draggable: Whether the brush supports drag-painting
 * 
 * Concrete brush types (RawBrush, etc.) inherit from this
 * and implement the pure virtual methods (draw, undraw, getType).
 */
class BrushBase : public IBrush {
public:
    /**
     * Construct a brush with the given properties.
     * 
     * @param name Brush name (used for lookup and display)
     * @param lookId Preview sprite ID
     * @param draggable Whether the brush supports drag-painting
     */
    BrushBase(std::string name, uint32_t lookId, bool draggable = true)
        : name_(std::move(name))
        , lookId_(lookId)
        , draggable_(draggable) 
    {}
    
    // ─── IBrush Implementation ────────────────────────────────────────────
    
    const std::string& getName() const override { return name_; }
    uint32_t getLookId() const override { return lookId_; }
    bool isDraggable() const override { return draggable_; }
    bool visibleInPalette() const override { return visible_; }
    void flagAsVisible() override { visible_ = true; }
    bool hasCollection() const override { return usesCollection_; }
    void setCollection() override { usesCollection_ = true; }
    BrushPreviewDescriptor getPreviewDescriptor() const override {
        if (previewDescriptor_.isExplicit()) {
            return previewDescriptor_;
        }
        return IBrush::getPreviewDescriptor();
    }

    void setPreviewDescriptor(BrushPreviewDescriptor descriptor) {
        previewDescriptor_ = std::move(descriptor);
    }
    
protected:
    std::string name_;
    uint32_t lookId_;
    bool draggable_;
    bool visible_ = false;
    bool usesCollection_ = false;
    BrushPreviewDescriptor previewDescriptor_;
};

} // namespace MapEditor::Brushes
