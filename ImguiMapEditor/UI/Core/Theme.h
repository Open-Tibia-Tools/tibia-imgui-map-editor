#pragma once

#include <algorithm> // for ranges
#include <ranges>
#include <imgui.h>
#include "imgui_internal.h"
#include <string>
#include <iterator>


// ============================================================================
// Semantic Colors — universal across all themes
// These represent UI state meaning, visible regardless of background.
// ============================================================================
namespace SemanticColors {
    // State indicators
    inline constexpr ImVec4 MODIFIED     { 1.00f, 0.80f, 0.00f, 1.00f }; // Yellow — unsaved/dirty
    inline constexpr ImVec4 SAVED        { 0.20f, 0.80f, 0.20f, 1.00f }; // Green — clean/saved
    inline constexpr ImVec4 DANGER       { 0.90f, 0.30f, 0.30f, 1.00f }; // Red — error/delete
    inline constexpr ImVec4 WARNING      { 0.90f, 0.70f, 0.20f, 1.00f }; // Orange — caution
    inline constexpr ImVec4 INFO         { 0.20f, 0.50f, 0.90f, 1.00f }; // Blue — informational
    inline constexpr ImVec4 GOLD         { 1.00f, 0.80f, 0.00f, 1.00f }; // Gold — special/attention

    // Text roles
    inline constexpr ImVec4 LABEL        { 0.55f, 0.58f, 0.62f, 1.00f }; // Gray — label text in key/value pairs
    inline constexpr ImVec4 VALUE        { 0.95f, 0.95f, 0.95f, 1.00f }; // Near-white — value text
    inline constexpr ImVec4 EMPTY        { 0.40f, 0.42f, 0.45f, 1.00f }; // Dark gray — absent/empty/unknown
    inline constexpr ImVec4 HEADER_TEXT  { 0.85f, 0.88f, 0.92f, 1.00f }; // Bright — panel/section headers
    inline constexpr ImVec4 MUTED        { 0.67f, 0.70f, 0.75f, 1.00f }; // Medium gray — secondary info

    // Pulse/Glow base
    inline constexpr ImVec4 PULSE_BASE   { 0.60f, 0.50f, 0.00f, 1.00f }; // Yellow base for glow effects

    // Misc
    inline constexpr float DISABLED_ALPHA = 0.5f;
} // namespace SemanticColors

// ============================================================================
// Theme selection enum
// ============================================================================
enum class ThemeType {
    TibiaRPG = 0,
    ModernDark,
    ForestGreen,
    SunsetOrange,
    MidnightPurple,
    ClassicLight,
    OtclientTheme
};

// Theme information for UI display
struct ThemeInfo {
    ThemeType type;
    const char* name;
    const char* description;
    ImVec4 preview_color;
};

// Available themes
constexpr ThemeInfo AVAILABLE_THEMES[] = {
    { ThemeType::TibiaRPG, "Tibia RPG", "Warm parchment with wood and gold accents", {0.8f, 0.8f, 0.7f, 1.0f} },
    { ThemeType::ModernDark, "Modern Dark", "Sleek dark blue with professional look", {0.1f, 0.1f, 0.1f, 1.0f} },
    { ThemeType::ForestGreen, "Forest Green", "Nature-inspired dark green theme", {0.1f, 0.3f, 0.1f, 1.0f} },
    { ThemeType::SunsetOrange, "Sunset Orange", "Warm orange and brown tones", {0.9f, 0.6f, 0.2f, 1.0f} },
    { ThemeType::MidnightPurple, "Midnight Purple", "Deep purple with magenta accents", {0.2f, 0.1f, 0.3f, 1.0f} },
    { ThemeType::ClassicLight, "Classic Light", "Clean light gray theme", {0.9f, 0.9f, 0.9f, 1.0f} },
    { ThemeType::OtclientTheme, "Otclient", "Inspired by otclient", {0.2f, 0.2f, 0.2f, 1.0f} },
};

// Kept for backward compatibility, but calculation is now constexpr
constexpr size_t THEME_COUNT = std::size(AVAILABLE_THEMES);

// Convert theme enum to string
inline const char* GetThemeName(ThemeType type) {
    const auto it = std::ranges::find(AVAILABLE_THEMES, type, &ThemeInfo::type);
    if (it != std::end(AVAILABLE_THEMES)) {
        return it->name;
    }
    return "Unknown";
}

// Theme 1: Tibia RPG - Warm parchment aesthetic
inline void ApplyTibiaRPGTheme() {
ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // === GEOMETRIC STYLING ===
    // Apply slight rounding to mimic worn paper edges, keep scrollbars square for retro feel
    style.WindowRounding = 3.0f;     // Slightly rounded window corners
    style.FrameRounding = 2.0f;      // Subtle rounding for input frames
    style.GrabRounding = 1.0f;       // Minimal rounding for grab handles
    style.PopupRounding = 3.0f;      // Match window rounding for consistency
    style.ScrollbarRounding = 0.0f;  // Square scrollbars for retro aesthetic
    style.TabRounding = 2.0f;        // Gentle tab rounding
    style.WindowBorderSize = 1.0f;   // Distinct 1px borders for definition
    style.FrameBorderSize = 1.0f;    // Frame borders for clear input boundaries
    style.PopupBorderSize = 1.0f;    // Consistent popup borders
    
    // === COLOR DEFINITIONS ===
    // Converted from hex to normalized RGB (0.0-1.0) format
    
    // Primary Colors
    ImVec4 color_parchment      = ImVec4(0.949f, 0.886f, 0.807f, 1.000f); // #F2E2CE
    ImVec4 color_parchment_dark = ImVec4(0.847f, 0.792f, 0.718f, 1.000f); // #D8C8B7
    ImVec4 color_ink            = ImVec4(0.102f, 0.094f, 0.086f, 1.000f); // #1A1816
    ImVec4 color_ink_faded      = ImVec4(0.400f, 0.400f, 0.400f, 1.000f); // #666666
    ImVec4 color_stone          = ImVec4(0.400f, 0.400f, 0.400f, 1.000f); // #666666
    ImVec4 color_stone_dark     = ImVec4(0.300f, 0.300f, 0.300f, 1.000f); // #4D4D4D
    
    // Biome Accents
    ImVec4 color_desert_sand    = ImVec4(0.949f, 0.753f, 0.580f, 1.000f); // #F2C094
    ImVec4 color_desert_hover   = ImVec4(0.965f, 0.827f, 0.655f, 1.000f); // #F6D3A7
    ImVec4 color_desert_active  = ImVec4(0.878f, 0.690f, 0.502f, 1.000f); // #E0B080
    ImVec4 color_forest         = ImVec4(0.608f, 0.749f, 0.584f, 1.000f); // #9BBF95
    ImVec4 color_forest_hover   = ImVec4(0.675f, 0.820f, 0.651f, 1.000f); // #ACD1A6
    ImVec4 color_forest_active  = ImVec4(0.541f, 0.678f, 0.518f, 1.000f); // #8AAD84
    ImVec4 color_lime           = ImVec4(0.773f, 0.795f, 0.322f, 1.000f); // #C5CB52
    ImVec4 color_lime_hover     = ImVec4(0.835f, 0.855f, 0.384f, 1.000f); // #D5D962
    ImVec4 color_lime_active    = ImVec4(0.710f, 0.737f, 0.260f, 1.000f); // #B5BC42
    
    // Special Elements
    ImVec4 color_input_bg       = ImVec4(1.000f, 1.000f, 1.000f, 0.400f); // Semi-transparent white
    ImVec4 color_input_focus    = ImVec4(1.000f, 1.000f, 1.000f, 0.800f); // More opaque when focused
    ImVec4 color_selection      = ImVec4(0.608f, 0.749f, 0.584f, 0.500f); // Forest green with transparency
    ImVec4 color_border         = ImVec4(0.400f, 0.400f, 0.400f, 0.600f); // Semi-transparent stone
    
    // === TEXT COLORS ===
    colors[ImGuiCol_Text]                   = color_ink;              // Primary text - dark ink
    colors[ImGuiCol_TextDisabled]           = color_ink_faded;        // Disabled text - faded ink
    colors[ImGuiCol_TextSelectedBg]         = color_selection;        // Text selection highlight
    
    // === WINDOW & CONTAINER BACKGROUNDS ===
    colors[ImGuiCol_WindowBg]               = color_parchment;        // Main window background
    colors[ImGuiCol_ChildBg]                = color_parchment;        // Child window background
    colors[ImGuiCol_PopupBg]                = color_parchment;        // Popup background
    colors[ImGuiCol_MenuBarBg]              = color_parchment_dark;   // Menu bar background
    
    // === BORDERS & SHADOWS ===
    colors[ImGuiCol_Border]                 = color_border;           // Standard borders
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // No shadow
    
    // === TITLE BARS & HEADERS ===
    colors[ImGuiCol_TitleBg]                = ImVec4(0.400f, 0.400f, 0.400f, 0.600f); // Stone with transparency
    colors[ImGuiCol_TitleBgActive]          = color_stone;            // Active title bar - solid stone
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.400f, 0.400f, 0.400f, 0.400f); // Collapsed - more transparent
    
    // Header elements (for tree nodes, collapsible headers)
    colors[ImGuiCol_Header]                 = color_forest;           // Header background
    colors[ImGuiCol_HeaderHovered]          = color_forest_hover;     // Header hover
    colors[ImGuiCol_HeaderActive]           = color_forest_active;    // Header active/clicked
    
    // === TABS ===
    colors[ImGuiCol_Tab]                    = ImVec4(0.949f, 0.753f, 0.580f, 0.600f); // Desert sand with transparency
    colors[ImGuiCol_TabHovered]             = color_forest_hover;     // Tab hover - forest green
    colors[ImGuiCol_TabActive]              = color_forest;           // Active tab - forest green
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.949f, 0.753f, 0.580f, 0.400f); // Unfocused tab - more transparent
    colors[ImGuiCol_TabUnfocusedActive]     = color_parchment_dark;   // Unfocused but active tab
    
    // === BUTTONS ===
    colors[ImGuiCol_Button]                 = color_desert_sand;      // Primary buttons - desert sand
    colors[ImGuiCol_ButtonHovered]          = color_desert_hover;     // Button hover - lighter sand
    colors[ImGuiCol_ButtonActive]           = color_desert_active;    // Button active - darker sand
    
    // === INPUT CONTROLS ===
    colors[ImGuiCol_FrameBg]                = color_input_bg;         // Input frame background
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(1.0f, 1.0f, 1.0f, 0.600f); // Frame hover
    colors[ImGuiCol_FrameBgActive]          = color_input_focus;      // Frame focused
    
    // Checkmarks and radio buttons
    colors[ImGuiCol_CheckMark]              = color_ink;              // Checkmark - dark ink (also used for radio buttons)
    
    // Sliders and progress bars
    colors[ImGuiCol_SliderGrab]             = color_stone;            // Slider grab - stone grey
    colors[ImGuiCol_SliderGrabActive]       = color_forest;           // Slider grab active - forest green
    // Progress bars use FrameBg for background and CheckMark for fill
    // Note: ProgressBarBg and ProgressBar don't exist in standard ImGui
    
    // === SCROLLBARS ===
    colors[ImGuiCol_ScrollbarBg]            = color_parchment_dark;   // Scrollbar background
    colors[ImGuiCol_ScrollbarGrab]          = color_stone;            // Scrollbar grab
    colors[ImGuiCol_ScrollbarGrabHovered]   = color_stone_dark;       // Scrollbar grab hover
    colors[ImGuiCol_ScrollbarGrabActive]    = color_ink_faded;        // Scrollbar grab active
    
    // === RESIZE & SEPARATION ===
    colors[ImGuiCol_ResizeGrip]             = color_stone;            // Window resize grip
    colors[ImGuiCol_ResizeGripHovered]      = color_lime_hover;       // Resize grip hover - lime
    colors[ImGuiCol_ResizeGripActive]       = color_lime_active;      // Resize grip active - lime
    
    // Separators
    colors[ImGuiCol_Separator]              = color_border;           // Separator lines
    colors[ImGuiCol_SeparatorHovered]       = color_forest_hover;     // Separator hover
    colors[ImGuiCol_SeparatorActive]        = color_forest;           // Separator active
    
    // === PLOTS & GRAPHS ===
    colors[ImGuiCol_PlotLines]              = color_ink_faded;        // Graph lines - faded ink
    colors[ImGuiCol_PlotLinesHovered]       = color_lime;             // Graph lines hover - lime
    colors[ImGuiCol_PlotHistogram]          = color_forest;           // Histogram bars - forest green
    colors[ImGuiCol_PlotHistogramHovered]   = color_lime;             // Histogram hover - lime
    
    // === NAVIGATION & DRAG&DROP ===
    colors[ImGuiCol_NavHighlight]           = color_lime;             // Navigation highlight - lime
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.0f, 1.0f, 1.0f, 0.700f); // Windowing highlight
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.450f); // Windowing dim
    
    colors[ImGuiCol_DragDropTarget]         = color_forest;           // Drag & drop target
}

// Theme 2: Modern Dark - Sleek dark blue theme
inline void ApplyModernDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Geometry
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);

    ImVec4* colors = style.Colors;

    // Color palette - Deep navy and steel blue
    ImVec4 bg_dark          = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    ImVec4 bg_medium        = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    ImVec4 bg_light         = ImVec4(0.24f, 0.26f, 0.30f, 1.00f);
    ImVec4 bg_lighter       = ImVec4(0.30f, 0.32f, 0.36f, 1.00f);
    ImVec4 accent_blue      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    ImVec4 accent_hover     = ImVec4(0.36f, 0.69f, 1.00f, 1.00f);
    ImVec4 accent_active    = ImVec4(0.16f, 0.49f, 0.88f, 1.00f);
    ImVec4 text_bright      = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    ImVec4 text_dim         = ImVec4(0.60f, 0.62f, 0.65f, 1.00f);

    colors[ImGuiCol_Text]                  = text_bright;
    colors[ImGuiCol_TextDisabled]          = text_dim;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_WindowBg]              = bg_dark;
    colors[ImGuiCol_ChildBg]               = bg_dark;
    colors[ImGuiCol_PopupBg]               = bg_medium;
    colors[ImGuiCol_Border]                = ImVec4(0.35f, 0.37f, 0.40f, 0.80f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TitleBg]               = bg_medium;
    colors[ImGuiCol_TitleBgActive]         = bg_light;
    colors[ImGuiCol_TitleBgCollapsed]      = bg_medium;
    colors[ImGuiCol_FrameBg]               = bg_medium;
    colors[ImGuiCol_FrameBgHovered]        = bg_light;
    colors[ImGuiCol_FrameBgActive]         = bg_lighter;
    colors[ImGuiCol_Button]                = bg_medium;
    colors[ImGuiCol_ButtonHovered]         = bg_light;
    colors[ImGuiCol_ButtonActive]          = bg_lighter;
    colors[ImGuiCol_ScrollbarBg]           = bg_dark;
    colors[ImGuiCol_ScrollbarGrab]         = bg_light;
    colors[ImGuiCol_ScrollbarGrabHovered]  = bg_lighter;
    colors[ImGuiCol_ScrollbarGrabActive]   = accent_blue;
    colors[ImGuiCol_CheckMark]             = accent_blue;
    colors[ImGuiCol_SliderGrab]            = accent_blue;
    colors[ImGuiCol_SliderGrabActive]      = accent_active;
    colors[ImGuiCol_Separator]             = ImVec4(0.35f, 0.37f, 0.40f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = accent_hover;
    colors[ImGuiCol_SeparatorActive]       = accent_active;
    colors[ImGuiCol_ResizeGrip]            = accent_blue;
    colors[ImGuiCol_ResizeGripHovered]     = accent_hover;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;
    colors[ImGuiCol_Tab]                   = bg_medium;
    colors[ImGuiCol_TabHovered]            = bg_light;
    colors[ImGuiCol_TabActive]             = bg_lighter;
    colors[ImGuiCol_TabUnfocused]          = bg_medium;
    colors[ImGuiCol_TabUnfocusedActive]    = bg_light;
    colors[ImGuiCol_MenuBarBg]             = bg_medium;
    colors[ImGuiCol_Header]                = bg_medium;
    colors[ImGuiCol_HeaderHovered]         = bg_light;
    colors[ImGuiCol_HeaderActive]          = bg_lighter;
    colors[ImGuiCol_TableHeaderBg]         = bg_medium;
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.35f, 0.37f, 0.40f, 0.80f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.25f, 0.27f, 0.30f, 0.50f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.30f, 0.32f, 0.36f, 0.15f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.26f, 0.59f, 0.98f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = accent_blue;
    colors[ImGuiCol_NavWindowingHighlight] = accent_hover;
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.10f, 0.10f, 0.10f, 0.35f);
}

// Theme 3: Forest Green - Nature-inspired dark theme
inline void ApplyForestGreenTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Geometry
    style.WindowRounding    = 5.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding       = 3.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);

    ImVec4* colors = style.Colors;

    // Color palette - Dark forest and moss green
    ImVec4 bg_forest        = ImVec4(0.10f, 0.15f, 0.12f, 1.00f);
    ImVec4 bg_moss          = ImVec4(0.15f, 0.22f, 0.18f, 1.00f);
    ImVec4 bg_lichen        = ImVec4(0.20f, 0.28f, 0.24f, 1.00f);
    ImVec4 bg_leaf          = ImVec4(0.25f, 0.35f, 0.30f, 1.00f);
    ImVec4 accent_lime      = ImVec4(0.60f, 0.85f, 0.20f, 1.00f);
    ImVec4 accent_hover     = ImVec4(0.70f, 0.95f, 0.30f, 1.00f);
    ImVec4 accent_active    = ImVec4(0.50f, 0.75f, 0.10f, 1.00f);
    ImVec4 text_light       = ImVec4(0.90f, 0.95f, 0.90f, 1.00f);
    ImVec4 text_dim         = ImVec4(0.55f, 0.65f, 0.55f, 1.00f);

    colors[ImGuiCol_Text]                  = text_light;
    colors[ImGuiCol_TextDisabled]          = text_dim;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.60f, 0.85f, 0.20f, 0.35f);
    colors[ImGuiCol_WindowBg]              = bg_forest;
    colors[ImGuiCol_ChildBg]               = bg_forest;
    colors[ImGuiCol_PopupBg]               = bg_moss;
    colors[ImGuiCol_Border]                = ImVec4(0.30f, 0.42f, 0.35f, 0.80f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TitleBg]               = bg_moss;
    colors[ImGuiCol_TitleBgActive]         = bg_lichen;
    colors[ImGuiCol_TitleBgCollapsed]      = bg_moss;
    colors[ImGuiCol_FrameBg]               = bg_moss;
    colors[ImGuiCol_FrameBgHovered]        = bg_lichen;
    colors[ImGuiCol_FrameBgActive]         = bg_leaf;
    colors[ImGuiCol_Button]                = bg_moss;
    colors[ImGuiCol_ButtonHovered]         = bg_lichen;
    colors[ImGuiCol_ButtonActive]          = bg_leaf;
    colors[ImGuiCol_ScrollbarBg]           = bg_forest;
    colors[ImGuiCol_ScrollbarGrab]         = bg_lichen;
    colors[ImGuiCol_ScrollbarGrabHovered]  = bg_leaf;
    colors[ImGuiCol_ScrollbarGrabActive]   = accent_lime;
    colors[ImGuiCol_CheckMark]             = accent_lime;
    colors[ImGuiCol_SliderGrab]            = accent_lime;
    colors[ImGuiCol_SliderGrabActive]      = accent_active;
    colors[ImGuiCol_Separator]             = ImVec4(0.30f, 0.42f, 0.35f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = accent_hover;
    colors[ImGuiCol_SeparatorActive]       = accent_active;
    colors[ImGuiCol_ResizeGrip]            = accent_lime;
    colors[ImGuiCol_ResizeGripHovered]     = accent_hover;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;
    colors[ImGuiCol_Tab]                   = bg_moss;
    colors[ImGuiCol_TabHovered]            = bg_lichen;
    colors[ImGuiCol_TabActive]             = bg_leaf;
    colors[ImGuiCol_TabUnfocused]          = bg_moss;
    colors[ImGuiCol_TabUnfocusedActive]    = bg_lichen;
    colors[ImGuiCol_MenuBarBg]             = bg_moss;
    colors[ImGuiCol_Header]                = bg_moss;
    colors[ImGuiCol_HeaderHovered]         = bg_lichen;
    colors[ImGuiCol_HeaderActive]          = bg_leaf;
    colors[ImGuiCol_TableHeaderBg]         = bg_moss;
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.30f, 0.42f, 0.35f, 0.80f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.32f, 0.25f, 0.50f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.25f, 0.35f, 0.30f, 0.15f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.60f, 0.85f, 0.20f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = accent_lime;
    colors[ImGuiCol_NavWindowingHighlight] = accent_hover;
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.05f, 0.10f, 0.05f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.05f, 0.10f, 0.05f, 0.35f);
}

// Theme 4: Sunset Orange - Warm orange and brown theme
inline void ApplySunsetOrangeTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Geometry
    style.WindowRounding    = 5.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding       = 3.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);

    ImVec4* colors = style.Colors;

    // Color palette - Warm sunset colors
    ImVec4 bg_dusk          = ImVec4(0.18f, 0.12f, 0.10f, 1.00f);
    ImVec4 bg_terracotta    = ImVec4(0.28f, 0.18f, 0.14f, 1.00f);
    ImVec4 bg_clay          = ImVec4(0.38f, 0.24f, 0.18f, 1.00f);
    ImVec4 bg_sand          = ImVec4(0.48f, 0.32f, 0.24f, 1.00f);
    ImVec4 accent_orange    = ImVec4(1.00f, 0.55f, 0.20f, 1.00f);
    ImVec4 accent_hover     = ImVec4(1.00f, 0.65f, 0.30f, 1.00f);
    ImVec4 accent_active    = ImVec4(0.90f, 0.45f, 0.10f, 1.00f);
    ImVec4 text_cream       = ImVec4(0.95f, 0.92f, 0.88f, 1.00f);
    ImVec4 text_dim         = ImVec4(0.65f, 0.58f, 0.52f, 1.00f);

    colors[ImGuiCol_Text]                  = text_cream;
    colors[ImGuiCol_TextDisabled]          = text_dim;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(1.00f, 0.55f, 0.20f, 0.35f);
    colors[ImGuiCol_WindowBg]              = bg_dusk;
    colors[ImGuiCol_ChildBg]               = bg_dusk;
    colors[ImGuiCol_PopupBg]               = bg_terracotta;
    colors[ImGuiCol_Border]                = ImVec4(0.48f, 0.32f, 0.24f, 0.80f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TitleBg]               = bg_terracotta;
    colors[ImGuiCol_TitleBgActive]         = bg_clay;
    colors[ImGuiCol_TitleBgCollapsed]      = bg_terracotta;
    colors[ImGuiCol_FrameBg]               = bg_terracotta;
    colors[ImGuiCol_FrameBgHovered]        = bg_clay;
    colors[ImGuiCol_FrameBgActive]         = bg_sand;
    colors[ImGuiCol_Button]                = bg_terracotta;
    colors[ImGuiCol_ButtonHovered]         = bg_clay;
    colors[ImGuiCol_ButtonActive]          = bg_sand;
    colors[ImGuiCol_ScrollbarBg]           = bg_dusk;
    colors[ImGuiCol_ScrollbarGrab]         = bg_clay;
    colors[ImGuiCol_ScrollbarGrabHovered]  = bg_sand;
    colors[ImGuiCol_ScrollbarGrabActive]   = accent_orange;
    colors[ImGuiCol_CheckMark]             = accent_orange;
    colors[ImGuiCol_SliderGrab]            = accent_orange;
    colors[ImGuiCol_SliderGrabActive]      = accent_active;
    colors[ImGuiCol_Separator]             = ImVec4(0.48f, 0.32f, 0.24f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = accent_hover;
    colors[ImGuiCol_SeparatorActive]       = accent_active;
    colors[ImGuiCol_ResizeGrip]            = accent_orange;
    colors[ImGuiCol_ResizeGripHovered]     = accent_hover;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;
    colors[ImGuiCol_Tab]                   = bg_terracotta;
    colors[ImGuiCol_TabHovered]            = bg_clay;
    colors[ImGuiCol_TabActive]             = bg_sand;
    colors[ImGuiCol_TabUnfocused]          = bg_terracotta;
    colors[ImGuiCol_TabUnfocusedActive]    = bg_clay;
    colors[ImGuiCol_MenuBarBg]             = bg_terracotta;
    colors[ImGuiCol_Header]                = bg_terracotta;
    colors[ImGuiCol_HeaderHovered]         = bg_clay;
    colors[ImGuiCol_HeaderActive]          = bg_sand;
    colors[ImGuiCol_TableHeaderBg]         = bg_terracotta;
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.48f, 0.32f, 0.24f, 0.80f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.38f, 0.24f, 0.18f, 0.50f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.38f, 0.24f, 0.18f, 0.15f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 0.55f, 0.20f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = accent_orange;
    colors[ImGuiCol_NavWindowingHighlight] = accent_hover;
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.18f, 0.10f, 0.08f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.10f, 0.06f, 0.04f, 0.35f);
}

// Theme 5: Midnight Purple - Deep purple with magenta accents
inline void ApplyMidnightPurpleTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Geometry
    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);

    ImVec4* colors = style.Colors;

    // Color palette - Deep purple and magenta
    ImVec4 bg_midnight      = ImVec4(0.12f, 0.10f, 0.18f, 1.00f);
    ImVec4 bg_violet        = ImVec4(0.18f, 0.14f, 0.26f, 1.00f);
    ImVec4 bg_plum          = ImVec4(0.24f, 0.18f, 0.34f, 1.00f);
    ImVec4 bg_lavender      = ImVec4(0.32f, 0.24f, 0.42f, 1.00f);
    ImVec4 accent_magenta   = ImVec4(0.90f, 0.35f, 0.85f, 1.00f);
    ImVec4 accent_hover     = ImVec4(1.00f, 0.45f, 0.95f, 1.00f);
    ImVec4 accent_active    = ImVec4(0.80f, 0.25f, 0.75f, 1.00f);
    ImVec4 text_pearl       = ImVec4(0.95f, 0.93f, 0.98f, 1.00f);
    ImVec4 text_dim         = ImVec4(0.62f, 0.58f, 0.70f, 1.00f);

    colors[ImGuiCol_Text]                  = text_pearl;
    colors[ImGuiCol_TextDisabled]          = text_dim;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.90f, 0.35f, 0.85f, 0.35f);
    colors[ImGuiCol_WindowBg]              = bg_midnight;
    colors[ImGuiCol_ChildBg]               = bg_midnight;
    colors[ImGuiCol_PopupBg]               = bg_violet;
    colors[ImGuiCol_Border]                = ImVec4(0.42f, 0.34f, 0.52f, 0.80f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TitleBg]               = bg_violet;
    colors[ImGuiCol_TitleBgActive]         = bg_plum;
    colors[ImGuiCol_TitleBgCollapsed]      = bg_violet;
    colors[ImGuiCol_FrameBg]               = bg_violet;
    colors[ImGuiCol_FrameBgHovered]        = bg_plum;
    colors[ImGuiCol_FrameBgActive]         = bg_lavender;
    colors[ImGuiCol_Button]                = bg_violet;
    colors[ImGuiCol_ButtonHovered]         = bg_plum;
    colors[ImGuiCol_ButtonActive]          = bg_lavender;
    colors[ImGuiCol_ScrollbarBg]           = bg_midnight;
    colors[ImGuiCol_ScrollbarGrab]         = bg_plum;
    colors[ImGuiCol_ScrollbarGrabHovered]  = bg_lavender;
    colors[ImGuiCol_ScrollbarGrabActive]   = accent_magenta;
    colors[ImGuiCol_CheckMark]             = accent_magenta;
    colors[ImGuiCol_SliderGrab]            = accent_magenta;
    colors[ImGuiCol_SliderGrabActive]      = accent_active;
    colors[ImGuiCol_Separator]             = ImVec4(0.42f, 0.34f, 0.52f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = accent_hover;
    colors[ImGuiCol_SeparatorActive]       = accent_active;
    colors[ImGuiCol_ResizeGrip]            = accent_magenta;
    colors[ImGuiCol_ResizeGripHovered]     = accent_hover;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;
    colors[ImGuiCol_Tab]                   = bg_violet;
    colors[ImGuiCol_TabHovered]            = bg_plum;
    colors[ImGuiCol_TabActive]             = bg_lavender;
    colors[ImGuiCol_TabUnfocused]          = bg_violet;
    colors[ImGuiCol_TabUnfocusedActive]    = bg_plum;
    colors[ImGuiCol_MenuBarBg]             = bg_violet;
    colors[ImGuiCol_Header]                = bg_violet;
    colors[ImGuiCol_HeaderHovered]         = bg_plum;
    colors[ImGuiCol_HeaderActive]          = bg_lavender;
    colors[ImGuiCol_TableHeaderBg]         = bg_violet;
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.42f, 0.34f, 0.52f, 0.80f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.32f, 0.24f, 0.42f, 0.50f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.24f, 0.18f, 0.34f, 0.15f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.90f, 0.35f, 0.85f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = accent_magenta;
    colors[ImGuiCol_NavWindowingHighlight] = accent_hover;
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.12f, 0.08f, 0.18f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.08f, 0.06f, 0.12f, 0.35f);
}

// Theme 6: Classic Light - Clean light gray theme
inline void ApplyClassicLightTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Geometry
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding       = 3.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);

    ImVec4* colors = style.Colors;

    // Color palette - Clean light grays and blue accent
    ImVec4 bg_white         = ImVec4(0.96f, 0.97f, 0.98f, 1.00f);
    ImVec4 bg_light         = ImVec4(0.90f, 0.91f, 0.92f, 1.00f);
    ImVec4 bg_medium        = ImVec4(0.82f, 0.84f, 0.85f, 1.00f);
    ImVec4 bg_dark          = ImVec4(0.72f, 0.74f, 0.76f, 1.00f);
    ImVec4 accent_blue      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    ImVec4 accent_hover     = ImVec4(0.36f, 0.69f, 1.00f, 1.00f);
    ImVec4 accent_active    = ImVec4(0.16f, 0.49f, 0.88f, 1.00f);
    ImVec4 text_dark        = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
    ImVec4 text_dim         = ImVec4(0.50f, 0.52f, 0.54f, 1.00f);

    colors[ImGuiCol_Text]                  = text_dark;
    colors[ImGuiCol_TextDisabled]          = text_dim;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_WindowBg]              = bg_white;
    colors[ImGuiCol_ChildBg]               = bg_white;
    colors[ImGuiCol_PopupBg]               = bg_white;
    colors[ImGuiCol_Border]                = ImVec4(0.72f, 0.74f, 0.76f, 0.80f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TitleBg]               = bg_light;
    colors[ImGuiCol_TitleBgActive]         = bg_medium;
    colors[ImGuiCol_TitleBgCollapsed]      = bg_light;
    colors[ImGuiCol_FrameBg]               = bg_light;
    colors[ImGuiCol_FrameBgHovered]        = bg_medium;
    colors[ImGuiCol_FrameBgActive]         = bg_dark;
    colors[ImGuiCol_Button]                = bg_light;
    colors[ImGuiCol_ButtonHovered]         = bg_medium;
    colors[ImGuiCol_ButtonActive]          = bg_dark;
    colors[ImGuiCol_ScrollbarBg]           = bg_white;
    colors[ImGuiCol_ScrollbarGrab]         = bg_medium;
    colors[ImGuiCol_ScrollbarGrabHovered]  = bg_dark;
    colors[ImGuiCol_ScrollbarGrabActive]   = accent_blue;
    colors[ImGuiCol_CheckMark]             = accent_blue;
    colors[ImGuiCol_SliderGrab]            = accent_blue;
    colors[ImGuiCol_SliderGrabActive]      = accent_active;
    colors[ImGuiCol_Separator]             = ImVec4(0.72f, 0.74f, 0.76f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = accent_hover;
    colors[ImGuiCol_SeparatorActive]       = accent_active;
    colors[ImGuiCol_ResizeGrip]            = accent_blue;
    colors[ImGuiCol_ResizeGripHovered]     = accent_hover;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;
    colors[ImGuiCol_Tab]                   = bg_light;
    colors[ImGuiCol_TabHovered]            = bg_medium;
    colors[ImGuiCol_TabActive]             = bg_dark;
    colors[ImGuiCol_TabUnfocused]          = bg_light;
    colors[ImGuiCol_TabUnfocusedActive]    = bg_medium;
    colors[ImGuiCol_MenuBarBg]             = bg_light;
    colors[ImGuiCol_Header]                = bg_light;
    colors[ImGuiCol_HeaderHovered]         = bg_medium;
    colors[ImGuiCol_HeaderActive]          = bg_dark;
    colors[ImGuiCol_TableHeaderBg]         = bg_light;
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.72f, 0.74f, 0.76f, 0.80f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.82f, 0.84f, 0.85f, 0.50f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.90f, 0.91f, 0.92f, 0.30f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.26f, 0.59f, 0.98f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = accent_blue;
    colors[ImGuiCol_NavWindowingHighlight] = accent_hover;
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.60f, 0.60f, 0.60f, 0.35f);
}

// Forward declaration
inline void ApplyOtclientTheme();

// Main function to apply theme by type
inline void ApplyTheme(ThemeType type) {
    switch (type) {
        case ThemeType::TibiaRPG:
            ApplyTibiaRPGTheme();
            break;
        case ThemeType::ModernDark:
            ApplyModernDarkTheme();
            break;
        case ThemeType::ForestGreen:
            ApplyForestGreenTheme();
            break;
        case ThemeType::SunsetOrange:
            ApplySunsetOrangeTheme();
            break;
        case ThemeType::MidnightPurple:
            ApplyMidnightPurpleTheme();
            break;
        case ThemeType::ClassicLight:
            ApplyClassicLightTheme();
            break;
        default:
            ApplyOtclientTheme(); // 
            break;
    }
}

inline void ApplyOtclientTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // === Style variables (rounding, spacing, etc.) ===
        style.WindowPadding            = ImVec2(8, 8);
        style.FramePadding             = ImVec2(6, 4);
        style.CellPadding              = ImVec2(4, 2);
        style.ItemSpacing              = ImVec2(8, 6);
        style.ItemInnerSpacing         = ImVec2(4, 4);
        style.TouchExtraPadding        = ImVec2(0, 0);
        style.IndentSpacing            = 21.0f;
        style.ScrollbarSize            = 14.0f;
        style.GrabMinSize              = 10.0f;

        style.WindowRounding           = 5.0f;
        style.ChildRounding            = 5.0f;
        style.FrameRounding            = 4.0f;
        style.PopupRounding            = 4.0f;
        style.ScrollbarRounding        = 9.0f;
        style.GrabRounding             = 3.0f;
        style.TabRounding              = 4.0f;
        style.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.WindowBorderSize         = 1.0f;
        style.ChildBorderSize          = 1.0f;
        style.PopupBorderSize          = 1.0f;
        style.FrameBorderSize          = 0.0f;
        style.TabBorderSize            = 0.0f;

        // === All 56+ colors — nothing is left default ===
        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.200f, 0.200f, 0.200f, 1.00f); // #333333
        colors[ImGuiCol_ChildBg]                = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.180f, 0.180f, 0.180f, 1.00f);
        colors[ImGuiCol_Border]                 = ImVec4(0.400f, 0.400f, 0.400f, 1.00f); // #666666
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        colors[ImGuiCol_FrameBg]                = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.240f, 0.240f, 0.240f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.300f, 0.300f, 0.300f, 1.00f);

        colors[ImGuiCol_TitleBg]                = ImVec4(0.180f, 0.180f, 0.180f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.200f, 0.200f, 0.200f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.000f, 0.000f, 0.000f, 0.510f);

        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.160f, 0.160f, 0.160f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.100f, 0.100f, 0.100f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);

        colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

        colors[ImGuiCol_Button]                 = ImVec4(0.250f, 0.250f, 0.250f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.330f, 0.330f, 0.330f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.400f, 0.400f, 0.400f, 1.00f);

        colors[ImGuiCol_Header]                 = ImVec4(0.250f, 0.250f, 0.250f, 1.00f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.350f, 0.350f, 0.350f, 1.00f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.420f, 0.420f, 0.420f, 1.00f);

        colors[ImGuiCol_Separator]              = ImVec4(0.400f, 0.400f, 0.400f, 1.00f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.26f, 0.26f, 0.40f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.45f, 0.45f, 0.45f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.65f, 0.65f, 0.65f, 0.95f);

        colors[ImGuiCol_Tab]                    = ImVec4(0.180f, 0.180f, 0.180f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);

        colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
        colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.80f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.80f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.90f, 0.00f, 1.00f);

        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);

        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
    }
