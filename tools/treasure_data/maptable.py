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


def marked_rows(table_path: Path) -> list[dict]:
    """The rows the mod marks: maps that lead somewhere, resolved to at least one level entity."""
    rows = json.loads(table_path.read_text(encoding="utf-8"))
    return [
        row
        for row in rows
        if row["category"] in MARKED_CATEGORIES and row["confidence"] != "unresolved" and row["entity_id"]
    ]


def tooltip_label(row: dict) -> str:
    """The localization label of a map's tooltip text, built by localization.build."""
    return f"ui_treasurefinder_map_{row['map']}"


def build(table_path: Path) -> bytes:
    """One entry per map: how the player gets it (an item class carried, or a trigger examined), its tooltip label,
    and every place it leads to, each a level entity id and name (ids repeat across levels and names within one, so
    the probe checks both)."""
    entries = []
    for row in marked_rows(table_path):
        if row.get("examined"):
            source = f"seen = {lua_string(row['examined']['entity_name'])}"
        else:
            source = f"item = {lua_string(row['guid'])}"
        places = [row, *row.get("extra_targets", [])]
        targets = ", ".join(
            f"{{ entity = {int(place['entity_id'])}, name = {lua_string(place['entity_name'])} }}" for place in places
        )
        entries.append(
            f"\t{{ map = {lua_string(row['map'])}, {source}, label = {lua_string(tooltip_label(row))}, "
            f"level = {lua_string(row['level'])},\n\t\ttargets = {{ {targets} }} }},"
        )
    lua = (
        f"-- Generated from {table_path.name} by treasure-data; do not edit.\n"
        "TreasureFinder.Maps = {\n" + "\n".join(entries) + "\n}\n"
    )
    return lua.encode("utf-8")
