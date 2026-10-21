"""Publish the GitHub release for the version in mod/mod.manifest: release.py <zip>.

The release notes are that version's section of CHANGELOG.md, and publishing the release starts the Nexus Mods
workflow, which uploads the zip. Needs the GitHub CLI (gh), signed in. The tag is made on GitHub at the commit checked
out here, so master must be pushed first.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
MANIFEST = "finderskeepers/mod.manifest"


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=REPO, check=True, capture_output=True, text=True).stdout.strip()


def manifest_version(text: str) -> str:
    match = re.search(r"<version>([^<]+)</version>", text)
    if not match:
        raise SystemExit("no <version> in the manifest")
    return match.group(1)


def changelog_notes(version: str) -> str:
    """The lines under "## <version>" up to the next version's heading."""
    text = (REPO / "CHANGELOG.md").read_text(encoding="utf-8")
    match = re.search(rf"^## {re.escape(version)}\n(.*?)(?=^## |\Z)", text, re.MULTILINE | re.DOTALL)
    if not match or not match.group(1).strip():
        raise SystemExit(f"CHANGELOG.md has no notes for {version}")
    return match.group(1).strip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="release.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("archive", type=Path, help="the release zip, from the package preset")
    args = parser.parse_args()

    version = manifest_version((REPO / "mod" / "mod.manifest").read_text(encoding="utf-8"))
    with zipfile.ZipFile(args.archive) as archive:
        if (packed := manifest_version(archive.read(MANIFEST).decode("utf-8"))) != version:
            raise SystemExit(f"{args.archive.name} is version {packed}, the manifest says {version}; rebuild it")
    notes = changelog_notes(version)
    tag = f"v{version}"

    if shutil.which("gh") is None:
        raise SystemExit("the GitHub CLI (gh) is not installed or not on PATH")
    if git("branch", "--show-current") != "master":
        raise SystemExit("release from master")
    if git("status", "--porcelain", "--untracked-files=no"):
        raise SystemExit("commit or discard the changes to tracked files first")
    git("fetch", "--quiet", "origin")
    head = git("rev-parse", "HEAD")
    if head != git("rev-parse", "origin/master"):
        raise SystemExit("push master first: the release is tagged at this commit on GitHub")
    if git("ls-remote", "--tags", "origin", f"refs/tags/{tag}"):
        raise SystemExit(f"{tag} already exists on GitHub")

    subprocess.run(
        ["gh", "release", "create", tag, str(args.archive), "--title", version, "--target", head, "--notes-file", "-"],
        cwd=REPO,
        input=notes,
        text=True,
        check=True,
    )
    git("fetch", "--quiet", "--tags", "origin")
    print(f"released {tag}; the Nexus Mods workflow uploads it from here")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
