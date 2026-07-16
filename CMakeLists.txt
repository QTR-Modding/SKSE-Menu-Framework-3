# ---------------------------------------------------- #
#                  CommonLibSSE-NG source              #
# ---------------------------------------------------- #
# Prefer the COMMONLIB_SSE_FOLDER env var (a shared local clone) when set; else
# fall back to the vendored submodule so the build is self-contained (CI, fresh
# contributors) with a pinned, static dependency.
if(DEFINED ENV{COMMONLIB_SSE_FOLDER})
  set(COMMONLIBSSE_DIR "$ENV{COMMONLIB_SSE_FOLDER}")
else()
  set(COMMONLIBSSE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
endif()
if(NOT EXISTS "${COMMONLIBSSE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "CommonLibSSE-NG not found at '${COMMONLIBSSE_DIR}'. Set COMMONLIB_SSE_FOLDER or run: git submodule update --init --recursive")
endif()

# ---------------------------------------------------- #
#               Imports and definitions                #
# ---------------------------------------------------- #

include(cmake/lib/copyOutputs.cmake)
include(cmake/lib/automaticGameFolderOutput.cmake)
include(cmake/lib/autoCollectSources.cmake)


add_src_folder(src)
add_include_folder(include)


cmake_minimum_required(VERSION 3.21)
set(AUTHOR_NAME "SkyrimThiago")
set(PRODUCT_NAME "SKSEMenuFramework")
set(BEAUTIFUL_NAME "SKSE Menu Framework")

project(${PRODUCT_NAME} VERSION 3.13.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# ---------------------------------------------------- #
#                      Resources                       #
# ---------------------------------------------------- #

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

set(RESOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
set(RESOURCE_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.res)

add_custom_command(
    OUTPUT ${RESOURCE_OUTPUT}
    COMMAND rc /fo ${RESOURCE_OUTPUT} ${RESOURCE_FILE}
    DEPENDS ${RESOURCE_FILE}
)

add_custom_target(Resource ALL DEPENDS ${RESOURCE_OUTPUT})

# ---------------------------------------------------- #
#                Plugin Configuration                  #
# ---------------------------------------------------- #

include(FetchContent)

set(ENABLE_SKYRIM_SE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL " " FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL " " FORCE)
set(BUILD_TESTS OFF CACHE BOOL " " FORCE)
set(VALIDATE_LICENSE OFF CACHE BOOL " " FORCE)

add_subdirectory("${COMMONLIBSSE_DIR}" ${CMAKE_CURRENT_BINARY_DIR}/_deps/clib-build)
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)

find_path(CLIB_UTIL_INCLUDE_DIRS "ClibUtil/utils.hpp")
file(
  GLOB_RECURSE
  sources
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  ${sources}
    imgui/backends/imgui_impl_dx11.cpp
    imgui/backends/imgui_impl_win32.cpp
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)

source_group(
  TREE ${CMAKE_CURRENT_SOURCE_DIR}
  FILES ${headers} ${sources}
)

set(BUILD_TESTS OFF)
include(${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(
  ${PROJECT_NAME}
  AUTHOR AUTHOR_NAME
  SOURCES ${headers} ${sources}
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE include/PCH.h)
target_compile_definitions(${PROJECT_NAME}  PRIVATE BEAUTIFUL_NAME="${BEAUTIFUL_NAME}" IMGUI_DEFINE_MATH_OPERATORS)

# ---------------------------------------------------- #
#                ImGuiVRHelper client API              #
# ---------------------------------------------------- #
# Pulled from upstream instead of vendored, so the dependency and its pinned
# version are explicit. The helper builds with xmake; we configure only its api/
# CMake target (SOURCE_SUBDIR) and skip the helper's own submodules. The INTERFACE
# target compiles the handshake stub into this plugin, inheriting our CommonLib /
# ImGui / DX11 / json. Override with -DFETCHCONTENT_SOURCE_DIR_IMGUIVRHELPER=<path>
# to build against a local checkout.
FetchContent_Declare(
    ImGuiVRHelper
    GIT_REPOSITORY https://github.com/alandtse/imgui-vr-helper.git
    GIT_TAG        v1.8.0
    GIT_SUBMODULES ""
    SOURCE_SUBDIR  api
)
FetchContent_MakeAvailable(ImGuiVRHelper)
target_link_libraries(${PROJECT_NAME} PRIVATE ImGuiVRHelper::api)


set(header_dirs "")

foreach(header ${headers})
    get_filename_component(dir ${header} DIRECTORY)
    list(APPEND header_dirs ${dir})
endforeach()

list(REMOVE_DUPLICATES header_dirs)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
    ${header_dirs}
    ${CLIB_UTIL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)


find_package(NanoSVG REQUIRED)

find_library(DIRECTXTK_LIBRARY NAMES DirectXTK PATHS ${CMAKE_BINARY_DIR}/debug/vcpkg_installed/x64-windows-static/lib)
target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
        NanoSVG::nanosvg 
        NanoSVG::nanosvgrast
        ${DIRECTXTK_LIBRARY}
)




# ---------------------------------------------------- #
#                        Output                        #
# ---------------------------------------------------- #

set(wildlander_output false)
set(steam_owrt_output false)
set(steam_mods_output true)
automaticGameFolderOutput(wildlander_output, steam_owrt_output, steam_mods_output)