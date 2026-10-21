"""Publish to Nexus Mods through its v3 API: publish.py {page,file}.

page: the summary and description, from store/summary.txt and store/description.bbcode.
file: a release zip, as the first version of the mod's main file or as its next version, plus the version's changelog.

Which mod and file: store/nexus.json. Needs a personal API key (https://www.nexusmods.com/settings/api-keys) in
NEXUS_API_KEY. Standard library only, so CI runs it with the runner's own Python.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import time
import urllib.error
import urllib.request
from pathlib import Path

API = "https://api.nexusmods.com/v3"
STORE = Path(__file__).resolve().parents[1] / "store"
SETTINGS = json.loads((STORE / "nexus.json").read_text(encoding="utf-8"))
GAME_DOMAIN = SETTINGS["game"]
MOD_ID = SETTINGS["mod"]  # the number in the page URL
# A new version goes to the active file with this name. Its description is set per version, so an edit shows from the
# next upload on.
FILE_NAME = SETTINGS["file"]
FILE_DESCRIPTION = SETTINGS["file_description"]
SUMMARY_LIMIT = 350
UPLOAD_TIMEOUT = 300


def call(method: str, url: str, body: dict | None = None) -> dict | None:
    """One API request; a failure stops the script with Nexus's problem details."""
    request = urllib.request.Request(
        url if url.startswith("https://") else API + url,
        method=method,
        data=None if body is None else json.dumps(body).encode(),
        headers={
            "apikey": os.environ["NEXUS_API_KEY"],
            "Accept": "application/json",
            "Content-Type": "application/json",
            "User-Agent": "FindersKeepers-publish",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            payload = response.read()
    except urllib.error.HTTPError as error:
        raise SystemExit(f"{method} {request.full_url}: {error.code} {error.read().decode(errors='replace')}") from None
    return json.loads(payload)["data"] if payload else None


def mod_uuid() -> str:
    """The API's own id for the mod; most endpoints take it rather than the page URL's number."""
    return call("GET", f"/games/{GAME_DOMAIN}/mods/{MOD_ID}")["id"]


def publish_page(mod: str) -> None:
    summary = " ".join((STORE / "summary.txt").read_text(encoding="utf-8").split())
    if len(summary) > SUMMARY_LIMIT:
        raise SystemExit(f"summary.txt is {len(summary)} characters; Nexus allows {SUMMARY_LIMIT}")
    description = (STORE / "description.bbcode").read_text(encoding="utf-8").strip()
    call("PATCH", f"/mods/{mod}", {"summary": summary, "description": description})
    print(f"updated the summary and description of {GAME_DOMAIN}/mods/{MOD_ID}")


def upload(archive: Path) -> str:
    """Upload a zip and wait until Nexus has it; returns the upload id a file version is made from."""
    data = archive.read_bytes()
    digest = hashlib.md5(data)
    created = call("POST", "/uploads", {"size_bytes": len(data), "filename": archive.name, "md5": digest.hexdigest()})
    # All three headers are part of the presigned URL's signature. Without its own Content-Type, urllib sends a form
    # type, which the signature rejects; octet-stream is what Nexus's upload action sends.
    put = urllib.request.Request(
        created["presigned_url"],
        method="PUT",
        data=data,
        headers={
            "Content-Type": "application/octet-stream",
            "Content-Disposition": f'attachment; filename="{archive.name}"',
            "Content-MD5": base64.b64encode(digest.digest()).decode(),
        },
    )
    try:
        urllib.request.urlopen(put, timeout=300).close()
    except urllib.error.HTTPError as error:
        raise SystemExit(f"uploading {archive.name}: {error.code} {error.read().decode(errors='replace')}") from None
    call("POST", f"/uploads/{created['id']}/finalise")
    deadline = time.monotonic() + UPLOAD_TIMEOUT
    while call("GET", f"/uploads/{created['id']}")["state"] != "available":
        if time.monotonic() > deadline:
            raise SystemExit(f"upload {created['id']} was not available after {UPLOAD_TIMEOUT} s")
        time.sleep(5)
    return created["id"]


def publish_file(mod: str, archive: Path, version: str, changelog: str) -> None:
    if not re.fullmatch(r"[a-zA-Z0-9.-]{1,50}", version):
        raise SystemExit(f"Nexus versions are letters, digits, dots and dashes: {version!r}")
    files = [
        file
        for file in call("GET", f"/mods/{mod}/files")["mod_files"]
        if file["name"] == FILE_NAME and file["is_active"]
    ]
    if len(files) > 1:
        raise SystemExit(f"{len(files)} active files are named {FILE_NAME!r}; keep one")
    fields = {
        "upload_id": upload(archive),
        "name": FILE_NAME,
        "description": FILE_DESCRIPTION,
        "version": version,
        "file_category": "main",
        "primary_mod_manager_download": True,
        # The page's requirements, shown again when the file is downloaded, for anyone who skips the description.
        "show_requirements_pop_up": True,
        "update_mod_version": True,
    }
    if files:
        call("POST", f"/mod-files/{files[0]['id']}/versions", fields)
        print(f"added {archive.name} as version {version} of {FILE_NAME!r}")
    else:
        call("POST", "/mod-files", fields | {"mod_id": mod})
        print(f"created {FILE_NAME!r} with {archive.name} as version {version}")
    if changelog:
        call("POST", f"/mods/{mod}/changelogs", {"version": version, "changelog": changelog})
        print(f"added the changelog for {version}")


def main() -> int:
    parser = argparse.ArgumentParser(prog="publish.py", description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("page", help="update the summary and description")
    file_cmd = commands.add_parser("file", help="upload a release zip as the main file's next version")
    file_cmd.add_argument("archive", type=Path, help="the release zip")
    file_cmd.add_argument("version", help="the version Nexus shows, e.g. 0.1.0")
    file_cmd.add_argument("--changelog", type=Path, help="text file with this version's changes (skipped when empty)")
    args = parser.parse_args()

    mod = mod_uuid()
    if args.command == "page":
        publish_page(mod)
    else:
        changelog = args.changelog.read_text(encoding="utf-8").strip() if args.changelog else ""
        publish_file(mod, args.archive, args.version, changelog)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
