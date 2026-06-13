cmake_minimum_required(VERSION 3.14)
project(cg_assignments)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json so the IDE/IntelliSense can resolve include paths
# (e.g. the FetchContent'd GLM headers) without a manual c_cpp_properties.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Put every executable at the top of the build dir (e.g. build/Release/hw1.exe)
# instead of inside its add_subdirectory folder (build/Release/hw1-files/hw1.exe),
# so build_and_run.ps1 can find "$buildDir\$Homework.exe".
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(FetchContent)

# --- minifb ---
set(MINIFB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIFB_BUILD_VERSION_INFO OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    minifb
    GIT_REPOSITORY https://github.com/emoon/minifb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(minifb)

# --- microui ---
FetchContent_Declare(
    microui
    GIT_REPOSITORY https://github.com/rxi/microui.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(microui)

add_library(microui_lib STATIC ${microui_SOURCE_DIR}/src/microui.c)
target_include_directories(microui_lib PUBLIC ${microui_SOURCE_DIR}/src)

# --- GLM ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

add_subdirectory(hw1-files)
add_subdirectory(hw2-files)
