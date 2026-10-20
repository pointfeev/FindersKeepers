"""The table of maps the mod marks, generated from the curated map table as C++ data for the plugin."""

from __future__ import annotations

import json
import uuid
from pathlib import Path

# Maps that lead somewhere; "not_a_location_map" items do not.
MARKED_CATEGORIES = {"treasure", "quest", "area"}
# Sources marked as the map's own location: an object that holds the map until the player takes it. Quest rewards
# are left out (the quest leads there), and so are sources with no fixed object (random encounters, quest souls).
SOURCE_KINDS = {"chest", "body", "npc_loot", "npc_sale", "world_pickup"}


def cpp_string(text: str) -> str:
    if not text.isascii():
        raise SystemExit(f"non-ASCII text in the map table: {text!r}")
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cpp_place(place: dict | None) -> str:
    """A level entity: its EntityId and name (ids repeat across levels and names within one, so the plugin checks
    both), and whether it is marked only once the map's quest has ended; an empty place has id 0."""
    if place is None:
        return "{}"
    after_quest = ", true" if place.get("after_quest") else ""
    return f"{{{int(place['entity_id'])}, {cpp_string(place['entity_name'])}{after_quest}}}"


def cpp_item_class(guid: str) -> str:
    """A GUID string as the game keeps it in memory (CryGUID: the Windows GUID layout)."""
    return "{" + ", ".join(f"0x{byte:02X}" for byte in uuid.UUID(guid).bytes_le) + "}"


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


def cpp_optional_string(text: str | None) -> str:
    return cpp_string(text) if text else "nullptr"


def build(table_path: Path) -> str:
    """One MapTable::Map per map (plugin/src/probe/MapTable.h): how the player gets it (an item class carried, or a
    trigger examined, and the quest before which examining does not count), every place it leads to, where it is
    found, the tooltip labels of both, and the quest whose end retires it."""
    targets, maps = [], []
    for n, row in enumerate(marked_rows(table_path)):
        places = ", ".join(cpp_place(place) for place in [row, *row.get("extra_targets", [])])
        targets.append(f"inline constexpr Place kTargets{n}[] = {{{places}}};")
        examined = row.get("examined")
        source = map_source(row)
        maps.append(
            f"    {{.name = {cpp_string(row['map'])},\n"
            f"     .item = {'{}' if examined else cpp_item_class(row['guid'])},\n"
            f"     .seen = {cpp_place(examined)},\n"
            f"     .seenAfter = {cpp_optional_string((examined or {}).get('after_quest'))},\n"
            f"     .label = {cpp_string(tooltip_label(row))},\n"
            f"     .targets = kTargets{n},\n"
            f"     .source = {cpp_place(source)},\n"
            f"     .sourceLabel = {cpp_string(tooltip_label(row, source=True)) if source else 'nullptr'},\n"
            f"     .quest = {cpp_optional_string(row.get('quest'))}}},"
        )
    return (
        f"// Generated from {table_path.name} by treasure-data; do not edit.\n"
        + "\n".join(targets)
        + "\ninline constexpr Map kMaps[] = {\n"
        + "\n".join(maps)
        + "\n};\n"
    )
