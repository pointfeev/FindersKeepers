"""Build a mod .pak (a zip the game's pak loader reads) from a data folder and files taken from the game's paks."""

from __future__ import annotations

import json
import zipfile
from pathlib import Path

from .game import read_member


def read_game_files(game_dir: Path, table: Path) -> dict[str, bytes]:
    """The files a table names, read from the install: pak path -> "<pak under Data>:<member>".

    Game assets the mod reuses under its own names stay out of the repo and come from the player's own install.
    """
    files = {}
    for name, source in json.loads(table.read_text(encoding="utf-8")).items():
        pak_name, member = source.split(":", 1)
        with zipfile.ZipFile(game_dir / "Data" / pak_name) as pak:
            files[name] = read_member(pak, pak.getinfo(member))
    return files


def pack(source: Path, out: Path, extra: dict[str, bytes] | None = None) -> int:
    files = sorted(path for path in source.rglob("*") if path.is_file())
    extra = extra or {}
    clashes = sorted(set(extra) & {path.relative_to(source).as_posix() for path in files})
    if clashes:
        raise SystemExit(f"game files would replace data files: {', '.join(clashes)}")
    out.parent.mkdir(parents=True, exist_ok=True)
    # Written to a seekable file, zipfile records sizes in each local header (no data descriptors) and stores only
    # file entries with forward-slash names, the layout the game's own paks use.
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as pak:
        for path in files:
            pak.write(path, path.relative_to(source).as_posix())
        for name, data in sorted(extra.items()):
            pak.writestr(zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0)), data, zipfile.ZIP_DEFLATED)
    return len(files) + len(extra)
