# Store page

The [Nexus Mods page](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3863) is published from this folder by `tools/publish.py`, through the [Nexus v3 API](https://api.nexusmods.com/openapi.yaml). The endpoints it uses are all marked experimental.

- `description.bbcode` is the page's description, in the BBCode Nexus uses.
- `summary.txt` is the short text on mod tiles and at the top of the page, 350 characters at most.
- `nexus.json` names the game, the mod and its main file, and holds the file's description.

## Publishing

The workflow `.github/workflows/nexus.yml` runs the script with the repository secret `NEXUS_API_KEY`, a personal key from the Nexus API keys page.

- A push to master that changes the page text or `nexus.json`, a release, or a manual run updates the summary and description.
- Publishing a GitHub release tagged `v<version>` with `FindersKeepers-<version>.zip` attached uploads the zip as the main file's next version, and the release notes become its changelog. The previous version moves to old files on its own.

To release, bump the version in `mod/mod.manifest`, build the zip with `cmake --build --preset package`, then publish the release with the zip from `build/release/`. The zip can't be built in CI, because the pak copies icons and map names from the installed game.

The same commands run locally with the key in `NEXUS_API_KEY`: `python tools/publish.py page`, or `python tools/publish.py file <zip> <version>`.

The API can't set the mod's name, category, tags, images, page requirements, or permissions and credits, so those are edited on the website.

## Why not Steam Workshop

KCSE loads plugins only from `<game>/mods/*/KCSE/Plugins`, and Workshop items install under `steamapps/workshop/content/1771300/`, so a Workshop copy would never load the DLL ([KCSE's plugin manager](https://github.com/JerryYOJ/KCSE-for-kcd2/blob/HEAD/src/PluginManager.cpp)). KCSE itself can't come from the Workshop either.
