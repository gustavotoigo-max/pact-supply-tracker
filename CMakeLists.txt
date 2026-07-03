cmake_minimum_required(VERSION 3.24)

project(PactSupplyTracker
    VERSION 1.2.1.0
    LANGUAGES CXX
)

add_library(imgui STATIC
    dependencies/imgui/imgui.cpp
    dependencies/imgui/imgui_draw.cpp
    dependencies/imgui/imgui_tables.cpp
    dependencies/imgui/imgui_widgets.cpp
)

target_compile_features(imgui PUBLIC cxx_std_20)

target_include_directories(imgui PUBLIC
    dependencies/imgui
)

if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
endif()

add_library(gw2_nexus_addon SHARED
    src/addon.cpp
    resources.rc
)

target_compile_features(gw2_nexus_addon PRIVATE cxx_std_20)

target_include_directories(gw2_nexus_addon PRIVATE
    include
    dependencies/nexus-api
)

target_link_libraries(gw2_nexus_addon PRIVATE
    imgui
)

target_compile_definitions(gw2_nexus_addon PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_link_options(gw2_nexus_addon PRIVATE /MAP /MAPINFO:EXPORTS)
endif()

if(MSVC)
    target_compile_options(gw2_nexus_addon PRIVATE
        /W4
        /permissive-
        /EHsc
    )
else()
    target_compile_options(gw2_nexus_addon PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

set_target_properties(gw2_nexus_addon PROPERTIES
    OUTPUT_NAME "pact_supply_tracker"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

