"""Check the curated map table against the current game build.

Run after every game patch: map items that appeared or vanished, and destination or source entities that no longer
exist, were renumbered, renamed or moved, are reported. Entities are identified by level, entity id and level GUID;
names are not unique within a level (dozens of entities are called Stash20), so a name only confirms the match.
"""

from __future__ import annotations

import json
import math
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from .index import Levels, load_levels, load_map_items, load_strings

QUEST_NAME = re.compile(r'<Quest Name="([^"]+)"')


@dataclass
class Report:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)


def _distance(a: str, b: str) -> float:
    ax, ay, *_ = (float(v) for v in a.split(","))
    bx, by, *_ = (float(v) for v in b.split(","))
    return math.hypot(ax - bx, ay - by)


class Checker:
    def __init__(self, levels: Levels, tolerance: float, report: Report):
        self.levels = levels
        self.by_guid = levels.by_guid()
        self.tolerance = tolerance
        self.report = report

    def check(self, label: str, ref: dict) -> None:
        level = ref.get("level")
        entity_id = str(ref.get("entity_id") or "")
        guid = ref.get("entity_guid") or ""
        if not entity_id:
            return
        entity = self.levels.entities.get((level, entity_id))
        if entity is None:
            moved_to = self.by_guid.get((level, guid), []) if guid else []
            if len(moved_to) == 1:
                self.report.errors.append(
                    f"{label}: entity {entity_id} is gone; GUID {guid} is now id {moved_to[0].id}"
                )
            else:
                self.report.errors.append(f"{label}: entity {entity_id} not found in {level}")
            return
        if guid and entity.guid != guid:
            self.report.errors.append(f"{label}: entity {entity_id} has GUID {entity.guid}, table says {guid}")
        elif not guid:
            self.report.warnings.append(f"{label}: no entity_guid in the table (game has {entity.guid})")
        if len(self.by_guid.get((level, entity.guid), [])) > 1:
            self.report.errors.append(f"{label}: GUID {entity.guid} is not unique in {level}")
        name = ref.get("entity_name")
        if name and entity.name != name:
            self.report.errors.append(f"{label}: entity {entity_id} is named '{entity.name}', table says '{name}'")
        pos = ref.get("pos")
        if pos and entity.pos and (moved := _distance(pos, entity.pos)) > self.tolerance:
            self.report.errors.append(f"{label}: entity {entity_id} moved {moved:.1f} m, now at {entity.pos}")


def quest_names(root: Path) -> set[str]:
    """The names of the game's quests, as the plugin looks them up (each quest graph's Skald Quest node)."""
    names: set[str] = set()
    for path in (root / "scripts" / "Quests").rglob("*.xml"):
        names.update(QUEST_NAME.findall(path.read_text(encoding="utf-8-sig", errors="replace")))
    return names


def document_ids(root: Path) -> set[str]:
    """The GUIDs of every document item (maps, notes, letters, books): riddle notes are documents but not maps."""
    ids: set[str] = set()
    for path in (root / "tables" / "Libs" / "Tables" / "item").glob("item*.xml"):
        ids.update(doc.get("Id", "").lower() for doc in ET.parse(path).iter("Document"))
    return ids


def validate(root: Path, table_path: Path, tolerance: float) -> Report:
    report = Report()
    strings = load_strings(root)
    game_maps = load_map_items(root, strings)
    documents = document_ids(root)
    checker = Checker(load_levels(root, strings), tolerance, report)
    rows = json.loads(table_path.read_text(encoding="utf-8"))
    quests = quest_names(root)
    for row in rows:
        for label, quest in (
            ("quest", row.get("quest")),
            ("after_quest", (row.get("examined") or {}).get("after_quest")),
        ):
            if quest and quest not in quests:
                report.errors.append(f"{row['map']}: {label} '{quest}' is not a quest in this build")

    table_guids = {row["guid"] for row in rows}
    for guid, item in sorted(game_maps.items(), key=lambda kv: kv[1]["name"]):
        if guid not in table_guids:
            report.errors.append(f"new map item not in the table: {item['name']} ({item['en_name']}, {guid})")
    for row in rows:
        # A map the player examines rather than carries (a carving) has no item, only its trigger.
        if row.get("examined"):
            checker.check(f"{row['map']} examined trigger", row["examined"])
        elif row["guid"] not in (documents if row["category"] == "riddle" else game_maps):
            report.errors.append(f"{row['map']}: map item {row['guid']} no longer exists in the game")
        if row.get("reveals") and row["reveals"] not in documents:
            report.errors.append(f"{row['map']}: the note it reveals, {row['reveals']}, no longer exists in the game")
        checker.check(f"{row['map']} destination", row)
        for place in row.get("extra_targets", []):
            checker.check(f"{row['map']} destination {place['entity_id']}", place)
        if row.get("source"):
            checker.check(f"{row['map']} source", row["source"])
    return report
