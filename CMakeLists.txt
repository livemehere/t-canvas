cmake_minimum_required(VERSION 4.2)

project(TCanvas LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)

add_executable(app
        src/main.cpp
        src/app/Application.cpp
        src/canvas/CanvasDocument.cpp
        src/canvas/CanvasView.cpp
        src/canvas/Shape.cpp
        src/canvas/Transformer.cpp
        src/platform/MacFileDialog.mm
        src/render/ImGuiLayer.cpp
        src/render/SkiaRenderer.cpp
)

target_link_libraries(app PRIVATE
        OpenGL::GL
        glfw
        imgui::imgui
        unofficial::skia::skia
        "-framework AppKit"
        "-framework UniformTypeIdentifiers"
)
