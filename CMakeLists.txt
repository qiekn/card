cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_MODULE_STD OFF)

project(card LANGUAGES C CXX)

# Default to Release for single-config generators when caller didn't specify
# CMAKE_BUILD_TYPE (suppresses raylib's default-build warning too).
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Hardcoded to libc++ (Clang). See README for MSVC / non-libc++ notes.
if (NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libraries as shared" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Submodule sanity checks
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/raylib/CMakeLists.txt")
  message(FATAL_ERROR
    "raylib submodule is missing. Run: git submodule update --init --recursive")
endif()

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/imgui.h")
  message(FATAL_ERROR
    "imgui submodule is missing. Run: git submodule update --init --recursive")
endif()

foreach(_dep IN ITEMS
    "deps/entt/single_include/entt/entt.hpp"
    "deps/json/single_include/nlohmann/json.hpp"
    "deps/magic-enum/include/magic_enum/magic_enum.hpp"
    "deps/raygui/src/raygui.h")
  if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_dep}")
    message(FATAL_ERROR
      "Submodule header missing: ${_dep}. "
      "Run: git submodule update --init --recursive")
  endif()
endforeach()

add_subdirectory(deps/raylib)

add_library(imgui STATIC
  deps/imgui/imgui.cpp
  deps/imgui/imgui_draw.cpp
  deps/imgui/imgui_tables.cpp
  deps/imgui/imgui_widgets.cpp
  deps/imgui/imgui_demo.cpp
  deps/imgui/backends/imgui_impl_glfw.cpp
  deps/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC deps/imgui deps/imgui/backends)
target_include_directories(imgui PRIVATE deps/raylib/src/external/glfw/include)

add_executable(${PROJECT_NAME}
  src/engine/atlas_registry.cpp
  src/engine/movable.cpp
  src/engine/sprite.cpp
  src/engine/text.cpp
  src/game/card.cpp
  src/game/cardarea.cpp
  src/game.cpp
  src/game_layer.cpp
  src/imgui_layer.cpp
  src/layer_stack.cpp
  src/main.cpp
)

target_include_directories(${PROJECT_NAME}
  PRIVATE
    deps/entt/single_include
    deps/json/single_include
    deps/magic-enum/include
    deps/raygui/src
    deps/raylib/src/external/glfw/include
    src
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    raylib
    imgui
)

if (APPLE)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      "-framework IOKit"
      "-framework Cocoa"
      "-framework OpenGL"
  )
endif()
