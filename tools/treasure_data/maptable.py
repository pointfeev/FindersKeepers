"""The Lua table of maps the mod marks, generated from the curated map table into the mod's .pak."""

from __future__ import annotations

import json
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

from .game import read_member

PAK_PATH = "Scripts/Mods/treasurefinder_maps.lua"
# Maps that lead somewhere; "not_a_location_map" items do not.
MARKED_CATEGORIES = {"treasure", "quest", "area"}
ITEM_TABLES = "Libs/Tables/item/item"


def lua_string(text: str) -> str:
    if not text.isascii():
        raise SystemExit(f"non-ASCII text in the map table: {text!r}")
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def read_ui_names(game_dir: Path) -> dict[str, str]:
    """Item GUID -> its UIName localization label, from the install's item tables."""
    labels = {}
    with zipfile.ZipFile(game_dir / "Data" / "Tables.pak") as pak:
        for info in pak.infolist():
            if info.filename.startswith(ITEM_TABLES) and info.filename.endswith(".xml"):
                for doc in ET.fromstring(read_member(pak, info)).iter("Document"):
                    if doc.get("Id") and doc.get("UIName"):
                        labels[doc.get("Id").lower()] = doc.get("UIName")
    return labels


def build(table_path: Path, game_dir: Path) -> bytes:
    """One entry per map whose destination is a known level entity: the map item's class and name label, the
    destination's level, entity id and name (ids repeat across levels and names within one, so the probe checks
    both)."""
    maps = json.loads(table_path.read_text(encoding="utf-8"))
    labels = read_ui_names(game_dir)
    rows = []
    for item in maps:
        if item["category"] not in MARKED_CATEGORIES or item["confidence"] == "unresolved" or not item["entity_id"]:
            continue
        label = labels.get(item["guid"].lower())
        if not label:
            raise SystemExit(f"{item['map']}: no UIName for item {item['guid']} in this install's item tables")
        rows.append(
            f"\t{{ map = {lua_string(item['map'])}, item = {lua_string(item['guid'])}, label = {lua_string(label)}, "
            f"level = {lua_string(item['level'])}, entity = {int(item['entity_id'])}, "
            f"name = {lua_string(item['entity_name'])} }},"
        )
    lua = (
        f"-- Generated from {table_path.name} by treasure-data; do not edit.\n"
        "TreasureFinder.Maps = {\n" + "\n".join(rows) + "\n}\n"
    )
    return lua.encode("utf-8")
