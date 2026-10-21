cmake_minimum_required(VERSION 4.0)

project(FindersKeepers LANGUAGES CXX)

set(MOD_ID finderskeepers)

if(DEFINED ENV{KCD2_DIR})
    set(KCD2_DIR_DEFAULT "$ENV{KCD2_DIR}")
else()
    set(KCD2_DIR_DEFAULT "C:/Program Files (x86)/Steam/steamapps/common/KingdomComeDeliverance2")
endif()
set(KCD2_DIR "${KCD2_DIR_DEFAULT}" CACHE PATH "KCD2 install folder that the deploy target copies the mod into")

file(GLOB_RECURSE PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/plugin/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/plugin/src/*.h")
add_library(FindersKeepers SHARED ${PLUGIN_SOURCES})
set_target_properties(FindersKeepers PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(FindersKeepers PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(FindersKeepers PRIVATE /utf-8 /W4)
target_include_directories(FindersKeepers PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/plugin/src")

# ---- Mod folder: <build>/mod/finderskeepers, laid out as it sits in the game's Mods folder ----
set(MOD_STAGE "${CMAKE_BINARY_DIR}/mod/${MOD_ID}")
set(MOD_PAK "${MOD_STAGE}/Data/${MOD_ID}.pak")
set(MOD_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/mod/mod.manifest")
# Game assets the pak reuses under our names (the mark type's icons), read from the KCD2_DIR install at build time.
set(MOD_GAME_FILES "${CMAKE_CURRENT_SOURCE_DIR}/mod/game_files.json")
# The curated map table: the plugin's map data (generated C++) and the map tooltip strings come from it.
set(MAP_TABLE "${CMAKE_CURRENT_SOURCE_DIR}/research/treasure_maps.json")
file(GLOB_RECURSE MOD_DATA_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/mod/data/*")
# The file list as a dependency: it is rewritten only when files are added or removed, so a deleted data file
# repacks the .pak too (file timestamps alone only catch edits).
string(REPLACE ";" "\n" MOD_DATA_LIST "${MOD_DATA_FILES}")
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/mod-data-files.txt" CONTENT "${MOD_DATA_LIST}\n")

find_program(UV_EXECUTABLE uv REQUIRED)

set(MAP_TABLE_DATA "${CMAKE_BINARY_DIR}/generated/MapTableData.inc")
add_custom_command(
    OUTPUT "${MAP_TABLE_DATA}"
    COMMAND "${UV_EXECUTABLE}" run --project "${CMAKE_CURRENT_SOURCE_DIR}" treasure-data maptable "${MAP_TABLE}"
            "${MAP_TABLE_DATA}"
    DEPENDS "${MAP_TABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/treasure_data/maptable.py"
    COMMENT "Generating the map table"
    VERBATIM)
target_sources(FindersKeepers PRIVATE "${MAP_TABLE_DATA}")
target_include_directories(FindersKeepers PRIVATE "${CMAKE_BINARY_DIR}/generated")

add_custom_command(
    OUTPUT "${MOD_PAK}"
    COMMAND "${UV_EXECUTABLE}" run --project "${CMAKE_CURRENT_SOURCE_DIR}" treasure-data --game "${KCD2_DIR}" pack
            "${CMAKE_CURRENT_SOURCE_DIR}/mod/data" "${MOD_PAK}" --game-files "${MOD_GAME_FILES}"
    DEPENDS ${MOD_DATA_FILES} "${CMAKE_BINARY_DIR}/mod-data-files.txt" "${MOD_GAME_FILES}"
    COMMENT "Packing ${MOD_ID}.pak"
    VERBATIM)

# One Localization/<Language>_xml.pak per game language, built from the string table; the stamp stands in for the
# set of paks, whose languages come from the table.
set(MOD_STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/mod/localization.json")
# Per language, how a map tooltip combines the mark type's name and the map's name (from the install's strings).
set(MOD_TOOLTIP_FORMATS "${CMAKE_CURRENT_SOURCE_DIR}/mod/tooltip_formats.json")
set(MOD_LOCALIZATION_STAMP "${CMAKE_BINARY_DIR}/localization.stamp")
add_custom_command(
    OUTPUT "${MOD_LOCALIZATION_STAMP}"
    COMMAND "${UV_EXECUTABLE}" run --project "${CMAKE_CURRENT_SOURCE_DIR}" treasure-data --game "${KCD2_DIR}" localize
            "${MOD_STRINGS}" "${MOD_STAGE}/Localization" --map-table "${MAP_TABLE}"
            --tooltip-formats "${MOD_TOOLTIP_FORMATS}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${MOD_LOCALIZATION_STAMP}"
    DEPENDS "${MOD_STRINGS}" "${MOD_TOOLTIP_FORMATS}" "${MAP_TABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/treasure_data/localization.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/treasure_data/maptable.py"
    COMMENT "Building localization paks"
    VERBATIM)

add_custom_target(mod ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MOD_MANIFEST}" "${MOD_STAGE}/mod.manifest"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MOD_STAGE}/KCSE/Plugins"
    # The DLL only: debuggers find its PDB at the build path recorded in the DLL.
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:FindersKeepers>" "${MOD_STAGE}/KCSE/Plugins"
    DEPENDS FindersKeepers "${MOD_PAK}" "${MOD_LOCALIZATION_STAMP}" "${MOD_MANIFEST}"
    COMMENT "Staging ${MOD_STAGE}"
    SOURCES "${MOD_MANIFEST}" "${MOD_STRINGS}" "${MOD_GAME_FILES}" ${MOD_DATA_FILES}
    VERBATIM)

# The mod folder is replaced whole, so files no longer part of the mod go too; the player's settings file stays.
add_custom_target(deploy
    COMMAND "${CMAKE_COMMAND}" "-DSTAGE=${MOD_STAGE}" "-DTARGET=${KCD2_DIR}/Mods/${MOD_ID}"
            "-DKEEP=KCSE/Plugins/FindersKeepers.ini" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy.cmake"
    DEPENDS mod
    COMMENT "Copying the mod into ${KCD2_DIR}/Mods/${MOD_ID}"
    VERBATIM)
