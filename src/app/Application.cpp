#define GL_SILENCE_DEPRECATION

#include "Application.h"

#include <cstdio>

#include <imgui.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColor.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkRRect.h>
#include <skia/core/SkRect.h>

namespace {
void GlfwErrorCallback(int error, const char *description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}
} // namespace

bool Application::Init() {
    glfwSetErrorCallback(GlfwErrorCallback);

    if (!glfwInit()) {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    window_ = glfwCreateWindow(1280, 720, "TCanvas", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    if (!skia_.Init()) {
        return false;
    }

    return imgui_.Init(window_);
}

void Application::Run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(window_, &windowWidth, &windowHeight);
        glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
        const float dpr = static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth);

        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.02f, 0.02f, 0.025f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        imgui_.BeginFrame();
        HandleInput();
        Render(dpr, framebufferWidth, framebufferHeight);
        imgui_.EndFrame();

        glfwSwapBuffers(window_);
    }
}

void Application::HandleInput() {
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }

    const Vec2 mouse{io.MousePos.x, io.MousePos.y};
    const Vec2 mouseWorld = view_.ScreenToWorld(mouse);

    if (io.MouseWheel != 0.0f && mouse.x >= 0.0f && mouse.y >= 0.0f) {
        view_.ZoomAt(mouse, io.MouseWheel);
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        DragMode mode = DragMode::None;
        Shape *selectedShape = document_.SelectedShape();
        if (selectedShape) {
            mode = transformer_.HitTest(mouse, *selectedShape, view_);
        }

        if (mode == DragMode::None) {
            const auto &shapes = document_.Shapes();
            for (int i = static_cast<int>(shapes.size()) - 1; i >= 0; --i) {
                if (PointInShape(mouseWorld, shapes[i])) {
                    document_.SelectShape(i);
                    selectedShape = document_.SelectedShape();
                    mode = DragMode::Move;
                    break;
                }
            }
        }

        if (mode == DragMode::None) {
            document_.SelectShape(-1);
            transformer_.EndDrag();
            selectedShape = nullptr;
        }

        if (selectedShape && mode != DragMode::None) {
            transformer_.BeginDrag(mode, mouseWorld, *selectedShape);
        }
    }

    if (Shape *selectedShape = document_.SelectedShape();
        selectedShape && ImGui::IsMouseDown(ImGuiMouseButton_Left) && transformer_.ActiveMode() != DragMode::None) {
        transformer_.UpdateDrag(mouseWorld, *selectedShape);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        transformer_.EndDrag();
    }

    if (io.MouseDown[1] && transformer_.ActiveMode() == DragMode::None) {
        if (!view_.isPanning) {
            view_.isPanning = true;
            view_.lastMouse = mouse;
        } else {
            const Vec2 delta = mouse - view_.lastMouse;
            view_.pan = view_.pan + delta;
            view_.lastMouse = mouse;
        }
    } else {
        view_.isPanning = false;
    }
}

void Application::RenderPanels() {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    constexpr float leftWidth = 240.0f;
    constexpr float rightWidth = 300.0f;
    constexpr ImGuiWindowFlags panelFlags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(leftWidth, viewport->WorkSize.y));
    ImGui::Begin("Layers", nullptr, panelFlags);

    ImGui::TextUnformatted("Layers");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 92.0f);
    if (ImGui::Button("+ Rectangle", ImVec2(100.0f, 0.0f))) {
        document_.AddRectangle();
        transformer_.EndDrag();
    }
    ImGui::Separator();

    const auto &shapes = document_.Shapes();
    for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
        const bool selected = document_.SelectedShapeIndex() == i;
        ImGui::PushID(i);
        if (ImGui::Selectable(shapes[i].name.c_str(), selected)) {
            document_.SelectShape(i);
            transformer_.EndDrag();
        }
        ImGui::PopID();
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - rightWidth, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(rightWidth, viewport->WorkSize.y));
    ImGui::Begin("Inspector", nullptr, panelFlags);

    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();
    Shape *shape = document_.SelectedShape();
    if (shape) {
        char nameBuffer[128] = {};
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", shape->name.c_str());
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            shape->name = nameBuffer;
        }

        ImGui::SeparatorText("Transform");
        ImGui::DragFloat2("Position", &shape->position.x, 1.0f);
        ImGui::DragFloat2("Size", &shape->size.x, 1.0f, 20.0f, 4000.0f);
        ImGui::DragFloat("Rotation", &shape->rotation, 0.5f, -360.0f, 360.0f);

        ImGui::SeparatorText("Appearance");
        ImGui::ColorEdit4("Fill", &shape->fill.r);
        ImGui::ColorEdit4("Border", &shape->border.r);
        ImGui::DragFloat("Border Width", &shape->borderWidth, 0.25f, 0.0f, 100.0f);
        ImGui::DragFloat("Radius", &shape->cornerRadius, 0.5f, 0.0f, 1000.0f);

        shape->size.x = Clamp(shape->size.x, 20.0f, 4000.0f);
        shape->size.y = Clamp(shape->size.y, 20.0f, 4000.0f);
        shape->borderWidth = Clamp(shape->borderWidth, 0.0f, 100.0f);
        shape->cornerRadius = Clamp(shape->cornerRadius, 0.0f, 1000.0f);
    } else {
        ImGui::TextUnformatted("No selection");
    }

    ImGui::End();
}

void Application::Render(float dpr, int framebufferWidth, int framebufferHeight) {
    SkCanvas *canvas = skia_.BeginFrame(framebufferWidth, framebufferHeight);
    if (!canvas) {
        return;
    }

    canvas->clear(SK_ColorBLACK);

    canvas->save();
    canvas->scale(dpr, dpr);
    canvas->translate(view_.pan.x, view_.pan.y);
    canvas->scale(view_.zoom, view_.zoom);

    for (const Shape &shape: document_.Shapes()) {
        canvas->save();
        canvas->translate(shape.position.x, shape.position.y);
        canvas->rotate(shape.rotation);

        const float radius = Clamp(shape.cornerRadius, 0.0f, std::min(shape.size.x, shape.size.y) * 0.5f);
        const SkRect rect = SkRect::MakeXYWH(
            -shape.size.x * 0.5f,
            -shape.size.y * 0.5f,
            shape.size.x,
            shape.size.y
        );
        SkPaint fill;
        fill.setAntiAlias(true);
        fill.setStyle(SkPaint::kFill_Style);
        fill.setColor(SkColorSetARGB(
            static_cast<U8CPU>(shape.fill.a * 255.0f),
            static_cast<U8CPU>(shape.fill.r * 255.0f),
            static_cast<U8CPU>(shape.fill.g * 255.0f),
            static_cast<U8CPU>(shape.fill.b * 255.0f)
        ));
        if (radius > 0.0f) {
            canvas->drawRRect(SkRRect::MakeRectXY(rect, radius, radius), fill);
        } else {
            canvas->drawRect(rect, fill);
        }

        if (shape.borderWidth > 0.0f) {
            SkPaint border;
            border.setAntiAlias(true);
            border.setStyle(SkPaint::kStroke_Style);
            border.setStrokeWidth(shape.borderWidth);
            border.setColor(SkColorSetARGB(
                static_cast<U8CPU>(shape.border.a * 255.0f),
                static_cast<U8CPU>(shape.border.r * 255.0f),
                static_cast<U8CPU>(shape.border.g * 255.0f),
                static_cast<U8CPU>(shape.border.b * 255.0f)
            ));
            if (radius > 0.0f) {
                canvas->drawRRect(SkRRect::MakeRectXY(rect, radius, radius), border);
            } else {
                canvas->drawRect(rect, border);
            }
        }

        canvas->restore();
    }
    canvas->restore();

    if (const Shape *selectedShape = document_.SelectedShape()) {
        canvas->save();
        canvas->scale(dpr, dpr);
        transformer_.Draw(canvas, *selectedShape, view_);
        canvas->restore();
    }

    skia_.EndFrame();

    RenderPanels();
}

void Application::Shutdown() {
    imgui_.Shutdown();
    skia_.Shutdown();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}
