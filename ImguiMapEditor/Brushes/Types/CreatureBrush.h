#pragma once
#include "Brushes/Core/IBrush.h"
#include "Domain/Outfit.h"
#include <string>

namespace MapEditor::Brushes {

    class CreatureBrush : public IBrush {
    public:
        CreatureBrush(const std::string& name, const Domain::Outfit& outfit);
        ~CreatureBrush() override = default;

        // IBrush interface
        const std::string& getName() const override { return name_; }
        BrushType getType() const override { return BrushType::Creature; }
        uint32_t getLookId() const override { return static_cast<uint32_t>(outfit_.lookType); } // Used for preview
        BrushPreviewDescriptor getPreviewDescriptor() const override {
            return BrushPreviewDescriptor::creature(outfit_);
        }
        
        void draw(Domain::ChunkedMap& map, Domain::Tile* tile, const DrawContext& ctx) override;
        void undraw(Domain::ChunkedMap& map, Domain::Tile* tile) override;

        const Domain::Outfit& getOutfit() const { return outfit_; }

    private:
        std::string name_;
        Domain::Outfit outfit_;
    };

} // namespace MapEditor::Brushes
