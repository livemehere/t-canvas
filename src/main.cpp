#define GL_SILENCE_DEPRECATION

#include <cstdio>
#include <cmath>
#include <array>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkRect.h>
#include <skia/core/SkSurface.h>

#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/GrDirectContext.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/ganesh/gl/GrGLInterface.h>

namespace {
    void GlfwErrorCallback(int error, const char *description) {
        std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
    }

    float Clamp(float value, float min, float max) {
        if (value < min) {
            return min;
        }
        if (value > max) {
            return max;
        }
        return value;
    }

    struct ViewState {
        ImVec2 pan{0.0f, 0.0f};
        ImVec2 lastMouse{0.0f, 0.0f};
        float zoom = 1.0f;
        bool isPanning = false;
    };

    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Shape {
        Vec2 position{180.0f, 130.0f};
        Vec2 size{200.0f, 100.0f};
        float rotation = 0.0f;
    };

    enum class DragMode {
        None,
        Move,
        ResizeTopLeft,
        ResizeTopRight,
        ResizeBottomRight,
        ResizeBottomLeft,
        Rotate
    };

    struct TransformerState {
        DragMode mode = DragMode::None;
        Vec2 startMouseWorld;
        Vec2 handleGrabOffset;
        Shape startShape;
        float rotationOffset = 0.0f;
    };

    Vec2 operator+(Vec2 a, Vec2 b) {
        return {a.x + b.x, a.y + b.y};
    }

    Vec2 operator-(Vec2 a, Vec2 b) {
        return {a.x - b.x, a.y - b.y};
    }

    Vec2 operator*(Vec2 a, float value) {
        return {a.x * value, a.y * value};
    }

    float Distance(Vec2 a, Vec2 b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    float DegreesToRadians(float degrees) {
        return degrees * 3.1415926535f / 180.0f;
    }

    float RadiansToDegrees(float radians) {
        return radians * 180.0f / 3.1415926535f;
    }

    Vec2 Rotate(Vec2 point, float degrees) {
        const float radians = DegreesToRadians(degrees);
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        return {
            point.x * c - point.y * s,
            point.x * s + point.y * c
        };
    }

    Vec2 LocalToWorld(Vec2 point, const Shape &shape) {
        return shape.position + Rotate(point, shape.rotation);
    }

    Vec2 WorldToLocal(Vec2 point, const Shape &shape) {
        return Rotate(point - shape.position, -shape.rotation);
    }

    Vec2 WorldToScreen(Vec2 point, const ViewState &view) {
        return {
            point.x * view.zoom + view.pan.x,
            point.y * view.zoom + view.pan.y
        };
    }

    Vec2 ScreenToWorld(Vec2 point, const ViewState &view) {
        return {
            (point.x - view.pan.x) / view.zoom,
            (point.y - view.pan.y) / view.zoom
        };
    }

    std::array<Vec2, 4> GetShapeCorners(const Shape &shape) {
        const float hw = shape.size.x * 0.5f;
        const float hh = shape.size.y * 0.5f;
        return {
            LocalToWorld({-hw, -hh}, shape),
            LocalToWorld({hw, -hh}, shape),
            LocalToWorld({hw, hh}, shape),
            LocalToWorld({-hw, hh}, shape)
        };
    }

    Vec2 Midpoint(Vec2 a, Vec2 b) {
        return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
    }

    Vec2 GetRotateHandle(const Shape &shape) {
        const auto corners = GetShapeCorners(shape);
        const Vec2 top = Midpoint(corners[0], corners[1]);
        const Vec2 outward = Rotate({0.0f, -40.0f}, shape.rotation);
        return top + outward;
    }

    Vec2 GetHandleWorldPosition(const Shape &shape, DragMode mode) {
        const auto corners = GetShapeCorners(shape);
        switch (mode) {
            case DragMode::ResizeTopLeft:
                return corners[0];
            case DragMode::ResizeTopRight:
                return corners[1];
            case DragMode::ResizeBottomRight:
                return corners[2];
            case DragMode::ResizeBottomLeft:
                return corners[3];
            case DragMode::Rotate:
                return GetRotateHandle(shape);
            default:
                return shape.position;
        }
    }

    bool PointInShape(Vec2 world, const Shape &shape) {
        const Vec2 local = WorldToLocal(world, shape);
        const float hw = shape.size.x * 0.5f;
        const float hh = shape.size.y * 0.5f;
        return local.x >= -hw && local.x <= hw && local.y >= -hh && local.y <= hh;
    }

    DragMode HitTestTransformer(Vec2 screen, const Shape &shape, const ViewState &view) {
        constexpr float handleRadius = 7.0f;
        const auto corners = GetShapeCorners(shape);
        const std::array<DragMode, 4> cornerModes{
            DragMode::ResizeTopLeft,
            DragMode::ResizeTopRight,
            DragMode::ResizeBottomRight,
            DragMode::ResizeBottomLeft
        };

        for (int i = 0; i < 4; ++i) {
            if (Distance(screen, WorldToScreen(corners[i], view)) <= handleRadius) {
                return cornerModes[i];
            }
        }

        if (Distance(screen, WorldToScreen(GetRotateHandle(shape), view)) <= handleRadius + 2.0f) {
            return DragMode::Rotate;
        }

        if (PointInShape(ScreenToWorld(screen, view), shape)) {
            return DragMode::Move;
        }

        return DragMode::None;
    }

    void ResizeFromCorner(Shape &shape, Vec2 mouseWorld, DragMode mode) {
        Vec2 anchorLocal;
        switch (mode) {
            case DragMode::ResizeTopLeft:
                anchorLocal = {shape.size.x * 0.5f, shape.size.y * 0.5f};
                break;
            case DragMode::ResizeTopRight:
                anchorLocal = {-shape.size.x * 0.5f, shape.size.y * 0.5f};
                break;
            case DragMode::ResizeBottomRight:
                anchorLocal = {-shape.size.x * 0.5f, -shape.size.y * 0.5f};
                break;
            case DragMode::ResizeBottomLeft:
                anchorLocal = {shape.size.x * 0.5f, -shape.size.y * 0.5f};
                break;
            default:
                return;
        }

        const Vec2 anchorWorld = LocalToWorld(anchorLocal, shape);
        const Vec2 anchorToMouseLocal = Rotate(mouseWorld - anchorWorld, -shape.rotation);
        const float newWidth = std::max(20.0f, std::abs(anchorToMouseLocal.x));
        const float newHeight = std::max(20.0f, std::abs(anchorToMouseLocal.y));
        const Vec2 anchorToCenterLocal{
            anchorToMouseLocal.x * 0.5f,
            anchorToMouseLocal.y * 0.5f
        };

        shape.size = {newWidth, newHeight};
        shape.position = anchorWorld + Rotate(anchorToCenterLocal, shape.rotation);
    }

    void DrawTransformer(SkCanvas *canvas, const Shape &shape, const ViewState &view) {
        const auto corners = GetShapeCorners(shape);

        SkPaint outline;
        outline.setAntiAlias(true);
        outline.setColor(SkColorSetARGB(255, 255, 255, 255));
        outline.setStyle(SkPaint::kStroke_Style);
        outline.setStrokeWidth(1.5f);

        SkPath path;
        Vec2 p = WorldToScreen(corners[0], view);
        path.moveTo(p.x, p.y);
        for (int i = 1; i < 4; ++i) {
            p = WorldToScreen(corners[i], view);
            path.lineTo(p.x, p.y);
        }
        path.close();
        canvas->drawPath(path, outline);

        SkPaint handle;
        handle.setAntiAlias(true);
        handle.setColor(SK_ColorWHITE);
        handle.setStyle(SkPaint::kFill_Style);

        SkPaint rotateHandle = handle;
        rotateHandle.setColor(SkColorSetARGB(255, 255, 220, 80));

        constexpr float half = 6.0f;
        for (Vec2 corner: corners) {
            const Vec2 screen = WorldToScreen(corner, view);
            canvas->drawRect(SkRect::MakeXYWH(screen.x - half, screen.y - half, half * 2.0f, half * 2.0f), handle);
        }

        const Vec2 top = WorldToScreen(Midpoint(corners[0], corners[1]), view);
        const Vec2 rotate = WorldToScreen(GetRotateHandle(shape), view);
        canvas->drawLine(top.x, top.y, rotate.x, rotate.y, outline);
        canvas->drawCircle(rotate.x, rotate.y, 7.0f, rotateHandle);
    }
} // namespace

int main() {
    glfwSetErrorCallback(GlfwErrorCallback);

    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(1280, 720, "TCanvas", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // skia
    auto glInterface = GrGLMakeNativeInterface();
    auto grContext = GrDirectContexts::MakeGL(glInterface);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410 core");

    ViewState view;
    Shape shape;
    TransformerState transformer;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        const float dpr = static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth);

        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.02f, 0.02f, 0.025f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!io.WantCaptureMouse) {
            const Vec2 mouse{io.MousePos.x, io.MousePos.y};
            const Vec2 mouseWorld = ScreenToWorld(mouse, view);

            if (io.MouseWheel != 0.0f && mouse.x >= 0.0f && mouse.y >= 0.0f) {
                const float oldZoom = view.zoom;
                const float zoomFactor = std::pow(1.12f, io.MouseWheel);
                view.zoom = Clamp(view.zoom * zoomFactor, 0.1f, 16.0f);

                const Vec2 worldBefore{
                    (mouse.x - view.pan.x) / oldZoom,
                    (mouse.y - view.pan.y) / oldZoom
                };
                view.pan = {
                    mouse.x - worldBefore.x * view.zoom,
                    mouse.y - worldBefore.y * view.zoom
                };
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                transformer.mode = HitTestTransformer(mouse, shape, view);
                transformer.startMouseWorld = mouseWorld;
                transformer.startShape = shape;
                transformer.handleGrabOffset = {0.0f, 0.0f};

                if (transformer.mode == DragMode::Rotate) {
                    const float mouseAngle = RadiansToDegrees(std::atan2(
                        mouseWorld.y - shape.position.y,
                        mouseWorld.x - shape.position.x
                    ));
                    transformer.rotationOffset = shape.rotation - mouseAngle;
                } else if (
                    transformer.mode == DragMode::ResizeTopLeft ||
                    transformer.mode == DragMode::ResizeTopRight ||
                    transformer.mode == DragMode::ResizeBottomRight ||
                    transformer.mode == DragMode::ResizeBottomLeft
                ) {
                    transformer.handleGrabOffset = mouseWorld - GetHandleWorldPosition(shape, transformer.mode);
                }
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && transformer.mode != DragMode::None) {
                switch (transformer.mode) {
                    case DragMode::Move: {
                        shape = transformer.startShape;
                        const Vec2 delta = mouseWorld - transformer.startMouseWorld;
                        shape.position = transformer.startShape.position + delta;
                        break;
                    }
                    case DragMode::ResizeTopLeft:
                    case DragMode::ResizeTopRight:
                    case DragMode::ResizeBottomRight:
                    case DragMode::ResizeBottomLeft:
                        shape = transformer.startShape;
                        ResizeFromCorner(shape, mouseWorld - transformer.handleGrabOffset, transformer.mode);
                        break;
                    case DragMode::Rotate: {
                        shape = transformer.startShape;
                        const float mouseAngle = RadiansToDegrees(std::atan2(
                            mouseWorld.y - transformer.startShape.position.y,
                            mouseWorld.x - transformer.startShape.position.x
                        ));
                        shape.rotation = mouseAngle + transformer.rotationOffset;
                        break;
                    }
                    case DragMode::None:
                        break;
                }
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                transformer.mode = DragMode::None;
            }

            if (io.MouseDown[1] && transformer.mode == DragMode::None) {
                if (!view.isPanning) {
                    view.isPanning = true;
                    view.lastMouse = {mouse.x, mouse.y};
                } else {
                    const ImVec2 delta{
                        mouse.x - view.lastMouse.x,
                        mouse.y - view.lastMouse.y
                    };
                    view.pan.x += delta.x;
                    view.pan.y += delta.y;
                    view.lastMouse = {mouse.x, mouse.y};
                }
            } else {
                view.isPanning = false;
            }
        }

        // skia
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
        auto surface = SkSurfaces::WrapBackendRenderTarget(
            grContext.get(),
            renderTarget,
            kBottomLeft_GrSurfaceOrigin,
            kRGBA_8888_SkColorType,
            nullptr,
            nullptr
        );
        SkCanvas *canvas = surface->getCanvas();
        canvas->clear(SK_ColorBLACK);
        canvas->save();
        canvas->scale(dpr, dpr);
        canvas->translate(view.pan.x, view.pan.y);
        canvas->scale(view.zoom, view.zoom);
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(SkColorSetARGB(255, 80, 180, 255));
        canvas->save();
        canvas->translate(shape.position.x, shape.position.y);
        canvas->rotate(shape.rotation);
        canvas->drawRect(SkRect::MakeXYWH(
                             -shape.size.x * 0.5f,
                             -shape.size.y * 0.5f,
                             shape.size.x,
                             shape.size.y
                         ), paint);
        canvas->restore();
        canvas->restore();

        canvas->save();
        canvas->scale(dpr, dpr);
        DrawTransformer(canvas, shape, view);
        canvas->restore();

        grContext->flushAndSubmit(surface.get());

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
