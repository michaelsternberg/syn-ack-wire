cmake_minimum_required(VERSION 3.24)
project(syn_ack_wire_desktop VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(MSVC)
  add_compile_options(/utf-8)
endif()

set(CLIENT ${CMAKE_CURRENT_SOURCE_DIR}/../client)
include(FetchContent)

# --- SDL3: the system's when there is one, else built from source (and linked in statically).
# SAW_FETCH_DEPS=ON always builds SDL3 and FreeType in, for a program that runs anywhere ---
option(SAW_FETCH_DEPS "Build SDL3 and FreeType from source and link them in" OFF)
if(NOT SAW_FETCH_DEPS)
  find_package(SDL3 CONFIG QUIET)
endif()
if(NOT SDL3_FOUND)
  message(STATUS "SDL3 not found on the system; fetching it")
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)  # linked in: its headers and library stay out of the package
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.10
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

find_package(OpenGL REQUIRED)

# --- FreeType, for the fonts: the system's, else built from source ---
if(NOT SAW_FETCH_DEPS)
  find_package(Freetype QUIET)
endif()
if(NOT Freetype_FOUND)
  message(STATUS "FreeType not found on the system; fetching it")
  FetchContent_Declare(freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        VER-2-13-3
    GIT_SHALLOW    TRUE)
  foreach(dep HARFBUZZ BROTLI PNG BZIP2 ZLIB)
    set(FT_DISABLE_${dep} ON CACHE BOOL "" FORCE)
  endforeach()
  set(SKIP_INSTALL_ALL ON)
  FetchContent_MakeAvailable(freetype)
  add_library(Freetype::Freetype ALIAS freetype)
endif()

# --- Dear ImGui (1.92+ for fonts at any size): the menu bar and dialogs ---
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.92.9
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/freetype)
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE IMGUI_DISABLE_DEMO_WINDOWS)
target_link_libraries(imgui PUBLIC SDL3::SDL3 OpenGL::GL Freetype::Freetype)

# --- Generated headers. The client's, into its source tree as its Makefile makes them (they're
# gitignored there): names and terms from theme/theme.toml, frame lengths from the server's
# proto.py, the MS-DOS title logo and circuit-board margins. The Region menu's servers, from
# server/servers.fly.toml, into the build folder ---
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)  # tomllib
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SAW_GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
function(saw_generate out)
  cmake_parse_arguments(G "" "" "ARGS;DEPENDS" ${ARGN})
  add_custom_command(OUTPUT ${out}
    COMMAND ${CMAKE_COMMAND} -DPYTHON=${Python3_EXECUTABLE} -DOUT=${out} "-DARGS=${G_ARGS}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen.cmake
    DEPENDS ${G_DEPENDS} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen.cmake
    VERBATIM)
endfunction()
saw_generate(${CLIENT}/src/common/theme_tab.h ARGS ${ROOT}/tools/gen_theme.py c
  DEPENDS ${ROOT}/theme/theme.toml ${ROOT}/tools/gen_theme.py)
saw_generate(${CLIENT}/src/common/proto_tab.h ARGS ${ROOT}/tools/gen_proto.py h
  DEPENDS ${ROOT}/server/acquire/proto.py ${ROOT}/tools/gen_proto.py)
saw_generate(${CLIENT}/src/msdos/title_art.h ARGS ${CLIENT}/tools/title_art.py d 0 ${CLIENT}/src/atari/title4.atascii
  DEPENDS ${CLIENT}/src/atari/title4.atascii ${CLIENT}/tools/title_art.py)
saw_generate(${CLIENT}/src/msdos/margins.h ARGS ${CLIENT}/tools/margins_msdos.py ${CLIENT}/src/msdos/margins.txt
  DEPENDS ${CLIENT}/src/msdos/margins.txt ${CLIENT}/tools/margins_msdos.py)
saw_generate(${SAW_GEN}/regions_tab.h ARGS ${ROOT}/tools/gen_regions.py h
  DEPENDS ${ROOT}/server/servers.fly.toml ${ROOT}/fly.toml ${ROOT}/tools/gen_regions.py)
add_custom_target(saw_headers DEPENDS
  ${CLIENT}/src/common/theme_tab.h ${CLIENT}/src/common/proto_tab.h
  ${CLIENT}/src/msdos/title_art.h ${CLIENT}/src/msdos/margins.h ${SAW_GEN}/regions_tab.h)

# --- The game: the 8-bit clients' common code, as it is, and this platform's half (desk.h) ---
# main.c's main() runs on the game thread as game_main(). Its char arithmetic is cc65's: unsigned.
add_library(saw_game STATIC
  ${CLIENT}/src/common/main.c
  ${CLIENT}/src/common/game.c
  ${CLIENT}/src/common/proto.c
  src/desk.c
  src/display_text.c
  src/net.c
  src/seats.c
  src/sound.c)
target_compile_definitions(saw_game PRIVATE
  __fastcall__= main=game_main DISPLAY_IDLE=1 QUIT_TO_OS=1 "QUIT_ASK=\"QUIT? Y/N\"")
target_compile_options(saw_game PRIVATE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-funsigned-char -Wall -Wno-main>
  $<$<C_COMPILER_ID:MSVC>:/J /W3 /wd4244 /wd4267 /D_CRT_SECURE_NO_WARNINGS>)
target_link_libraries(saw_game PUBLIC SDL3::SDL3)
add_dependencies(saw_game saw_headers)
if(WIN32)
  target_link_libraries(saw_game PUBLIC ws2_32)
endif()

# --- The instructions: the help pages as screen images (client/tools/help_msdos.py). The desktop's
# own pages are client/help/desktop/screen/ once there are any; until then the MS-DOS ones ---
if(EXISTS ${CLIENT}/help/desktop/screen/HELP01.TXT)
  set(SAW_HELP_DIR ${CLIENT}/help/desktop/screen)
  set(SAW_QR_URL "HTTPS://SYN-ACK-WIRE.FLY.DEV/DESKTOP" CACHE STRING "The QR code on the help pages")
else()
  set(SAW_HELP_DIR ${CLIENT}/help/msdos/screen)
  set(SAW_QR_URL "HTTPS://SYN-ACK-WIRE.FLY.DEV/MSDOS" CACHE STRING "The QR code on the help pages")
endif()
file(GLOB SAW_HELP_PAGES CONFIGURE_DEPENDS ${SAW_HELP_DIR}/HELP*.TXT)
list(SORT SAW_HELP_PAGES)
set(SAW_DATA ${CMAKE_CURRENT_BINARY_DIR}/data)
add_custom_command(
  OUTPUT ${SAW_DATA}/SYNACK.HLP
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SAW_DATA}
  COMMAND ${Python3_EXECUTABLE} ${CLIENT}/tools/help_msdos.py --qr ${SAW_QR_URL} ${SAW_DATA}/SYNACK.HLP ${SAW_HELP_PAGES}
  DEPENDS ${SAW_HELP_PAGES} ${CLIENT}/tools/help_msdos.py ${CLIENT}/tools/qr.py
  COMMENT "The help pages"
  VERBATIM)
add_custom_target(saw_help DEPENDS ${SAW_DATA}/SYNACK.HLP)

# --- The window ---
set(SAW_WARNINGS $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra> $<$<CXX_COMPILER_ID:MSVC>:/W4>)
add_library(saw_theme STATIC src/theme.cpp)
target_include_directories(saw_theme PUBLIC src)
target_link_libraries(saw_theme PUBLIC imgui)

add_executable(syn-ack-wire WIN32 MACOSX_BUNDLE
  src/main.cpp
  src/app.cpp
  src/crt.cpp
  src/fonts.cpp
  src/glyphs.cpp
  src/settings.cpp)
target_link_libraries(syn-ack-wire PRIVATE saw_game saw_theme imgui SDL3::SDL3 OpenGL::GL)
target_compile_options(syn-ack-wire PRIVATE ${SAW_WARNINGS})
target_include_directories(syn-ack-wire PRIVATE ${SAW_GEN})
target_compile_definitions(syn-ack-wire PRIVATE SAW_VERSION="${PROJECT_VERSION}"
  SAW_SOURCE_DATA_DIR="${SAW_DATA}")
add_dependencies(syn-ack-wire saw_help saw_headers)
set_target_properties(syn-ack-wire PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "SYN-ACK WIRE"
  MACOSX_BUNDLE_GUI_IDENTIFIER "net.synackwire.desktop"
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

# The fonts and the help pages go in data/ beside the program (on macOS, in the bundle's Resources)
file(GLOB SAW_FONTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/*)
if(APPLE)
  set(SAW_RUN_DATA $<TARGET_BUNDLE_CONTENT_DIR:syn-ack-wire>/Resources/data)
else()
  set(SAW_RUN_DATA $<TARGET_FILE_DIR:syn-ack-wire>/data)
endif()
add_custom_command(TARGET syn-ack-wire POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SAW_RUN_DATA}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SAW_FONTS} ${SAW_DATA}/SYNACK.HLP ${SAW_RUN_DATA}
  VERBATIM)

install(TARGETS syn-ack-wire RUNTIME DESTINATION . BUNDLE DESTINATION .)
if(NOT APPLE)
  install(FILES ${SAW_FONTS} ${SAW_DATA}/SYNACK.HLP DESTINATION data)
endif()
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION .)

# --- Tests ---
enable_testing()
add_executable(theme_test tests/theme_test.cpp)
target_link_libraries(theme_test PRIVATE saw_theme)
add_test(NAME theme COMMAND theme_test)
