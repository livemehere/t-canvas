#pragma once

#include <GLFW/glfw3.h>
#include <skia/core/SkSurface.h>
#include <skia/gpu/ganesh/GrDirectContext.h>

class SkiaRenderer {
public:
    bool Init();
    SkCanvas *BeginFrame(int framebufferWidth, int framebufferHeight);
    void EndFrame();
    void Shutdown();

private:
    sk_sp<GrDirectContext> context_;
    sk_sp<SkSurface> surface_;
};
