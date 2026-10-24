"""Build the map, container and POI index from extracted game files.

Map items come from the item tables (documents of class 4, "map"). Where a quest graph names a map's GUID and declares
assets, the SmartObjectHolder entity of that quest in the level binds each asset alias to an entity; following that
binding is the only map-to-destination link the game data holds. Everything else is matched by hand.
"""

from __future__ import annotations

import csv
import json
import os
import re
import xml.etree.ElementTree as ET
from collections import defaultdict
from dataclasses import asdict, dataclass, field
from pathlib import Path

from .game import LEVELS

MAP_DOCUMENT_CLASS = "4"
CONTAINER_CLASSES = {"Stash", "Hole", "SO_DiggingSpot", "StashCorpse", "CartStash", "DestroStash"}
# A marked place's kind (MapTable::Kind), by its level entity's class: what the plugin reads to tell whether the place
# is dealt with, or whether it still holds its map.
PLACE_KINDS = {
    "Stash": "container",
    "StashCorpse": "container",
    "Nest": "container",
    "Hole": "hole",
    "ItemSlot": "item_slot",
    "NPC": "person",
    "NPC_Female": "person",
    "InteractionTrigger": "trigger",
    "TriggerArea": "area",
    "AreaShape": "area",
    "TagPoint": "area",
}

_ENTITY_RE = re.compile(r"<Entity\s([^>]*?)\s*(?:/>|>(.*?)</Entity>)", re.DOTALL)
_POI_RE = re.compile(r'<Object Type="POI" ([^>]*)/>')
_ATTR_RE = re.compile(r'(\w+)="([^"]*)"')
_ASSET_LINK_RE = re.compile(r'<Link TargetId="(\d+)"[^>]*Name="asset\[&apos;([^&]*)&apos;\]"')
_PRESET_RE = re.compile(r'sInventoryPreset="([^"]*)"')
_INVENTORY_PRESET_RE = re.compile(
    r'<InventoryPreset\s[^>]*Name="([^"]*)"[^>]*?(?:/>|>(.*?)</InventoryPreset>)', re.DOTALL
)
_PRESET_ITEM_RE = re.compile(r'<PresetItem\s[^>]*Name="([^"]*)"')
_PRESET_REF_RE = re.compile(r'<InventoryPresetRef\s[^>]*Name="([^"]*)"')


@dataclass
class Entity:
    level: str
    cls: str
    id: str
    guid: str
    name: str
    pos: str
    layer: str
    preset: str
    asset_links: list[tuple[str, str]] = field(default_factory=list)


@dataclass
class Poi:
    level: str
    name: str
    pos: str
    label: str
    poi_type: str


@dataclass
class Levels:
    entities: dict[tuple[str, str], Entity]
    pois: list[Poi]

    def by_name(self) -> dict[tuple[str, str], list[Entity]]:
        index: dict[tuple[str, str], list[Entity]] = defaultdict(list)
        for entity in self.entities.values():
            index[(entity.level, entity.name)].append(entity)
        return index

    def by_guid(self) -> dict[tuple[str, str], list[Entity]]:
        index: dict[tuple[str, str], list[Entity]] = defaultdict(list)
        for entity in self.entities.values():
            if entity.guid:
                index[(entity.level, entity.guid)].append(entity)
        return index


def load_strings(root: Path) -> dict[str, str]:
    """English UI strings keyed by lowercase string id; the third cell is the shipped text when present."""
    strings: dict[str, str] = {}
    for path in sorted((root / "localization").glob("*.xml")):
        for row in ET.parse(path).getroot().iter("Row"):
            cells = [cell.text or "" for cell in row.findall("Cell")]
            if len(cells) >= 2:
                strings[cells[0].lower()] = cells[2] if len(cells) > 2 and cells[2] else cells[1]
    return strings


def load_map_items(root: Path, strings: dict[str, str]) -> dict[str, dict]:
    items: dict[str, dict] = {}
    for path in sorted((root / "tables" / "Libs" / "Tables" / "item").glob("item*.xml")):
        for doc in ET.parse(path).iter("Document"):
            if doc.get("Type") != MAP_DOCUMENT_CLASS:
                continue
            guid = doc.get("Id", "").lower()
            items[guid] = {
                "name": doc.get("Name"),
                "guid": guid,
                "table": path.name,
                "quest_item": doc.get("IsQuestItem") == "true",
                "en_name": strings.get((doc.get("UIName") or "").lower(), ""),
                "en_text": strings.get((doc.get("UIInfo") or "").lower(), ""),
                "images": [image.get("Image") for image in doc.iter("Image")],
            }
    return items


def load_presets(root: Path) -> tuple[dict[str, set[str]], dict[str, set[str]]]:
    """Inventory presets: items each one lists directly, and presets each one pulls in by reference."""
    items: dict[str, set[str]] = defaultdict(set)
    refs: dict[str, set[str]] = defaultdict(set)
    # Some preset files are not well-formed XML, so they are scanned rather than parsed.
    for path in sorted((root / "tables" / "Libs" / "Tables" / "item").glob("InventoryPreset*.xml")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in _INVENTORY_PRESET_RE.finditer(text):
            body = match.group(2) or ""
            items[match.group(1)].update(_PRESET_ITEM_RE.findall(body))
            refs[match.group(1)].update(_PRESET_REF_RE.findall(body))
    return items, refs


def presets_containing(item: str, items: dict[str, set[str]], refs: dict[str, set[str]]) -> set[str]:
    found = {preset for preset, listed in items.items() if item in listed}
    frontier = set(found)
    while frontier:
        frontier = {preset for preset, pulled in refs.items() if pulled & frontier} - found
        found |= frontier
    return found


def load_levels(root: Path, strings: dict[str, str]) -> Levels:
    entities: dict[tuple[str, str], Entity] = {}
    pois: list[Poi] = []
    for level in LEVELS:
        level_dir = root / "levels" / level
        for path in [level_dir / "objects_mission0.xml", *sorted((level_dir / "layers").glob("*.xml"))]:
            text = path.read_text(encoding="utf-8", errors="replace")
            for match in _ENTITY_RE.finditer(text):
                attrs = dict(_ATTR_RE.findall(match.group(1)))
                if "EntityId" not in attrs:
                    continue
                body = match.group(2) or ""
                preset = _PRESET_RE.search(body)
                entity = Entity(
                    level=level,
                    cls=attrs.get("EntityClass", ""),
                    id=attrs["EntityId"],
                    guid=attrs.get("EntityGuid", ""),
                    name=attrs.get("Name", ""),
                    pos=attrs.get("Pos", ""),
                    layer=attrs.get("EditorLayer", ""),
                    preset=preset.group(1) if preset else "",
                    asset_links=_ASSET_LINK_RE.findall(body),
                )
                entities[(level, entity.id)] = entity
            for match in _POI_RE.finditer(text):
                attrs: dict[str, str] = dict(_ATTR_RE.findall(match.group(1)))
                label = attrs.get("CustomLabel", "")
                pois.append(
                    Poi(
                        level=level,
                        name=attrs.get("Name", ""),
                        pos=attrs.get("Pos", ""),
                        label=strings.get(label.lstrip("@").lower(), label),
                        poi_type=attrs.get("POITypeId", ""),
                    )
                )
    return Levels(entities, pois)


@dataclass
class QuestGraph:
    kind: str
    name: str
    definitions: list[Path]
    assets: list[tuple[str, str]]
    instances: dict[str, list[str]]
    raw: bytes


def load_quest_graphs(root: Path) -> dict[Path, QuestGraph]:
    graphs: dict[Path, QuestGraph] = {}
    for dirpath, _, filenames in os.walk(root / "scripts" / "Quests" / "Final"):
        for filename in filenames:
            if not filename.endswith(".xml"):
                continue
            path = Path(os.path.normpath(os.path.join(dirpath, filename)))
            raw = path.read_bytes()
            skald = ET.fromstring(raw).find("Skald")
            if skald is None or len(skald) == 0:
                continue
            top = skald[0]
            assets = top.find("Assets")
            nodes = top.find("Nodes")
            instances: dict[str, list[str]] = defaultdict(list)
            for node in nodes if nodes is not None else []:
                if name := node.get("Name"):
                    instances[node.tag].append(name)
            graphs[path] = QuestGraph(
                kind=top.tag,
                name=top.get("Name", ""),
                definitions=[
                    Path(os.path.normpath(os.path.join(dirpath, d.get("File", "")))) for d in top.iter("Definition")
                ],
                assets=[(asset.tag, asset.get("Name", "")) for asset in (assets if assets is not None else [])],
                instances=instances,
                raw=raw.lower(),
            )
    return graphs


class QuestLinker:
    """Follows quest graphs that name a map to the level entities bound to their assets."""

    def __init__(self, graphs: dict[Path, QuestGraph], levels: Levels, quests_root: Path):
        self.graphs = graphs
        self.levels = levels
        self.quests_root = quests_root
        self.parent = {child: path for path, graph in graphs.items() for child in graph.definitions}
        self.holders: dict[str, list[Entity]] = defaultdict(list)
        for entity in levels.entities.values():
            if entity.asset_links:
                self.holders[entity.name.lower()].append(entity)

    def links(self, guid: str) -> list[dict]:
        refs = []
        for path, graph in self.graphs.items():
            if guid.encode() in graph.raw:
                chain = self._chain(path)
                refs.append(
                    {
                        "file": path.relative_to(self.quests_root).as_posix(),
                        "chain": [link.relative_to(self.quests_root).as_posix() for link in chain],
                        "bound_entities": self.bound(path),
                    }
                )
        return refs

    def bound(self, path: Path) -> list[dict]:
        """The level entities bound to the assets of a graph and of every graph above it."""
        return [bound for link in self._chain(path) for bound in self._bound(link)]

    def _chain(self, path: Path) -> list[Path]:
        """The graph and every graph above it, up to the quest root."""
        chain: list[Path] = []
        current: Path | None = path
        while current is not None and current not in chain:
            chain.append(current)
            current = self.parent.get(current)
        return chain

    def _bound(self, path: Path) -> list[dict]:
        # A graph's holder entity is named after the instance its parent graph places it as.
        graph = self.graphs[path]
        aliases = {name for _, name in graph.assets}
        if not aliases:
            return []
        owner = self.parent.get(path)
        instance_names = set(self.graphs[owner].instances.get(graph.name, [])) if owner else set()
        bound = []
        for instance in instance_names | {graph.name}:
            for holder in self.holders.get(instance.lower(), []):
                for target_id, alias in holder.asset_links:
                    target = self.levels.entities.get((holder.level, target_id))
                    if target is not None and alias in aliases:
                        bound.append({"holder": holder.name, "alias": alias, **_entity_row(target)})
        return bound


def _entity_row(entity: Entity) -> dict:
    row = asdict(entity)
    del row["asset_links"]
    return row


def build_index(root: Path, out_dir: Path) -> dict[str, dict]:
    strings = load_strings(root)
    maps = load_map_items(root, strings)
    preset_items, preset_refs = load_presets(root)
    levels = load_levels(root, strings)
    linker = QuestLinker(load_quest_graphs(root), levels, root / "scripts" / "Quests" / "Final")
    for item in maps.values():
        found_in = presets_containing(item["name"], preset_items, preset_refs)
        item["found_in_presets"] = sorted(found_in)
        item["found_in_entities"] = [
            _entity_row(entity) for entity in levels.entities.values() if entity.preset and entity.preset in found_in
        ]
        item["quest_refs"] = linker.links(item["guid"])

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "maps.json").write_text(json.dumps(list(maps.values()), indent=1, ensure_ascii=False), encoding="utf-8")
    with (out_dir / "containers.csv").open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerow(
            ["level", "class", "entity_id", "entity_guid", "name", "pos", "editor_layer", "inventory_preset"]
        )
        for e in levels.entities.values():
            if e.cls in CONTAINER_CLASSES:
                writer.writerow([e.level, e.cls, e.id, e.guid, e.name, e.pos, e.layer, e.preset])
    with (out_dir / "pois.csv").open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerow(["level", "name", "pos", "label_en", "poi_type"])
        for poi in levels.pois:
            writer.writerow([poi.level, poi.name, poi.pos, poi.label, poi.poi_type])
    return maps
