"""Locating a KCD2 install and reading its .pak archives (plain zip files)."""

from __future__ import annotations

import json
import os
import zipfile
from pathlib import Path

DEFAULT_GAME_DIR = Path(r"C:\Program Files (x86)\Steam\steamapps\common\KingdomComeDeliverance2")
LEVELS = ("trosecko", "kutnohorsko", "klaster")


def find_game_dir(arg: str | None) -> Path:
    game_dir = Path(arg or os.environ.get("KCD2_DIR") or DEFAULT_GAME_DIR)
    if not (game_dir / "Data" / "Tables.pak").is_file():
        raise SystemExit(f"Not a KCD2 install (no Data/Tables.pak): {game_dir}. Pass --game or set KCD2_DIR.")
    return game_dir


def build_id(game_dir: Path) -> str:
    """The build string KCSE logs, e.g. 'release_1_5-15693'."""
    info = json.loads((game_dir / "whdlversions.json").read_text(encoding="utf-8"))
    return f"{info['Preset']['Branch']['Name']}-{info['Assembly']['Id']}"


def read_member(pak: zipfile.ZipFile, info: zipfile.ZipInfo) -> bytes:
    try:
        return pak.read(info)
    except zipfile.BadZipFile:
        # Level paks name some files with backslashes in the local header and slashes in the central directory;
        # zipfile refuses the mismatch unless it is told the header spelling.
        info.orig_filename = info.filename.replace("/", "\\")
        return pak.read(info)
