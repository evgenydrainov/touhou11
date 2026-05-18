cmake_minimum_required(VERSION 3.20)

set(CMAKE_UNITY_BUILD ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 64)

# for clang
set(CMAKE_CXX_STANDARD 17)

project(game)

set(ASSET_INFO_GENERATED_H "${CMAKE_SOURCE_DIR}/src/asset_info.generated.h")
set(ASSET_INFO_GENERATED_CPP "${CMAKE_SOURCE_DIR}/src/asset_info.generated.cpp")
set(SINE_TABLE_GENERATED_CPP "${CMAKE_SOURCE_DIR}/src/sine_table.generated.cpp")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLATFORM "x64")
else()
    set(PLATFORM "x86")
endif()

#
# Game
#
add_executable(
    game
    src/asset_info.h
    src/common.h
    src/debug.h
    src/game.cpp
    src/game.h
    src/game_assets.cpp
    src/game_assets.h
    src/intrinsics.h
    src/math_stuff.h
    src/opengl_functions.h
    src/platform_api.h
    src/platform_sdl2.cpp
    src/platform_sdl2_win32.cpp
    src/random.h
    src/render_backend.h
    src/render_backend_d3d9.cpp
    src/render_backend_d3d9.h
    src/render_backend_opengl.cpp
    src/render_backend_opengl.h
    src/render_backend_software.cpp
    src/render_backend_software.h
    src/renderer.cpp
    src/renderer.h
    src/scripting.h
    src/stage1.cpp
    src/stb_sprintf.cpp
    ${ASSET_INFO_GENERATED_CPP}
    ${ASSET_INFO_GENERATED_H}
    ${SINE_TABLE_GENERATED_CPP}
)

#
# Find SDL2 & SDL2_mixer
#
if(WIN32)
    target_include_directories(game PRIVATE
        "${CMAKE_SOURCE_DIR}/vendor/SDL2/include"
        "${CMAKE_SOURCE_DIR}/vendor/SDL2_mixer/include"
    )

    target_link_libraries(game PRIVATE
        "${CMAKE_SOURCE_DIR}/vendor/SDL2/lib/${PLATFORM}/SDL2main.lib"
        "${CMAKE_SOURCE_DIR}/vendor/SDL2/lib/${PLATFORM}/SDL2.lib"
        "${CMAKE_SOURCE_DIR}/vendor/SDL2_mixer/lib/${PLATFORM}/SDL2_mixer.lib"
    )
else()
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(SDL2 REQUIRED sdl2)
    target_include_directories(game PRIVATE ${SDL2_INCLUDE_DIRS})
    target_compile_options(game PRIVATE ${SDL2_CFLAGS_OTHER})
    target_link_directories(game PRIVATE ${SDL2_LIBRARY_DIRS})
    target_link_libraries(game PRIVATE ${SDL2_LIBRARIES})

    pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
    target_include_directories(game PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
    target_compile_options(game PRIVATE ${SDL2_MIXER_CFLAGS_OTHER})
    target_link_directories(game PRIVATE ${SDL2_MIXER_LIBRARY_DIRS})
    target_link_libraries(game PRIVATE ${SDL2_MIXER_LIBRARIES})
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT game
)

set_property(TARGET game PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
    add_custom_command(
        TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/vendor/SDL2/lib/${PLATFORM}/SDL2.dll"
                $<TARGET_FILE_DIR:game>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/vendor/SDL2/lib/${PLATFORM}/SDL2.pdb"
                $<TARGET_FILE_DIR:game>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/vendor/SDL2_mixer/lib/${PLATFORM}/SDL2_mixer.dll"
                $<TARGET_FILE_DIR:game>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/vendor/SDL2_mixer/lib/${PLATFORM}/SDL2_mixer.pdb"
                $<TARGET_FILE_DIR:game>
        VERBATIM
    )
endif()

#
# Asset Baker
#
add_executable(
    asset_baker
    src/asset_baker.cpp
    src/common.h
    src/math_stuff.h
)

#
# Find Freetype2
#
if(WIN32)
    target_include_directories(asset_baker PRIVATE
        "${CMAKE_SOURCE_DIR}/vendor/freetype2/include"
    )

    target_link_libraries(asset_baker PRIVATE
        "${CMAKE_SOURCE_DIR}/vendor/freetype2/lib/${PLATFORM}/freetype.lib"
    )
else()
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(FREETYPE REQUIRED freetype2)
    target_include_directories(asset_baker PRIVATE ${FREETYPE_INCLUDE_DIRS})
    target_compile_options(asset_baker PRIVATE ${FREETYPE_CFLAGS_OTHER})
    target_link_directories(asset_baker PRIVATE ${FREETYPE_LIBRARY_DIRS})
    target_link_libraries(asset_baker PRIVATE ${FREETYPE_LIBRARIES})
endif()

add_custom_command(
    OUTPUT ${ASSET_INFO_GENERATED_H}
           ${ASSET_INFO_GENERATED_CPP}
           ${SINE_TABLE_GENERATED_CPP}
    COMMAND ${CMAKE_COMMAND} -E make_directory assets_baked
    COMMAND ${CMAKE_COMMAND} -E make_directory assets_baked/audio
    COMMAND ${CMAKE_COMMAND} -E make_directory assets_baked/fonts
    COMMAND ${CMAKE_COMMAND} -E make_directory assets_baked/images
    COMMAND ${CMAKE_COMMAND} -E make_directory assets_baked/misc
    COMMAND ${CMAKE_COMMAND} -E make_directory assets_baked/videos
    COMMAND asset_baker
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS asset_baker
            assets_raw/characters.png
            assets_raw/projectiles.png
            assets_raw/background.png
            assets_raw/background_2.png
            assets_raw/background_3.png
            assets_raw/enemies.png
            assets_raw/white.png
            assets_raw/gfw_misty_lake.png
            assets_raw/gfw_misty_lake2.png
            assets_raw/gfw_misty_lake3.png
            assets_raw/gfw_misty_lake4.png
)

# You can set "asset_baker" as the startup project and just hit F5 to debug it
set_property(TARGET asset_baker PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

#
# Common Flags
#
if(MSVC)
    #
    # /fp:fast   - use the fast floating point model
    # /Oi        - Generate Intrinsic Functions
    # /W4        - warning level 4
    # /WX        - warnings as errors
    # /std:c++17 - needed for /we5262
    #
    # warning C4201: nonstandard extension used: nameless struct/union
    # warning C4100: 'Foo': unreferenced parameter
    # warning C4189: 'Foo': local variable is initialized but not referenced
    # warning C4127: conditional expression is constant
    # warning C4505: 'Foo': unreferenced function with internal linkage has been removed
    # warning C4200: nonstandard extension used: zero-sized array in struct/union
    #
    # error C4062: enumerator 'Foo_Bar' in switch of enum 'Foo' is not handled
    # error C5262: implicit fall-through occurs here; are you missing a break statement? NOTE: needs c++17
    #
    set(MSVC_FLAGS
        /fp:fast /Oi /W4 /WX /std:c++17
        /wd4201 /wd4100 /wd4189 /wd4127 /wd4505 /wd4200
        /we4062 /we5262
    )

    set(MSVC_LINK_FLAGS /SAFESEH:NO)

    target_compile_options(
        game PRIVATE
        ${MSVC_FLAGS}
    )
    target_compile_options(
        asset_baker PRIVATE
        ${MSVC_FLAGS}
    )
    target_link_options(
        game PRIVATE
        ${MSVC_LINK_FLAGS}
    )
    target_link_options(
        asset_baker PRIVATE
        ${MSVC_LINK_FLAGS}
    )
endif()

#
# Flags specific for game
#
if(MSVC)
    #
    # /EHs-c-  - Disable exceptions
    # /GS-     - Disable buffer security check
    # /Gm-     - Disable Minimal Rebuild (NOTE: this option is deprecated)
    # /GR-     - Disable Run-Time Type Information
    #
    set(MSVC_RELEASE_FLAGS_GAME /MD /EHs-c- /GS- /Gm- /GR-)

    set(MSVC_DEBUG_FLAGS_GAME /ZI)

    set(MSVC_RELEASE_DEFINITIONS_GAME
        _HAS_EXCEPTIONS=0
        WIN32_BUILD_WITHOUT_LIBC=1
    )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MSVC_LINK_FLAGS_GAME /SUBSYSTEM:CONSOLE)
    else()
        set(MSVC_LINK_FLAGS_GAME /SUBSYSTEM:CONSOLE,5.1) # windows xp build
    endif()

    #
    # /OPT:ICF  - Perform identical code folding
    # /OPT:REF  - remove unreferenced functions/globals
    #
    set(MSVC_RELEASE_LINK_FLAGS_GAME /INCREMENTAL:NO /OPT:ICF /OPT:REF /MAP)

    set(MSVC_RELEASE_LIBS_GAME
        libvcruntime.lib
        ucrt.lib
    )

    target_compile_options(
        game PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:${MSVC_RELEASE_FLAGS_GAME}>
        $<$<CONFIG:Debug>:${MSVC_RELEASE_FLAGS_GAME}>
    )
    target_compile_definitions(
        game PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:${MSVC_RELEASE_DEFINITIONS_GAME}>
    )
    target_link_options(
        game PRIVATE
        ${MSVC_LINK_FLAGS_GAME}
        $<$<NOT:$<CONFIG:Debug>>:${MSVC_RELEASE_LINK_FLAGS_GAME}>
    )
    target_link_libraries(
        game PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:${MSVC_RELEASE_LIBS_GAME}>
    )
endif()

# target_compile_options(
#     game PRIVATE
#     /Bt+ /d2cgsummary /d1reportTime
# )

target_compile_definitions(
    game PRIVATE
    ENABLE_DEBUG_PROFILER=1
)
