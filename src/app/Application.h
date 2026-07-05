#pragma once

#include <GLFW/glfw3.h>

#include "../canvas/CanvasDocument.h"
#include "../canvas/CanvasView.h"
#include "../canvas/Transformer.h"
#include "../render/ImGuiLayer.h"
#include "../render/SkiaRenderer.h"

class Application {
public:
    bool Init();
    void Run();
    void Shutdown();

private:
    GLFWwindow *window_ = nullptr;
    SkiaRenderer skia_;
    ImGuiLayer imgui_;
    CanvasDocument document_;
    CanvasView view_;
    Transformer transformer_;

    void HandleInput();
    void Render(float dpr, int framebufferWidth, int framebufferHeight);
    void RenderPanels();
};
