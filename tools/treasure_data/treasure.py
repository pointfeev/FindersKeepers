"""Treasure items: the items a destination container's mark follows.

Most containers clear once any item of their own loot list (the items it is always filled with, of some value) has been
taken: the player has found the place, and junk rolled on top never counts. Containers that hold quest items that
never refill (Brunswick's armour) list only those and clear once all of them are gone. The map table holds the lists;
this module reads what the game puts in each container, so validate can check them against the installed build.
"""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path

from .index import Levels, QuestLinker, load_presets, load_quest_graphs

_STASH_DEFAULT_ITEM_RE = re.compile(r"<AddStashDefaultItem\s.*?</AddStashDefaultItem>", re.DOTALL)
_ITEM_CLASS_RE = re.compile(r'<Constant Name="ItemClass" Value="([^"]+)"')
_STASHES_RE = re.compile(r'<Asset Name="Stashes" Alias="([^"]+)"')
_PRESET_RE = re.compile(r"<InventoryPreset\s([^>]*?)(?:/>|>(.*?)</InventoryPreset>)", re.DOTALL)
_PRESET_NAME_RE = re.compile(r'\bName="([^"]*)"')
_PRESET_MODE_RE = re.compile(r'\bMode="([^"]*)"')
_PRESET_ITEM_RE = re.compile(r'<PresetItem\s[^>]*Name="([^"]*)"')
_PRESET_REF_RE = re.compile(r'<InventoryPresetRef\s[^>]*Name="([^"]*)"')


def _item_elements(root: Path):
    for path in sorted((root / "tables" / "Libs" / "Tables" / "item").glob("item*.xml")):
        for element in ET.parse(path).getroot().iter():
            if element.tag != "ItemAlias" and element.get("Name") and element.get("Id"):
                yield element


def item_ids(root: Path) -> dict[str, str]:
    """Every item class's GUID (lowercase) by its name, from the item tables; aliases are left out."""
    ids: dict[str, str] = {}
    for element in _item_elements(root):
        ids.setdefault(element.get("Name", ""), element.get("Id", "").lower())
    return ids


def item_prices(root: Path) -> dict[str, int]:
    """Every item class's base price (tenths of a groschen) by its name; 0 for worthless filler such as cobwebs."""
    prices: dict[str, int] = {}
    for element in _item_elements(root):
        prices.setdefault(element.get("Name", ""), int(float(element.get("Price") or 0)))
    return prices


def guaranteed_items(root: Path) -> dict[str, set[str]]:
    """The items each inventory preset always gives, by preset name.

    A preset without a Mode gives every entry; a preset with a Mode picks among its entries (OneChild, AmountCount,
    AmountFraction), so its items count only when it has a single item and no sub-presets, the mode then deciding just
    the amount.
    """
    presets: dict[str, tuple[str, list[str], list[str]]] = {}
    for path in sorted((root / "tables" / "Libs" / "Tables" / "item").glob("InventoryPreset*.xml")):
        for match in _PRESET_RE.finditer(path.read_text(encoding="utf-8", errors="replace")):
            if name := _PRESET_NAME_RE.search(match.group(1)):
                mode = _PRESET_MODE_RE.search(match.group(1))
                body = match.group(2) or ""
                presets[name.group(1)] = (
                    mode.group(1) if mode else "",
                    _PRESET_ITEM_RE.findall(body),
                    _PRESET_REF_RE.findall(body),
                )

    def given(name: str, seen: frozenset[str]) -> set[str]:
        if name in seen:
            return set()
        mode, items, refs = presets.get(name, ("", [], []))
        if mode:
            return set(items) if len(items) == 1 and not refs else set()
        return set(items).union(*(given(ref, seen | {name}) for ref in refs))

    return {name: given(name, frozenset()) for name in presets}


def _preset_items(preset: str, items: dict[str, set[str]], refs: dict[str, set[str]]) -> set[str]:
    """The items a preset can give, through every preset it pulls in."""
    found: set[str] = set()
    seen: set[str] = set()
    frontier = {preset}
    while frontier:
        seen |= frontier
        for name in frontier:
            found |= items.get(name, set())
        frontier = {ref for name in frontier for ref in refs.get(name, set())} - seen
    return found


def container_items(root: Path, levels: Levels, ids: dict[str, str]) -> dict[tuple[str, str], set[str]]:
    """The item names each container can hold, by (level, entity id): its inventory preset's items, and the items
    quests add to it as stash default items (AddStashDefaultItem, bound to the container through the quest's asset
    alias)."""
    preset_items, preset_refs = load_presets(root)
    names: dict[tuple[str, str], set[str]] = defaultdict(set)
    for key, entity in levels.entities.items():
        if entity.preset:
            names[key] |= _preset_items(entity.preset, preset_items, preset_refs)
    by_guid = {guid: name for name, guid in ids.items()}
    graphs = load_quest_graphs(root)
    linker = QuestLinker(graphs, levels, root / "scripts" / "Quests" / "Final")
    for path, graph in graphs.items():
        if b"<addstashdefaultitem" not in graph.raw:
            continue
        bound = linker.bound(path)
        for node in _STASH_DEFAULT_ITEM_RE.findall(path.read_text(encoding="utf-8-sig", errors="replace")):
            item, stashes = _ITEM_CLASS_RE.search(node), _STASHES_RE.search(node)
            if not item or not stashes:
                continue
            name = by_guid.get(item.group(1).lower(), item.group(1).lower())
            for entity in bound:
                if entity["alias"] == stashes.group(1):
                    names[(entity["level"], entity["id"])].add(name)
    return names
