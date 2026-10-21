"""Unpack the game files the index needs into a per-build cache folder."""

from __future__ import annotations

import zipfile
from collections.abc import Callable, Iterator
from pathlib import Path

from .game import LEVELS, build_id, read_member

COMPLETE_MARKER = ".complete"


def _sources(game_dir: Path) -> Iterator[tuple[Path, str, Callable[[str], bool]]]:
    data = game_dir / "Data"
    yield data / "Tables.pak", "tables", lambda name: name.startswith("Libs/Tables/item/")
    yield (
        data / "Scripts.pak",
        "scripts",
        lambda name: name.startswith("Quests/Final/") or (name.startswith("Scripts/") and name.endswith(".lua")),
    )
    yield game_dir / "Localization" / "English_xml.pak", "localization", lambda name: name.endswith(".xml")
    for level in LEVELS:
        yield (
            data / "Levels" / level / "level.pak",
            f"levels/{level}",
            lambda name: name == "objects_mission0.xml" or name.startswith("layers/"),
        )


def game_root(cache_dir: Path, game_dir: Path) -> Path:
    return cache_dir / build_id(game_dir) / "game"


def extract(game_dir: Path, cache_dir: Path, force: bool = False) -> Path:
    root = game_root(cache_dir, game_dir)
    marker = root / COMPLETE_MARKER
    if marker.is_file() and not force:
        return root
    for pak_path, subdir, wanted in _sources(game_dir):
        dest = root / subdir
        count = 0
        with zipfile.ZipFile(pak_path) as pak:
            for info in pak.infolist():
                if info.is_dir() or not wanted(info.filename):
                    continue
                target = dest / info.filename
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(read_member(pak, info))
                count += 1
        print(f"extracted {count:6} files from {pak_path.relative_to(game_dir)}")
    marker.write_text(build_id(game_dir) + "\n", encoding="utf-8")
    return root
