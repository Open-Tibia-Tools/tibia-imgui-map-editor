# Brush + Tool Options Parity Execution Plan

Scope is limited to:
- Brush workflow parity
- Tool Options parity

Reference baseline:
- `RME_Readonly` wx implementation
- `ImguiMapEditor` implementation

## Phase 0 - Spec Freeze (Completed)

Parity matrix focus:
- Input modifiers: `Ctrl`, `Alt`, `Shift`, combinations
- Brush-family behavior: ground, wall, doodad, flag, eraser, spawn, door
- Tool Options visibility and ordering semantics
- `Preview Border` behavior parity
- Size model parity (discrete wx mapping)
- Tool icon/source parity

## Phase 1 - Input/Modifier Parity

Targets:
- `Ctrl+LMB` / `Ctrl+drag` erase routing through brush flow
- `Ctrl+Alt+LMB` smart brush pick precedence
- Restore `Shift+drag` brush draw mode without breaking selection drag when no brush

## Phase 2 - Family-Specific Brush Semantics

Targets:
- Wall special single-click only under wx-equivalent Alt gating
- Wall Alt stroke behavior parity
- Ground Alt replace lifecycle reset at stroke boundaries
- Doodad brush pipeline side effects parity

## Phase 3 - Tool Options State Model

Targets:
- Single source of truth for Tool Options state
- Palette/brush-family visibility mapping parity
- Door lock, spawn radius, and related option coupling parity

## Phase 4 - Tool Options UI/Preview/Icon Parity

Targets:
- Tool Options control order and sections parity
- `Preview Border` semantics aligned to wx behavior
- Discrete wx size stepping and shape coupling
- Tool icon source/order parity

## Phase 5 - Verification Gate

Required checks:
1. Build: `./build_ninja.bat`
2. Runtime smoke for brush actions:
   - `Ctrl` erase
   - `Ctrl+Alt` smart pick
   - `Shift+drag` brush mode
   - Alt ground/wall behavior
3. Tool Options parity checks:
   - section visibility/order
   - preview border behavior
   - size stepping behavior
4. Undo/redo grouping sanity checks for modified brush paths

## Progress Log

- [x] Phase 0 complete
- [x] Phase 1 complete
- [x] Phase 2 complete
- [x] Phase 3 complete
- [x] Phase 4 complete
- [x] Phase 5 complete
