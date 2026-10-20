"""The Lua table of maps the mod marks, generated from the curated map table into the mod's .pak."""

from __future__ import annotations

import json
from pathlib import Path

PAK_PATH = "Scripts/Mods/treasurefinder_maps.lua"
# Maps that lead somewhere; "not_a_location_map" items do not.
MARKED_CATEGORIES = {"treasure", "quest", "area"}
# Sources marked as the map's own location: an object that holds the map until the player takes it. Quest rewards
# are left out (the quest leads there), and so are sources with no fixed object (random encounters, quest souls).
SOURCE_KINDS = {"chest", "body", "npc_loot", "npc_sale", "world_pickup"}


def lua_string(text: str) -> str:
    if not text.isascii():
        raise SystemExit(f"non-ASCII text in the map table: {text!r}")
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def lua_entity(place: dict) -> str:
    return f"{{ entity = {int(place['entity_id'])}, name = {lua_string(place['entity_name'])} }}"


def marked_rows(table_path: Path) -> list[dict]:
    """The rows the mod marks: maps that lead somewhere, resolved to at least one level entity."""
    rows = json.loads(table_path.read_text(encoding="utf-8"))
    return [
        row
        for row in rows
        if row["category"] in MARKED_CATEGORIES and row["confidence"] != "unresolved" and row["entity_id"]
    ]


def map_source(row: dict) -> dict | None:
    """Where the map itself is found, as a level entity: the trigger of a map the player examines, or the object
    that holds a carried map until the player takes it."""
    if row.get("examined"):
        return row["examined"]
    source = row.get("source") or {}
    return source if source.get("how") in SOURCE_KINDS and source.get("entity_id") else None


def tooltip_label(row: dict, source: bool = False) -> str:
    """The localization label of a map's tooltip text, on its destinations or on its own location; built by
    localization.build."""
    return f"ui_treasurefinder_{'mapsource' if source else 'map'}_{row['map']}"


def build(table_path: Path) -> bytes:
    """One entry per map: how the player gets it (an item class carried, or a trigger examined), every place it leads
    to and where it is found, each a level entity id and name (ids repeat across levels and names within one, so the
    probe checks both), and the tooltip labels of both."""
    entries = []
    for row in marked_rows(table_path):
        if row.get("examined"):
            how = f"seen = {lua_string(row['examined']['entity_name'])}"
        else:
            how = f"item = {lua_string(row['guid'])}"
        targets = ", ".join(lua_entity(place) for place in [row, *row.get("extra_targets", [])])
        found = ""
        if (where := map_source(row)) is not None:
            found = f",\n\t\tsource = {lua_entity(where)}, sourceLabel = {lua_string(tooltip_label(row, source=True))}"
        entries.append(
            f"\t{{ map = {lua_string(row['map'])}, {how}, label = {lua_string(tooltip_label(row))}, "
            f"level = {lua_string(row['level'])},\n\t\ttargets = {{ {targets} }}{found} }},"
        )
    lua = (
        f"-- Generated from {table_path.name} by treasure-data; do not edit.\n"
        "TreasureFinder.Maps = {\n" + "\n".join(entries) + "\n}\n"
    )
    return lua.encode("utf-8")
