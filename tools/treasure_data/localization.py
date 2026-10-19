"""Build the mod's localization paks (Localization/<Language>_xml.pak) from one string table.

The table maps each string id to its text per game language. Each pak holds one text_ui_treasurefinder.xml in the
game's format: a <Row> per string with the id, the English original and the translation.
"""

from __future__ import annotations

import json
import zipfile
from pathlib import Path
from xml.sax.saxutils import escape

TABLE_NAME = "text_ui_treasurefinder.xml"


def build(table_path: Path, out_dir: Path) -> list[Path]:
    table: dict[str, dict[str, str]] = json.loads(table_path.read_text(encoding="utf-8"))
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
