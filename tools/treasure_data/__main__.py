"""Command line: treasure-data {extract,index,validate}."""

from __future__ import annotations

import argparse
from pathlib import Path

from .extract import extract
from .game import build_id, find_game_dir
from .index import build_index
from .validate import validate

REPO_ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    parser = argparse.ArgumentParser(prog="treasure-data", description=__doc__)
    parser.add_argument("--game", help="KCD2 install folder (default: KCD2_DIR, else the Steam install path)")
    parser.add_argument("--cache", type=Path, default=REPO_ROOT / ".cache", help="where extracted files and indexes go")
    commands = parser.add_subparsers(dest="command", required=True)

    extract_cmd = commands.add_parser("extract", help="unpack the needed game files into the cache")
    extract_cmd.add_argument("--force", action="store_true", help="unpack again even if this build is cached")

    commands.add_parser("index", help="build maps.json, containers.csv and pois.csv for this build")

    validate_cmd = commands.add_parser("validate", help="check the map table against this build")
    validate_cmd.add_argument("--table", type=Path, default=REPO_ROOT / "research" / "treasure_maps.json")
    validate_cmd.add_argument("--tolerance", type=float, default=2.0, help="metres an entity may move (default 2)")

    args = parser.parse_args()
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
