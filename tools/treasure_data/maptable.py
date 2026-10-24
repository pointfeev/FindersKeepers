"""The table of maps the mod marks, generated from the curated map table as C++ data for the plugin."""

from __future__ import annotations

import json
import uuid
from pathlib import Path

# Maps that lead to something to find. Area maps (hunting grounds) and "not_a_location_map" items do not.
MARKED_CATEGORIES = {"treasure", "quest", "riddle"}
# Sources marked as the map's own location: an object that holds the map until the player takes it. Quest rewards
# are left out (the quest leads there), and so are sources with no fixed object (random encounters, quest souls).
SOURCE_KINDS = {"chest", "body", "npc_loot", "npc_sale", "world_pickup"}


def cpp_string(text: str) -> str:
    if not text.isascii():
        raise SystemExit(f"non-ASCII text in the map table: {text!r}")
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cpp_entity_guid(guid: str) -> str:
    """A level EntityGuid "AAAAAAAA-BBBB-CCCC" as the 64-bit value the game parses it into (0xCCCCBBBBAAAAAAAA)."""
    low, middle, high = (int(part, 16) for part in guid.split("-"))
    return f"0x{high:04X}{middle:04X}{low:08X}ull"


def cpp_place(place: dict | None, treasure: str | None = None, any_taken: bool = False) -> str:
    """A level entity as a MapTable::Place, or an empty one (GUID 0).

    The plugin finds it by its level GUID, which a patch that renumbers EntityIds leaves alone, and reads it by its
    kind (a hole, an item slot, a container and so on). A place can also be marked only once the map's quest has
    ended, and a place holding items names the array of them and whether taking any of them clears it (any_taken) or
    only taking all of them (all_taken).
    """
    if place is None:
        return "{}"
    # In MapTable::Place's declaration order, as designated initializers require.
    fields = []
    if treasure:
        fields.append(f".treasure = {treasure}")
    fields.append(f".guid = {cpp_entity_guid(place['entity_guid'])}")
    fields.append(f".kind = Kind::{''.join(word.capitalize() for word in place['kind'].split('_'))}")
    if place.get("after_quest"):
        fields.append(".afterQuest = true")
    if treasure and any_taken:
        fields.append(".anyTaken = true")
    return "{" + ", ".join(fields) + "}"


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
    return f"ui_finderskeepers_{'mapsource' if source else 'map'}_{row['map']}"


def cpp_optional_string(text: str | None) -> str:
    return cpp_string(text) if text else "nullptr"


def build(table_path: Path) -> str:
    """The plugin's map table: one MapTable::Map per map (plugin/src/probe/MapTable.h).

    Each map has how the player gets it (an item class to carry, or a trigger to examine, which may only count after
    a quest), every place it leads to (a container with its treasure items), where it is found, the tooltip labels
    for both, and the quest whose end retires it.
    """
    targets, maps = [], []
    for n, row in enumerate(marked_rows(table_path)):
        places = []
        for k, place in enumerate([row, *row.get("extra_targets", [])]):
            treasure = None
            if place.get("treasure"):
                treasure = f"kTreasure{n}_{k}"
                items = ", ".join(cpp_item_class(entry["guid"]) for entry in place["treasure"])
                targets.append(f"inline constexpr Game::ItemClassId {treasure}[] = {{{items}}};")
            places.append(cpp_place(place, treasure, place.get("clears_when") == "any_taken"))
        targets.append(f"inline constexpr Place kTargets{n}[] = {{{', '.join(places)}}};")
        examined = row.get("examined")
        source = map_source(row)
        # A carried map's source holds the map the way a container holds its treasure: until the player takes it.
        source_items = None
        if source and not examined:
            source_items = f"kSourceItem{n}"
            targets.append(f"inline constexpr Game::ItemClassId {source_items}[] = {{{cpp_item_class(row['guid'])}}};")
        maps.append(
            f"    {{.name = {cpp_string(row['map'])},\n"
            f"     .item = {'{}' if examined else cpp_item_class(row['guid'])},\n"
            f"     .seen = {cpp_place(examined)},\n"
            f"     .seenAfter = {cpp_optional_string((examined or {}).get('after_quest'))},\n"
            f"     .label = {cpp_string(tooltip_label(row))},\n"
            f"     .targets = kTargets{n},\n"
            f"     .source = {cpp_place(source, source_items, any_taken=True)},\n"
            f"     .sourceLabel = {cpp_string(tooltip_label(row, source=True)) if source else 'nullptr'},\n"
            f"     .quest = {cpp_optional_string(row.get('quest'))},\n"
            f"     .reveals = {cpp_item_class(row['reveals']) if row.get('reveals') else '{}'},\n"
            f"     .category = Category::{row['category'].capitalize()}}},"
        )
    return (
        f"// Generated from {table_path.name} by treasure-data; do not edit.\n"
        + "\n".join(targets)
        + "\ninline constexpr Map kMaps[] = {\n"
        + "\n".join(maps)
        + "\n};\n"
    )
