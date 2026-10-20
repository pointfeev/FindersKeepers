# Replaces the installed mod folder with the staged one, so files no longer part of the mod go too, but keeps the
# player's settings file, which the plugin writes and the mod does not ship.
# Usage: cmake -DSTAGE=<staged mod> -DTARGET=<installed mod> -DKEEP=<path inside the mod> -P deploy.cmake
set(kept "${TARGET}/${KEEP}")
set(saved "${TARGET}.kept")
if(EXISTS "${kept}")
    file(RENAME "${kept}" "${saved}")
endif()
file(REMOVE_RECURSE "${TARGET}")
file(COPY "${STAGE}/" DESTINATION "${TARGET}")
if(EXISTS "${saved}")
    file(RENAME "${saved}" "${kept}")
endif()
