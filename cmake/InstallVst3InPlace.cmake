# Install a VST3 by syncing into the existing bundle directory.
#
# JUCE's default COPY_PLUGIN_AFTER_BUILD does REMOVE_RECURSE on the destination
# first. That deletes the bundle FL Studio is watching, so the host never gets a
# clean "plugin file changed → reload wrapper" signal — it looks like the plugin
# vanished. rsync into the same directory keeps the bundle path stable and
# updates the Mach-O in place, which is what FL's wrapper reload flow expects.

if (NOT DEFINED SRC OR NOT DEFINED DEST)
    message (FATAL_ERROR "InstallVst3InPlace.cmake: SRC and DEST are required")
endif()

if (NOT EXISTS "${SRC}")
    message (FATAL_ERROR "InstallVst3InPlace.cmake: source missing: ${SRC}")
endif()

get_filename_component (_parent "${DEST}" DIRECTORY)
file (MAKE_DIRECTORY "${_parent}")

execute_process (
    COMMAND rsync -a --delete "${SRC}/" "${DEST}/"
    RESULT_VARIABLE _rv
)
if (NOT _rv EQUAL 0)
    message (FATAL_ERROR "InstallVst3InPlace.cmake: rsync failed (${_rv})")
endif()

message (STATUS "Installed ${DEST}")
