"""The Lua table of maps the mod marks, generated from the curated map table into the mod's .pak."""

from __future__ import annotations

import json
from pathlib import Path

PAK_PATH = "Scripts/Mods/treasurefinder_maps.lua"
# Maps that lead somewhere; "not_a_location_map" items do not.
MARKED_CATEGORIES = {"treasure", "quest", "area"}


def lua_string(text: str) -> str:
    if not text.isascii():
        raise SystemExit(f"non-ASCII text in the map table: {text!r}")
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def build(table_path: Path) -> bytes:
    """One entry per map whose destination is a known level entity: the map item's class, the destination's level,
    entity id and name (ids repeat across levels and names within one, so the probe checks both)."""
    maps = json.loads(table_path.read_text(encoding="utf-8"))
    rows = []
    for item in maps:
        if item["category"] not in MARKED_CATEGORIES or item["confidence"] == "unresolved" or not item["entity_id"]:
            continue
        rows.append(
            f"\t{{ map = {lua_string(item['map'])}, item = {lua_string(item['guid'])}, "
            f"level = {lua_string(item['level'])}, entity = {int(item['entity_id'])}, "
            f"name = {lua_string(item['entity_name'])} }},"
        )
    lua = (
        f"-- Generated from {table_path.name} by treasure-data; do not edit.\n"
        "TreasureFinder.Maps = {\n" + "\n".join(rows) + "\n}\n"
    )
    return lua.encode("utf-8")
