# FluidSynth is an optional desktop music decoder. Configuration never downloads
# a library or SoundFont; ON requires it, AUTO enables it when available.
set(FTE_DEP_FLUIDSYNTH AUTO CACHE STRING "MIDI support: AUTO, ON, or OFF.")
set_property(CACHE FTE_DEP_FLUIDSYNTH PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${FTE_DEP_FLUIDSYNTH}" fte_midi_mode)
if(NOT fte_midi_mode MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "FTE_DEP_FLUIDSYNTH must be AUTO, ON, or OFF")
endif()
if(NOT fte_midi_mode STREQUAL "OFF")
    find_package(FluidSynth 2.2 CONFIG QUIET)
    if(NOT TARGET FluidSynth::libfluidsynth)
        find_path(FLUIDSYNTH_INCLUDE_DIR fluidsynth.h)
        find_library(FLUIDSYNTH_LIBRARY NAMES fluidsynth libfluidsynth libfluidsynth-3)
        if(FLUIDSYNTH_INCLUDE_DIR AND FLUIDSYNTH_LIBRARY)
            file(STRINGS "${FLUIDSYNTH_INCLUDE_DIR}/fluidsynth/version.h" fte_fluid_version_line
                REGEX "^#define FLUIDSYNTH_VERSION[ \t]+\"")
            string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" fte_fluid_version "${fte_fluid_version_line}")
        endif()
        if(FLUIDSYNTH_INCLUDE_DIR AND FLUIDSYNTH_LIBRARY AND fte_fluid_version VERSION_GREATER_EQUAL 2.2)
            add_library(FluidSynth::libfluidsynth UNKNOWN IMPORTED)
            set_target_properties(FluidSynth::libfluidsynth PROPERTIES
                IMPORTED_LOCATION "${FLUIDSYNTH_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FLUIDSYNTH_INCLUDE_DIR}")
        endif()
    endif()
    if(TARGET FluidSynth::libfluidsynth)
        add_library(fte-midi-core STATIC engine/client/midi_fluid.c)
        target_link_libraries(fte-midi-core PUBLIC FluidSynth::libfluidsynth ${SYS_LIBS})
        target_include_directories(fte-midi-core PUBLIC "${PROJECT_SOURCE_DIR}/engine/client")
        target_compile_definitions(fte-midi-core PUBLIC AVAIL_MIDI
            FTE_MIDI_INSTALLED_FONT="${CMAKE_INSTALL_FULL_DATADIR}/fteqw/soundfonts/FluidR3_GM.sf2")
        list(APPEND FTE_LIBS fte-midi-core)
        message(STATUS "MIDI playback enabled (FluidSynth)")
    elseif(fte_midi_mode STREQUAL "ON")
        message(FATAL_ERROR "MIDI requires FluidSynth 2.2+ development headers and library")
    else()
        message(STATUS "FluidSynth not found; MIDI playback disabled")
    endif()
endif()

# Only the explicitly staged, checksum-verified FluidR3 release is bundled.
set(FTE_MIDI_SOUNDFONT "" CACHE FILEPATH "Verified FluidR3_GM.sf2 to stage/install with the client.")
if(FTE_MIDI_SOUNDFONT)
    if(NOT TARGET fte-midi-core)
        message(FATAL_ERROR "FTE_MIDI_SOUNDFONT requires MIDI support")
    endif()
    file(SHA256 "${FTE_MIDI_SOUNDFONT}" fte_midi_font_hash)
    if(NOT fte_midi_font_hash STREQUAL "74594e8f4250680adf590507a306655a299935343583256f3b722c48a1bc1cb0")
        message(FATAL_ERROR "FTE_MIDI_SOUNDFONT is not the pinned FluidR3 GM 3.1 file")
    endif()
    install(FILES "${FTE_MIDI_SOUNDFONT}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fteqw/soundfonts" RENAME FluidR3_GM.sf2 COMPONENT MIDI)
    install(FILES "${PROJECT_SOURCE_DIR}/dist/soundfonts/FluidR3-GM/COPYING"
        "${PROJECT_SOURCE_DIR}/dist/soundfonts/FluidR3-GM/README"
        "${PROJECT_SOURCE_DIR}/dist/soundfonts/FluidR3-GM/SOURCE.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fteqw/soundfonts/FluidR3-GM" COMPONENT MIDI)
endif()
