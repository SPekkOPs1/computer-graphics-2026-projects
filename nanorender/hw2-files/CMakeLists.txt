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

# Bake the absolute assets path so the exe can find .obj files regardless of
# what directory it is launched from.
target_compile_definitions(hw2 PRIVATE
    "HW2_ASSETS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\""
)
