add_executable(hw1
    src/main.cpp
    src/ui_renderer.cpp
    src/ui_renderer.h
    src/ui_bridge.h
)

target_link_libraries(hw1 PRIVATE microui_lib minifb)
target_include_directories(hw1 PRIVATE
    src
    ${microui_SOURCE_DIR}/src
    ${minifb_SOURCE_DIR}/include
)
