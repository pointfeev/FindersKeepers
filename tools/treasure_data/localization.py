"""Build the mod's localization paks (Localization/<Language>_xml.pak) from one string table.

The table maps each string id to its text per game language. Each pak holds one text_ui_treasurefinder.xml in the
game's format: a <Row> per string with the id, the English original and the translation. Each marked map in the
map table also gets its tooltip string, "<type>: <map name>" in that language's own format, the map name taken
from the install's own localization.
"""

from __future__ import annotations

import json
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape

from .game import read_member
from .maptable import marked_rows, tooltip_label

TABLE_NAME = "text_ui_treasurefinder.xml"
ITEM_TABLES = "Libs/Tables/item/item"
# The mark type's name in our own table: the tooltip's "<type>".
TYPE_STRING = "ui_maplegend_TreasureFinder"


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
    """Each marked map's tooltip string id -> its text per language."""
    formats: dict[str, str] = json.loads(formats_path.read_text(encoding="utf-8"))
    ui_names = read_ui_names(game_dir)
    name_labels = {}
    for row in marked_rows(map_table):
        label = row.get("name_label") or ui_names.get((row["guid"] or "").lower())
        if not label:
            raise SystemExit(f"{row['map']}: no name label (an item UIName or the row's name_label)")
        name_labels[tooltip_label(row)] = label.lstrip("@").lower()
    tooltips: dict[str, dict[str, str]] = {string_id: {} for string_id in name_labels}
    for language, pattern in formats.items():
        strings = read_game_strings(game_dir, language)
        kind = table[TYPE_STRING].get(language) or table[TYPE_STRING]["English"]
        for string_id, name_label in name_labels.items():
            name = strings.get(name_label)
            if name is None:
                raise SystemExit(f"{string_id}: {name_label} is not in the game's {language} strings")
            tooltips[string_id][language] = pattern.format(type=kind, name=name.strip())
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
