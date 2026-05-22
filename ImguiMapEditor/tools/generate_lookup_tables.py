#!/usr/bin/env python3
"""
Generate brush lookup tables from the wx RME Redux reference contracts.

This script vendors the reference table logic into this repo so the checked-in
`.inc` files can be regenerated deterministically instead of being hand-edited.
"""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "Services" / "Brushes"


EDGE_NONE = 0
EDGE_N = 1
EDGE_E = 2
EDGE_S = 3
EDGE_W = 4
EDGE_CNW = 5
EDGE_CNE = 6
EDGE_CSW = 7
EDGE_CSE = 8
EDGE_DNW = 9
EDGE_DNE = 10
EDGE_DSE = 11
EDGE_DSW = 12
EDGE_CENTER = 13

WALL_POLE = 0
WALL_SOUTH_END = 1
WALL_EAST_END = 2
WALL_NORTHWEST_DIAGONAL = 3
WALL_WEST_END = 4
WALL_NORTHEAST_DIAGONAL = 5
WALL_HORIZONTAL = 6
WALL_SOUTH_T = 7
WALL_NORTH_END = 8
WALL_VERTICAL = 9
WALL_SOUTHWEST_DIAGONAL = 10
WALL_EAST_T = 11
WALL_SOUTHEAST_DIAGONAL = 12
WALL_WEST_T = 13
WALL_NORTH_T = 14
WALL_INTERSECTION = 15
WALL_UNTOUCHABLE = 16

TABLE_NORTH_END = 0
TABLE_SOUTH_END = 1
TABLE_EAST_END = 2
TABLE_WEST_END = 3
TABLE_HORIZONTAL = 4
TABLE_VERTICAL = 5
TABLE_ALONE = 6

NW = 1
N = 2
NE = 4
W = 8
E = 16
SW = 32
S = 64
SE = 128

WN = 1
WW = 2
WE = 4
WS = 8


def pack(*values: int) -> int:
    result = 0
    for index, value in enumerate(values[:4]):
        result |= value << (index * 8)
    return result


def generate_ground_table() -> list[int]:
    table = [0] * 256
    for i in range(256):
        values: list[int] = []
        has_n = bool(i & N)
        has_s = bool(i & S)
        has_e = bool(i & E)
        has_w = bool(i & W)

        nw_d = has_n and has_w and not has_s and not has_e
        ne_d = has_n and has_e and not has_s and not has_w
        sw_d = has_s and has_w and not has_n and not has_e
        se_d = has_s and has_e and not has_n and not has_w

        used_n = used_s = used_e = used_w = False

        if nw_d:
            values.append(EDGE_DNW)
            used_n = used_w = True
        if ne_d:
            values.append(EDGE_DNE)
            used_n = used_e = True
        if sw_d:
            values.append(EDGE_DSW)
            used_s = used_w = True
        if se_d:
            values.append(EDGE_DSE)
            used_s = used_e = True

        if has_n and not used_n:
            values.append(EDGE_N)
        if has_s and not used_s:
            values.append(EDGE_S)
        if has_e and not used_e:
            values.append(EDGE_E)
        if has_w and not used_w:
            values.append(EDGE_W)

        if (i & NW) and not has_n and not has_w:
            values.append(EDGE_CNW)
        if (i & NE) and not has_n and not has_e:
            values.append(EDGE_CNE)
        if (i & SW) and not has_s and not has_w:
            values.append(EDGE_CSW)
        if (i & SE) and not has_s and not has_e:
            values.append(EDGE_CSE)

        table[i] = pack(*values)
    return table


def generate_wall_tables() -> tuple[list[int], list[int]]:
    full = [0] * 16
    full[0] = WALL_POLE
    full[WN] = WALL_SOUTH_END
    full[WW] = WALL_EAST_END
    full[WN | WW] = WALL_NORTHWEST_DIAGONAL
    full[WE] = WALL_WEST_END
    full[WN | WE] = WALL_NORTHEAST_DIAGONAL
    full[WW | WE] = WALL_HORIZONTAL
    full[WN | WW | WE] = WALL_SOUTH_T
    full[WS] = WALL_NORTH_END
    full[WN | WS] = WALL_VERTICAL
    full[WW | WS] = WALL_SOUTHWEST_DIAGONAL
    full[WN | WW | WS] = WALL_EAST_T
    full[WE | WS] = WALL_SOUTHEAST_DIAGONAL
    full[WN | WE | WS] = WALL_WEST_T
    full[WW | WE | WS] = WALL_NORTH_T
    full[WN | WW | WE | WS] = WALL_INTERSECTION

    half = [0] * 16
    for i in range(16):
        bits = i & (WN | WW)
        if bits == (WN | WW):
            half[i] = WALL_NORTHWEST_DIAGONAL
        elif bits == WN:
            half[i] = WALL_VERTICAL
        elif bits == WW:
            half[i] = WALL_HORIZONTAL
        else:
            half[i] = WALL_POLE

    return full, half


def generate_table_table() -> list[int]:
    table = [TABLE_ALONE] * 256
    for i in range(256):
        has_n = bool(i & N)
        has_s = bool(i & S)
        has_e = bool(i & E)
        has_w = bool(i & W)

        if has_n and has_s and not has_e and not has_w:
            table[i] = TABLE_VERTICAL
        elif has_e and has_w and not has_n and not has_s:
            table[i] = TABLE_HORIZONTAL
        elif has_n and not has_s and not has_e and not has_w:
            table[i] = TABLE_SOUTH_END
        elif has_s and not has_n and not has_e and not has_w:
            table[i] = TABLE_NORTH_END
        elif has_e and not has_w and not has_n and not has_s:
            table[i] = TABLE_WEST_END
        elif has_w and not has_e and not has_n and not has_s:
            table[i] = TABLE_EAST_END
        else:
            table[i] = TABLE_ALONE
    return table


def generate_carpet_table() -> list[int]:
    table = [0] * 256
    for i in range(256):
        nw = bool(i & NW)
        n = bool(i & N)
        ne = bool(i & NE)
        w = bool(i & W)
        e = bool(i & E)
        sw = bool(i & SW)
        s = bool(i & S)
        se = bool(i & SE)

        if n and s and e and w:
            missing_diag = int(not nw) + int(not ne) + int(not sw) + int(not se)
            if missing_diag == 1:
                if not nw:
                    table[i] = EDGE_DSE
                elif not ne:
                    table[i] = EDGE_DSW
                elif not sw:
                    table[i] = EDGE_DNE
                else:
                    table[i] = EDGE_DNW
            else:
                table[i] = EDGE_CENTER
            continue

        if n and s and w:
            if sw and nw:
                table[i] = EDGE_W
            elif sw:
                table[i] = EDGE_CSW
            elif nw:
                table[i] = EDGE_CNW
            else:
                table[i] = EDGE_W
            continue

        if n and s and e:
            table[i] = EDGE_E
            continue

        if n and w and e:
            if sw:
                table[i] = EDGE_CNW
            else:
                table[i] = EDGE_N
            continue

        if s and w and e:
            table[i] = EDGE_S
            continue

        if n and w:
            table[i] = EDGE_CNW
            continue
        if n and e:
            table[i] = EDGE_CNE
            continue
        if s and w:
            table[i] = EDGE_CSW
            continue
        if s and e:
            table[i] = EDGE_CSE
            continue

        if n and s:
            if nw and sw:
                table[i] = EDGE_W
            elif nw:
                table[i] = EDGE_CNW
            elif sw:
                table[i] = EDGE_CSW
            elif ne:
                table[i] = EDGE_CNE
            elif se:
                table[i] = EDGE_CSE
            else:
                table[i] = EDGE_CENTER
            continue

        if w and e:
            n_side = nw or ne
            s_side = sw or se
            if sw and e and w:
                table[i] = EDGE_CSW
            elif n_side and s_side:
                table[i] = EDGE_CENTER
            elif n_side:
                table[i] = EDGE_N
            elif s_side:
                table[i] = EDGE_S
            else:
                table[i] = EDGE_CENTER
            continue

        if n:
            if nw:
                table[i] = EDGE_CNW
            elif ne:
                table[i] = EDGE_CNE
            elif sw:
                table[i] = EDGE_CSW
            elif se:
                table[i] = EDGE_CSE
            else:
                table[i] = EDGE_CENTER
            continue

        if s:
            if sw:
                table[i] = EDGE_CSW
            elif se:
                table[i] = EDGE_CSE
            elif nw:
                table[i] = EDGE_CNW
            elif ne:
                table[i] = EDGE_CNE
            else:
                table[i] = EDGE_CSW
            continue

        if w:
            if nw:
                table[i] = EDGE_W
            elif sw:
                table[i] = EDGE_CSW
            elif se:
                table[i] = EDGE_S
            else:
                table[i] = EDGE_CENTER
            continue

        if e:
            if nw:
                table[i] = EDGE_CNE
            elif ne:
                table[i] = EDGE_CNE
            elif se:
                table[i] = EDGE_S
            else:
                table[i] = EDGE_CENTER
            continue

        if nw and ne:
            table[i] = EDGE_N
        elif sw and se:
            table[i] = EDGE_S
        elif nw and sw:
            table[i] = EDGE_W
        elif ne and se:
            table[i] = EDGE_E
        elif ne:
            table[i] = EDGE_CNE
        elif se:
            table[i] = EDGE_CSE
        elif sw:
            table[i] = EDGE_CSW
        else:
            table[i] = EDGE_CENTER

    return table


def emit_border_table(table: list[int]) -> str:
    lines = [
        "// AUTO-GENERATED from wx RME reference logic - DO NOT EDIT MANUALLY",
        "// Generated by tools/generate_lookup_tables.py",
        "",
        "void BorderLookupService::initializeTable() {",
        "    table_.fill(0);",
    ]
    for index, value in enumerate(table):
        if value:
            lines.append(f"    table_[{index}] = 0x{value:08X}U;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def emit_carpet_table(table: list[int]) -> str:
    lines = [
        "// AUTO-GENERATED from wx RME reference logic - DO NOT EDIT MANUALLY",
        "// Generated by tools/generate_lookup_tables.py",
        "",
        "void CarpetLookupService::initializeTable() {",
        "    table_.fill(0);",
    ]
    for index, value in enumerate(table):
        if value:
            lines.append(f"    table_[{index}] = 0x{value:08X}U;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def emit_wall_tables(full: list[int], half: list[int]) -> str:
    lines = [
        "// AUTO-GENERATED from wx RME reference logic - DO NOT EDIT MANUALLY",
        "// Generated by tools/generate_lookup_tables.py",
        "",
        "void WallLookupService::initializeTable() {",
        "    fullTable_.fill(static_cast<WallAlign>(0));",
    ]
    for index, value in enumerate(full):
        lines.append(f"    fullTable_[{index}] = static_cast<WallAlign>({value});")
    lines.append("")
    lines.append("    halfTable_.fill(static_cast<WallAlign>(0));")
    for index, value in enumerate(half):
        lines.append(f"    halfTable_[{index}] = static_cast<WallAlign>({value});")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def emit_table_table(table: list[int]) -> str:
    lines = [
        "// AUTO-GENERATED from wx RME reference logic - DO NOT EDIT MANUALLY",
        "// Generated by tools/generate_lookup_tables.py",
        "",
        "void TableLookupService::initializeTable() {",
        f"    table_.fill(static_cast<TableAlign>({TABLE_ALONE}));",
    ]
    for index, value in enumerate(table):
        lines.append(f"    table_[{index}] = static_cast<TableAlign>({value});")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def write_file(path: Path, contents: str) -> None:
    path.write_text(contents, encoding="utf-8", newline="\n")
    print(f"wrote {path.relative_to(ROOT)}")


def main() -> None:
    write_file(OUT_DIR / "BorderLookupTable.inc", emit_border_table(generate_ground_table()))
    full_wall, half_wall = generate_wall_tables()
    write_file(OUT_DIR / "WallLookupTable.inc", emit_wall_tables(full_wall, half_wall))
    write_file(OUT_DIR / "CarpetLookupTable.inc", emit_carpet_table(generate_carpet_table()))
    write_file(OUT_DIR / "TableLookupTable.inc", emit_table_table(generate_table_table()))


if __name__ == "__main__":
    main()
