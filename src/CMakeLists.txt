add_subdirectory(doom)

# Common source files used by absolutely everything:

set(COMMON_SOURCE_FILES
    i_main.c
    i_system.c           i_system.h
    m_argv.c             m_argv.h
    m_misc.c             m_misc.h)

# Dedicated server (chocolate-server):

set(DEDSERV_FILES
    d_dedicated.c
    d_iwad.c            d_iwad.h
    d_mode.c            d_mode.h
    deh_str.c           deh_str.h
    i_timer.c           i_timer.h
    m_config.c          m_config.h
    z_native.c          z_zone.h)

add_executable("${PROGRAM_PREFIX}server" WIN32 ${COMMON_SOURCE_FILES} ${DEDSERV_FILES})
target_include_directories("${PROGRAM_PREFIX}server"
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")
target_link_libraries("${PROGRAM_PREFIX}server" SDL2::SDL2main SDL2::net)

# Source files used by the game binaries (chocolate-doom, etc.)

set(GAME_SOURCE_FILES
    d_event.c           d_event.h
                        doomkeys.h
                        doomtype.h
    d_iwad.c            d_iwad.h
    d_loop.c            d_loop.h
    d_mode.c            d_mode.h
                        d_ticcmd.h
    deh_str.c           deh_str.h
    gusconf.c           gusconf.h
    i_endoom.c          i_endoom.h
    i_glob.c            i_glob.h
    i_input.c           i_input.h
    i_joystick.c        i_joystick.h
                        i_swap.h
    i_midipipe.c        i_midipipe.h
    i_timer.c           i_timer.h
    midifile.c          midifile.h
    m_bbox.c            m_bbox.h
    m_cheat.c           m_cheat.h
    m_config.c          m_config.h
    m_controls.c        m_controls.h
    m_fixed.c           m_fixed.h
    memio.c             memio.h
    tables.c            tables.h
                        v_patch.h
    w_main.c            w_main.h
    w_wad.c             w_wad.h
    w_file.c            w_file.h
    w_file_stdc.c
    w_file_posix.c
    w_file_win32.c
    w_merge.c           w_merge.h
    z_zone.c            z_zone.h)

set(GAME_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/../")

if(MSVC)
    list(APPEND GAME_SOURCE_FILES
         "../win32/win_opendir.c" "../win32/win_opendir.h")

    list(APPEND GAME_INCLUDE_DIRS
         "${PROJECT_SOURCE_DIR}/win32/")
endif()

set(DEHACKED_SOURCE_FILES
    deh_defs.h
    deh_io.c            deh_io.h
    deh_main.c          deh_main.h
    deh_mapping.c       deh_mapping.h
    deh_text.c)

# Some games support dehacked patches, some don't:

set(SOURCE_FILES ${COMMON_SOURCE_FILES} ${GAME_SOURCE_FILES})
set(SOURCE_FILES_WITH_DEH ${SOURCE_FILES} ${DEHACKED_SOURCE_FILES})

set(EXTRA_LIBS SDL2::SDL2main SDL2::SDL2 SDL2::mixer SDL2::net textscreen pcsound opl)
if(SAMPLERATE_FOUND)
    list(APPEND EXTRA_LIBS samplerate::samplerate)
endif()
if(PNG_FOUND)
    list(APPEND EXTRA_LIBS PNG::PNG)
endif()

if(WIN32)
    add_executable("${PROGRAM_PREFIX}doom" WIN32 ${SOURCE_FILES_WITH_DEH} "${CMAKE_CURRENT_BINARY_DIR}/resource.rc")
else()
    add_executable("${PROGRAM_PREFIX}doom" ${SOURCE_FILES_WITH_DEH})
endif()

target_include_directories("${PROGRAM_PREFIX}doom" PRIVATE ${GAME_INCLUDE_DIRS})
target_link_libraries("${PROGRAM_PREFIX}doom" ${EXTRA_LIBS})
