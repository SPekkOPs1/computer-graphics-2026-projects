add_executable(hw2
    src/main.cpp
    src/ui_renderer.cpp
    src/ui_renderer.h
    src/ui_bridge.h
)

target_link_libraries(hw2 PRIVATE microui_lib minifb glm::glm)
target_include_directories(hw2 PRIVATE
    src
    ${microui_SOURCE_DIR}/src
    ${minifb_SOURCE_DIR}/include
)
