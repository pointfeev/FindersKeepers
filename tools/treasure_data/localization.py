"""Build the mod's localization paks (Localization/<Language>_xml.pak) from one string table.

The table maps each string id to its text per game language. Each pak holds one text_ui_treasurefinder.xml in the
game's format: a <Row> per string with the id, the English original and the translation. Each marked map in the
map table also gets its tooltip strings, "<type>: <map name>" in that language's own format, on its destinations
and on its own location, the map name taken from the install's own localization.
"""

from __future__ import annotations

import json
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape

from .game import read_member
from .maptable import map_source, marked_rows, tooltip_label

TABLE_NAME = "text_ui_treasurefinder.xml"
ITEM_TABLES = "Libs/Tables/item/item"
# Our mark types' names in our own table, the tooltip's "<type>": on a map's destinations and on its own location.
TYPE_STRINGS = {False: "ui_maplegend_TreasureFinder", True: "ui_maplegend_TreasureFinderMap"}


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


def read_game_strings(game_dir: Path, language: str) -> dict[str, str]:
    """Every UI string of one game language, label (lowercase) -> text."""
    strings = {}
    with zipfile.ZipFile(game_dir / "Localization" / f"{language}_xml.pak") as pak:
        for info in pak.infolist():
            if not info.filename.endswith(".xml"):
                continue
            for row in ET.fromstring(read_member(pak, info)).iter("Row"):
                cells = [cell.text or "" for cell in row.findall("Cell")]
                if len(cells) >= 2:
                    strings[cells[0].lower()] = cells[2] if len(cells) > 2 and cells[2] else cells[1]
    return strings


def map_tooltips(
    table: dict[str, dict[str, str]], map_table: Path, formats_path: Path, game_dir: Path
) -> dict[str, dict[str, str]]:
    """Each marked map's tooltip string ids (its destinations, and its own location where it has one) -> text per
    language."""
    formats: dict[str, str] = json.loads(formats_path.read_text(encoding="utf-8"))
    ui_names = read_ui_names(game_dir)
    # string id -> (whether it is a map location's, the map's name label)
    entries: dict[str, tuple[bool, str]] = {}
    for row in marked_rows(map_table):
        label = row.get("name_label") or ui_names.get((row["guid"] or "").lower())
        if not label:
            raise SystemExit(f"{row['map']}: no name label (an item UIName or the row's name_label)")
        name_label = label.lstrip("@").lower()
        entries[tooltip_label(row)] = (False, name_label)
        if map_source(row) is not None:
            entries[tooltip_label(row, source=True)] = (True, name_label)
    tooltips: dict[str, dict[str, str]] = {string_id: {} for string_id in entries}
    for language, pattern in formats.items():
        strings = read_game_strings(game_dir, language)
        for string_id, (source, name_label) in entries.items():
            name = strings.get(name_label)
            if name is None:
                raise SystemExit(f"{string_id}: {name_label} is not in the game's {language} strings")
            kinds = table[TYPE_STRINGS[source]]
            tooltips[string_id][language] = pattern.format(
                type=kinds.get(language) or kinds["English"], name=name.strip()
            )
    return tooltips


def build(table_path: Path, out_dir: Path, map_table: Path, formats_path: Path, game_dir: Path) -> list[Path]:
    table: dict[str, dict[str, str]] = json.loads(table_path.read_text(encoding="utf-8"))
    table |= map_tooltips(table, map_table, formats_path, game_dir)
    languages = sorted({language for texts in table.values() for language in texts})
    out_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for language in languages:
        rows = []
        for string_id, texts in sorted(table.items()):
            english = texts.get("English", "")
            text = texts.get(language, english)
            rows.append(
                f"<Row><Cell>{escape(string_id)}</Cell><Cell>{escape(english)}</Cell><Cell>{escape(text)}</Cell></Row>"
            )
        xml = "<Table>\n" + "\n".join(rows) + "\n</Table>\n"
        pak = out_dir / f"{language}_xml.pak"
        with zipfile.ZipFile(pak, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr(TABLE_NAME, xml.encode("utf-8"))
        written.append(pak)
    return written
