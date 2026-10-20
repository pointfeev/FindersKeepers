"""Command line: treasure-data {extract,index,validate,pack,localize}."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from .extract import extract
from .game import build_id, find_game_dir
from .index import build_index
from .localization import build as build_localization
from .maptable import PAK_PATH as MAP_TABLE_PAK_PATH
from .maptable import build as build_map_table
from .pack import pack, read_game_files
from .validate import validate

REPO_ROOT = Path(__file__).resolve().parents[2]
# Outside the repo: tens of thousands of unpacked game files that an IDE would otherwise index.
DEFAULT_CACHE = Path(os.environ.get("LOCALAPPDATA") or Path.home() / ".cache") / "treasure-finder"


def main() -> int:
    parser = argparse.ArgumentParser(prog="treasure-data", description=__doc__)
    parser.add_argument("--game", help="KCD2 install folder (default: KCD2_DIR, else the Steam install path)")
    parser.add_argument(
        "--cache",
        type=Path,
        default=DEFAULT_CACHE,
        help=f"where extracted files and indexes go (default {DEFAULT_CACHE})",
    )
    commands = parser.add_subparsers(dest="command", required=True)

    extract_cmd = commands.add_parser("extract", help="unpack the needed game files into the cache")
    extract_cmd.add_argument("--force", action="store_true", help="unpack again even if this build is cached")

    commands.add_parser("index", help="build maps.json, containers.csv and pois.csv for this build")

    validate_cmd = commands.add_parser("validate", help="check the map table against this build")
    validate_cmd.add_argument("--table", type=Path, default=REPO_ROOT / "research" / "treasure_maps.json")
    validate_cmd.add_argument("--tolerance", type=float, default=2.0, help="metres an entity may move (default 2)")

    pack_cmd = commands.add_parser("pack", help="build a mod .pak from a data folder")
    pack_cmd.add_argument("source", type=Path, help="folder whose contents become the pak root")
    pack_cmd.add_argument("out", type=Path, help="the .pak file to write")
    pack_cmd.add_argument(
        "--game-files", type=Path, help='JSON: pak path -> "<game pak>:<member>", copied from the install'
    )
    pack_cmd.add_argument("--map-table", type=Path, help=f"the curated map table, generated into {MAP_TABLE_PAK_PATH}")

    localize_cmd = commands.add_parser("localize", help="build Localization/<Language>_xml.pak from a string table")
    localize_cmd.add_argument("table", type=Path, help="JSON: string id -> {language: text}")
    localize_cmd.add_argument("out_dir", type=Path, help="folder for the <Language>_xml.pak files")

    args = parser.parse_args()
    if args.command == "pack":
        extra = read_game_files(find_game_dir(args.game), args.game_files) if args.game_files else {}
        if args.map_table:
            extra[MAP_TABLE_PAK_PATH] = build_map_table(args.map_table)
        count = pack(args.source, args.out, extra)
        print(f"packed {count} files into {args.out}")
        return 0
    if args.command == "localize":
        paks = build_localization(args.table, args.out_dir)
        print(f"wrote {len(paks)} localization paks into {args.out_dir}")
        return 0

    game_dir = find_game_dir(args.game)
    build = build_id(game_dir)
    print(f"game build {build}")
    root = extract(game_dir, args.cache, force=getattr(args, "force", False))

    if args.command == "extract":
        print(f"game files in {root}")
    elif args.command == "index":
        out_dir = args.cache / build / "index"
        maps = build_index(root, out_dir)
        linked = sum(1 for item in maps.values() if any(ref["bound_entities"] for ref in item["quest_refs"]))
        print(f"{len(maps)} map items, {linked} with quest-bound entities; index in {out_dir}")
    elif args.command == "validate":
        report = validate(root, args.table, args.tolerance)
        for line in report.errors:
            print(f"error: {line}")
        for line in report.warnings:
            print(f"warning: {line}")
        print(f"{len(report.errors)} errors, {len(report.warnings)} warnings")
        return 1 if report.errors else 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
