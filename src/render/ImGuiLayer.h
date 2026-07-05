#pragma once

struct GLFWwindow;

class ImGuiLayer {
public:
    bool Init(GLFWwindow *window);
    void BeginFrame();
    void EndFrame();
    void Shutdown();
};
