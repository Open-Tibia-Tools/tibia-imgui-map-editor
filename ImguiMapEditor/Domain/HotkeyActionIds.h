#pragma once

namespace MapEditor::Domain::Actions {

// File
inline constexpr const char* NEW_MAP   = "NEW";
inline constexpr const char* OPEN_MAP  = "OPEN";
inline constexpr const char* SAVE_AS   = "SAVE_AS";
inline constexpr const char* CLOSE_MAP = "CLOSE";

// Edit
inline constexpr const char* UNDO          = "UNDO";
inline constexpr const char* REDO          = "REDO";
inline constexpr const char* CUT           = "CUT";
inline constexpr const char* COPY          = "COPY";
inline constexpr const char* PASTE         = "PASTE";
inline constexpr const char* PASTE_REPLACE = "PASTE_REPLACE";
inline constexpr const char* DELETE_SEL    = "DELETE";
inline constexpr const char* SAVE          = "SAVE";

// View / Zoom
inline constexpr const char* ZOOM_IN         = "ZOOM_IN";
inline constexpr const char* ZOOM_OUT        = "ZOOM_OUT";
inline constexpr const char* ZOOM_RESET      = "ZOOM_RESET";
inline constexpr const char* SHOW_GRID       = "SHOW_GRID";
inline constexpr const char* GHOST_ITEMS     = "GHOST_ITEMS";
inline constexpr const char* GHOST_HIGHER    = "GHOST_HIGHER_FLOORS";
inline constexpr const char* GHOST_LOWER     = "GHOST_LOWER_FLOORS";
inline constexpr const char* SHOW_ALL_FLOORS = "SHOW_ALL_FLOORS";
inline constexpr const char* SHOW_SHADE      = "SHOW_SHADE";

// Overlays
inline constexpr const char* SHOW_SPAWNS      = "SHOW_SPAWNS";
inline constexpr const char* SHOW_CREATURES   = "SHOW_CREATURES";
inline constexpr const char* SHOW_BLOCKING    = "SHOW_BLOCKING";
inline constexpr const char* SHOW_SPECIAL     = "SHOW_SPECIAL";
inline constexpr const char* SHOW_HOUSES      = "SHOW_HOUSES";
inline constexpr const char* HIGHLIGHT_ITEMS  = "HIGHLIGHT_ITEMS";
inline constexpr const char* HIGHLIGHT_LOCKED = "HIGHLIGHT_LOCKED_DOORS";

// Preview
inline constexpr const char* SHOW_INGAME_BOX  = "SHOW_INGAME_BOX";
inline constexpr const char* SHOW_MINIMAP     = "SHOW_MINIMAP";
inline constexpr const char* SHOW_BROWSE_TILE = "SHOW_BROWSE_TILE";
inline constexpr const char* SHOW_TOOLTIPS    = "SHOW_TOOLTIPS";
inline constexpr const char* SHOW_PREVIEW     = "SHOW_PREVIEW";

// Navigation
inline constexpr const char* FLOOR_UP   = "FLOOR_UP";
inline constexpr const char* FLOOR_DOWN = "FLOOR_DOWN";

// Selection
inline constexpr const char* SELECT_ALL = "SELECT_ALL";
inline constexpr const char* DESELECT   = "DESELECT";

// Search
inline constexpr const char* QUICK_SEARCH    = "QUICK_SEARCH";
inline constexpr const char* ADVANCED_SEARCH = "ADVANCED_SEARCH";

// Map menu
inline constexpr const char* EDIT_TOWNS     = "EDIT_TOWNS";
inline constexpr const char* MAP_PROPERTIES = "MAP_PROPERTIES";

} // namespace MapEditor::Domain::Actions
