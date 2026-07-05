#define GL_SILENCE_DEPRECATION

#include "SkiaRenderer.h"

#include <GLFW/glfw3.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/ganesh/gl/GrGLInterface.h>

bool SkiaRenderer::Init() {
    auto glInterface = GrGLMakeNativeInterface();
    context_ = GrDirectContexts::MakeGL(glInterface);
    return context_ != nullptr;
}

SkCanvas *SkiaRenderer::BeginFrame(int framebufferWidth, int framebufferHeight) {
    GrGLFramebufferInfo framebufferInfo;
    framebufferInfo.fFBOID = 0;
    framebufferInfo.fFormat = GL_RGBA8;

    GrBackendRenderTarget renderTarget = GrBackendRenderTargets::MakeGL(
        framebufferWidth,
        framebufferHeight,
        0,
        8,
        framebufferInfo
    );

    surface_ = SkSurfaces::WrapBackendRenderTarget(
        context_.get(),
        renderTarget,
        kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType,
        nullptr,
        nullptr
    );

    if (!surface_) {
        return nullptr;
    }

    return surface_->getCanvas();
}

void SkiaRenderer::EndFrame() {
    if (surface_) {
        context_->flushAndSubmit(surface_.get());
        surface_.reset();
    }
}

void SkiaRenderer::Shutdown() {
    surface_.reset();
    context_.reset();
}
