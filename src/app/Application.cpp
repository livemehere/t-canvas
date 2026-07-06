#define GL_SILENCE_DEPRECATION

#include "Application.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include <imgui.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColor.h>
#include <skia/core/SkData.h>
#include <skia/core/SkFont.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkRRect.h>
#include <skia/core/SkRect.h>
#include <skia/core/SkSamplingOptions.h>

#include "../platform/FileDialog.h"

namespace {
void GlfwErrorCallback(int error, const char *description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

SkColor ToSkColor(Color color) {
    return SkColorSetARGB(
        static_cast<U8CPU>(Clamp(color.a, 0.0f, 1.0f) * 255.0f),
        static_cast<U8CPU>(Clamp(color.r, 0.0f, 1.0f) * 255.0f),
        static_cast<U8CPU>(Clamp(color.g, 0.0f, 1.0f) * 255.0f),
        static_cast<U8CPU>(Clamp(color.b, 0.0f, 1.0f) * 255.0f)
    );
}

const char *ToolLabel(Application::Tool tool) {
    switch (tool) {
        case Application::Tool::Select:
            return "Select";
        case Application::Tool::Pan:
            return "Pan";
        case Application::Tool::Rect:
            return "Rect";
        case Application::Tool::Circle:
            return "Circle";
        case Application::Tool::Line:
            return "Line";
        case Application::Tool::Arrow:
            return "Arrow";
        case Application::Tool::Text:
            return "Text";
        case Application::Tool::Image:
            return "Image";
    }
    return "";
}

bool IsLineTool(Application::Tool tool) {
    return tool == Application::Tool::Line || tool == Application::Tool::Arrow;
}

void SetLineGeometry(Shape &shape, Vec2 start, Vec2 end) {
    const Vec2 delta = end - start;
    const float length = std::max(2.0f, std::sqrt(delta.x * delta.x + delta.y * delta.y));
    shape.position = Midpoint(start, end);
    shape.size = {length, 1.0f};
    shape.rotation = RadiansToDegrees(std::atan2(delta.y, delta.x));
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
    defaultCursor_ = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    handCursor_ = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

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
    spacePanActive_ = ImGui::IsKeyDown(ImGuiKey_Space);
    const bool panToolActive = activeTool_ == Tool::Pan || spacePanActive_;
    glfwSetCursor(window_, panToolActive && handCursor_ ? handCursor_ : defaultCursor_);

    if (io.WantCaptureMouse) {
        return;
    }

    const Vec2 mouse{io.MousePos.x, io.MousePos.y};
    const Vec2 mouseWorld = view_.ScreenToWorld(mouse);

    if (!io.WantCaptureKeyboard &&
        (ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Delete))) {
        document_.RemoveSelectedShape();
        transformer_.EndDrag();
        snapGuides_.clear();
        isDrawingLine_ = false;
    }

    if (io.MouseWheel != 0.0f && mouse.x >= 0.0f && mouse.y >= 0.0f) {
        view_.ZoomAt(mouse, io.MouseWheel);
    }

    if (panToolActive) {
        transformer_.EndDrag();
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (!isPanningCanvas_) {
                isPanningCanvas_ = true;
                view_.lastMouse = mouse;
            } else {
                const Vec2 delta = mouse - view_.lastMouse;
                view_.pan = view_.pan + delta;
                view_.lastMouse = mouse;
            }
        } else {
            isPanningCanvas_ = false;
        }
        return;
    }

    isPanningCanvas_ = false;

    if (isDrawingLine_) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            UpdateLineDrawing(mouseWorld);
            return;
        }
        FinishLineDrawing();
        return;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (activeTool_ != Tool::Select) {
            if (IsLineTool(activeTool_)) {
                BeginLineDrawing(activeTool_, mouseWorld);
            } else if (activeTool_ != Tool::Image) {
                AddShapeFromTool(activeTool_);
                if (Shape *shape = document_.SelectedShape()) {
                    shape->position = mouseWorld;
                }
                activeTool_ = Tool::Select;
            }
            return;
        }

        DragMode mode = DragMode::None;
        Shape *selectedShape = document_.SelectedShape();
        if (selectedShape) {
            mode = transformer_.HitTest(mouse, *selectedShape, view_);
        }

        if (mode == DragMode::None) {
            const auto &shapes = document_.Shapes();
            for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
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
        selectedShape && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) &&
        transformer_.ActiveMode() != DragMode::None) {
        transformer_.UpdateDrag(mouseWorld, *selectedShape);
        if (transformer_.ActiveMode() == DragMode::Move) {
            ApplySnapping(*selectedShape);
        } else {
            snapGuides_.clear();
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        transformer_.EndDrag();
        snapGuides_.clear();
    }

    view_.isPanning = false;
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
    ImGui::Separator();

    const auto &shapes = document_.Shapes();
    for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
        const bool selected = document_.SelectedShapeIndex() == i;
        ImGui::PushID(i);
        if (ImGui::Selectable(shapes[i].name.c_str(), selected)) {
            document_.SelectShape(i);
            transformer_.EndDrag();
        }
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("SHAPE_INDEX", &i, sizeof(int));
            ImGui::TextUnformatted(shapes[i].name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SHAPE_INDEX")) {
                const int from = *static_cast<const int *>(payload->Data);
                document_.MoveShape(from, i);
                transformer_.EndDrag();
            }
            ImGui::EndDragDropTarget();
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

void Application::RenderToolbar() {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    constexpr float toolbarWidth = 680.0f;
    constexpr float toolbarHeight = 52.0f;
    const ImVec2 pos{
        viewport->WorkPos.x + (viewport->WorkSize.x - toolbarWidth) * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y - toolbarHeight - 18.0f
    };

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(toolbarWidth, toolbarHeight));
    ImGui::Begin("Tools", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoCollapse);

    auto toolButton = [&](Tool tool, const char *label) {
        const bool selected = activeTool_ == tool;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.31f, 0.36f, 0.43f, 1.0f));
        }
        if (ImGui::Button(label, ImVec2(68.0f, 32.0f))) {
            if (tool == Tool::Image) {
                AddShapeFromTool(tool);
                activeTool_ = Tool::Select;
            } else {
                activeTool_ = tool;
            }
            transformer_.EndDrag();
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
    };

    toolButton(Tool::Select, "Select");
    toolButton(Tool::Pan, "Pan");
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    toolButton(Tool::Rect, "Rect");
    toolButton(Tool::Circle, "Circle");
    toolButton(Tool::Line, "Line");
    toolButton(Tool::Arrow, "Arrow");
    toolButton(Tool::Text, "Text");
    toolButton(Tool::Image, "Image");

    ImGui::End();
}

Vec2 Application::ViewCenterWorld() const {
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(window_, &windowWidth, &windowHeight);
    return view_.ScreenToWorld({
        static_cast<float>(windowWidth) * 0.5f,
        static_cast<float>(windowHeight) * 0.5f
    });
}

void Application::AddShapeFromTool(Tool tool) {
    Shape shape;
    shape.position = ViewCenterWorld();

    switch (tool) {
        case Tool::Rect:
            shape.type = ShapeType::Rect;
            shape.name = "Rectangle " + std::to_string(document_.Shapes().size() + 1);
            shape.size = {200.0f, 120.0f};
            break;
        case Tool::Circle:
            shape.type = ShapeType::Circle;
            shape.name = "Circle " + std::to_string(document_.Shapes().size() + 1);
            shape.size = {140.0f, 140.0f};
            shape.cornerRadius = 1000.0f;
            break;
        case Tool::Line:
            shape.type = ShapeType::Line;
            shape.name = "Line " + std::to_string(document_.Shapes().size() + 1);
            shape.size = {220.0f, 1.0f};
            shape.borderWidth = 3.0f;
            break;
        case Tool::Arrow:
            shape.type = ShapeType::Arrow;
            shape.name = "Arrow " + std::to_string(document_.Shapes().size() + 1);
            shape.size = {220.0f, 1.0f};
            shape.borderWidth = 3.0f;
            break;
        case Tool::Text:
            shape.type = ShapeType::Text;
            shape.name = "Text " + std::to_string(document_.Shapes().size() + 1);
            shape.size = {180.0f, 48.0f};
            shape.text = "Text";
            shape.fill = {0.92f, 0.94f, 0.96f, 1.0f};
            shape.borderWidth = 0.0f;
            break;
        case Tool::Image: {
            const std::string path = OpenImageFileDialog();
            if (path.empty()) {
                return;
            }
            sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());
            sk_sp<SkImage> image = data ? SkImages::DeferredFromEncodedData(data) : nullptr;
            if (!image) {
                return;
            }
            shape.type = ShapeType::Image;
            shape.name = "Image " + std::to_string(document_.Shapes().size() + 1);
            shape.imagePath = path;
            shape.image = image;
            shape.size = {static_cast<float>(image->width()), static_cast<float>(image->height())};
            shape.fill = {1.0f, 1.0f, 1.0f, 1.0f};
            shape.borderWidth = 0.0f;
            break;
        }
        case Tool::Select:
        case Tool::Pan:
            return;
    }

    document_.AddShape(std::move(shape));
}

void Application::BeginLineDrawing(Tool tool, Vec2 startWorld) {
    Shape shape;
    shape.type = tool == Tool::Arrow ? ShapeType::Arrow : ShapeType::Line;
    shape.name = std::string(tool == Tool::Arrow ? "Arrow " : "Line ") +
        std::to_string(document_.Shapes().size() + 1);
    shape.borderWidth = 3.0f;
    lineStartWorld_ = startWorld;
    drawingLineTool_ = tool;
    SetLineGeometry(shape, startWorld, startWorld);
    document_.AddShape(std::move(shape));
    isDrawingLine_ = true;
    transformer_.EndDrag();
}

void Application::UpdateLineDrawing(Vec2 endWorld) {
    Shape *shape = document_.SelectedShape();
    if (!shape || !IsLineTool(drawingLineTool_)) {
        isDrawingLine_ = false;
        return;
    }

    SetLineGeometry(*shape, lineStartWorld_, endWorld);
}

void Application::FinishLineDrawing() {
    isDrawingLine_ = false;
    activeTool_ = Tool::Select;
    transformer_.EndDrag();
}

void Application::RenderGridAndRulers(SkCanvas *canvas, float logicalWidth, float logicalHeight) {
    float worldStep = 10.0f;
    while (worldStep * view_.zoom < 36.0f) {
        worldStep *= 2.0f;
    }
    while (worldStep * view_.zoom > 96.0f) {
        worldStep *= 0.5f;
    }

    const Vec2 worldTopLeft = view_.ScreenToWorld({0.0f, 0.0f});
    const Vec2 worldBottomRight = view_.ScreenToWorld({logicalWidth, logicalHeight});
    const float firstWorldX = std::floor(worldTopLeft.x / worldStep) * worldStep;
    const float firstWorldY = std::floor(worldTopLeft.y / worldStep) * worldStep;

    SkPaint grid;
    grid.setAntiAlias(false);
    grid.setColor(SkColorSetARGB(14, 255, 255, 255));
    grid.setStrokeWidth(1.0f);

    for (float worldX = firstWorldX; worldX <= worldBottomRight.x + worldStep; worldX += worldStep) {
        const float x = worldX * view_.zoom + view_.pan.x;
        canvas->drawLine(x, 0.0f, x, logicalHeight, grid);
    }
    for (float worldY = firstWorldY; worldY <= worldBottomRight.y + worldStep; worldY += worldStep) {
        const float y = worldY * view_.zoom + view_.pan.y;
        canvas->drawLine(0.0f, y, logicalWidth, y, grid);
    }

    SkPaint rulerBg;
    rulerBg.setColor(SkColorSetARGB(210, 24, 26, 30));
    canvas->drawRect(SkRect::MakeXYWH(0.0f, 0.0f, logicalWidth, 24.0f), rulerBg);
    canvas->drawRect(SkRect::MakeXYWH(0.0f, 0.0f, 36.0f, logicalHeight), rulerBg);

    SkPaint tick;
    tick.setColor(SkColorSetARGB(120, 210, 214, 220));
    tick.setStrokeWidth(1.0f);

    SkPaint labelPaint;
    labelPaint.setAntiAlias(true);
    labelPaint.setColor(SkColorSetARGB(190, 210, 214, 220));
    SkFont font(nullptr, 9.0f);

    char label[32] = {};
    for (float worldX = firstWorldX; worldX <= worldBottomRight.x + worldStep; worldX += worldStep) {
        const float x = worldX * view_.zoom + view_.pan.x;
        canvas->drawLine(x, 16.0f, x, 24.0f, tick);
        std::snprintf(label, sizeof(label), "%d", static_cast<int>(std::round(worldX)));
        canvas->drawString(label, x + 3.0f, 12.0f, font, labelPaint);
    }
    for (float worldY = firstWorldY; worldY <= worldBottomRight.y + worldStep; worldY += worldStep) {
        const float y = worldY * view_.zoom + view_.pan.y;
        canvas->drawLine(28.0f, y, 36.0f, y, tick);
        std::snprintf(label, sizeof(label), "%d", static_cast<int>(std::round(worldY)));
        canvas->drawString(label, 4.0f, std::max(11.0f, y - 3.0f), font, labelPaint);
    }
}

void Application::RenderShape(SkCanvas *canvas, const Shape &shape) {
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
    fill.setColor(ToSkColor(shape.fill));

    SkPaint border;
    border.setAntiAlias(true);
    border.setStyle(SkPaint::kStroke_Style);
    border.setStrokeWidth(shape.borderWidth);
    border.setColor(ToSkColor(shape.border));

    if (shape.type == ShapeType::Circle) {
        canvas->drawOval(rect, fill);
        if (shape.borderWidth > 0.0f) {
            canvas->drawOval(rect, border);
        }
    } else if (shape.type == ShapeType::Line || shape.type == ShapeType::Arrow) {
        const float halfWidth = shape.size.x * 0.5f;
        canvas->drawLine(-halfWidth, 0.0f, halfWidth, 0.0f, border);
        if (shape.type == ShapeType::Arrow) {
            SkPath arrow;
            arrow.moveTo(halfWidth, 0.0f);
            arrow.lineTo(halfWidth - 14.0f, -7.0f);
            arrow.lineTo(halfWidth - 14.0f, 7.0f);
            arrow.close();
            canvas->drawPath(arrow, border);
        }
    } else if (shape.type == ShapeType::Text) {
        SkFont font(nullptr, std::max(12.0f, shape.size.y * 0.55f));
        canvas->drawString(shape.text.c_str(), rect.left(), rect.centerY() + font.getSize() * 0.35f, font, fill);
    } else {
        if (shape.type == ShapeType::Image && shape.image) {
            canvas->save();
            if (radius > 0.0f) {
                canvas->clipRRect(SkRRect::MakeRectXY(rect, radius, radius), true);
            } else {
                canvas->clipRect(rect, true);
            }
            canvas->drawImageRect(shape.image, rect, SkSamplingOptions(), &fill);
            canvas->restore();
        } else if (radius > 0.0f) {
            canvas->drawRRect(SkRRect::MakeRectXY(rect, radius, radius), fill);
        } else {
            canvas->drawRect(rect, fill);
        }

        if (shape.borderWidth > 0.0f) {
            if (radius > 0.0f) {
                canvas->drawRRect(SkRRect::MakeRectXY(rect, radius, radius), border);
            } else {
                canvas->drawRect(rect, border);
            }
        }
    }

    canvas->restore();
}

bool Application::IsSnapDisabled() const {
    const ImGuiIO &io = ImGui::GetIO();
    return io.KeyAlt || io.KeyCtrl || io.KeySuper;
}

void Application::ApplySnapping(Shape &shape) {
    snapGuides_.clear();
    if (IsSnapDisabled()) {
        return;
    }

    constexpr float threshold = 6.0f;
    float bestDx = 0.0f;
    float bestDy = 0.0f;
    float bestX = threshold / view_.zoom;
    float bestY = threshold / view_.zoom;

    const auto corners = GetShapeCorners(shape);
    const float left = std::min({corners[0].x, corners[1].x, corners[2].x, corners[3].x});
    const float right = std::max({corners[0].x, corners[1].x, corners[2].x, corners[3].x});
    const float top = std::min({corners[0].y, corners[1].y, corners[2].y, corners[3].y});
    const float bottom = std::max({corners[0].y, corners[1].y, corners[2].y, corners[3].y});
    const float centerX = (left + right) * 0.5f;
    const float centerY = (top + bottom) * 0.5f;

    auto testX = [&](float value, float target) {
        const float delta = target - value;
        if (std::abs(delta) < bestX) {
            bestX = std::abs(delta);
            bestDx = delta;
            if (snapGuides_.empty() || snapGuides_.back().axis != SnapAxis::Vertical) {
                snapGuides_.push_back({SnapAxis::Vertical, target});
            } else {
                snapGuides_.back().position = target;
            }
        }
    };
    auto testY = [&](float value, float target) {
        const float delta = target - value;
        if (std::abs(delta) < bestY) {
            bestY = std::abs(delta);
            bestDy = delta;
            snapGuides_.push_back({SnapAxis::Horizontal, target});
        }
    };

    float gridStep = 10.0f;
    while (gridStep * view_.zoom < 36.0f) {
        gridStep *= 2.0f;
    }
    auto nearestGrid = [&](float value) {
        return std::round(value / gridStep) * gridStep;
    };

    for (float value: {left, centerX, right}) {
        testX(value, nearestGrid(value));
    }
    for (float value: {top, centerY, bottom}) {
        testY(value, nearestGrid(value));
    }

    for (const Shape &other: document_.Shapes()) {
        if (&other == &shape) {
            continue;
        }
        const auto otherCorners = GetShapeCorners(other);
        const float otherLeft = std::min({otherCorners[0].x, otherCorners[1].x, otherCorners[2].x, otherCorners[3].x});
        const float otherRight = std::max({otherCorners[0].x, otherCorners[1].x, otherCorners[2].x, otherCorners[3].x});
        const float otherTop = std::min({otherCorners[0].y, otherCorners[1].y, otherCorners[2].y, otherCorners[3].y});
        const float otherBottom = std::max({otherCorners[0].y, otherCorners[1].y, otherCorners[2].y, otherCorners[3].y});
        const float otherCenterX = (otherLeft + otherRight) * 0.5f;
        const float otherCenterY = (otherTop + otherBottom) * 0.5f;

        for (float value: {left, centerX, right}) {
            for (float target: {otherLeft, otherCenterX, otherRight}) {
                testX(value, target);
            }
        }
        for (float value: {top, centerY, bottom}) {
            for (float target: {otherTop, otherCenterY, otherBottom}) {
                testY(value, target);
            }
        }
    }

    shape.position.x += bestDx;
    shape.position.y += bestDy;
}

void Application::Render(float dpr, int framebufferWidth, int framebufferHeight) {
    SkCanvas *canvas = skia_.BeginFrame(framebufferWidth, framebufferHeight);
    if (!canvas) {
        return;
    }

    canvas->clear(SK_ColorBLACK);
    const float logicalWidth = static_cast<float>(framebufferWidth) / dpr;
    const float logicalHeight = static_cast<float>(framebufferHeight) / dpr;

    canvas->save();
    canvas->scale(dpr, dpr);
    RenderGridAndRulers(canvas, logicalWidth, logicalHeight);
    canvas->restore();

    canvas->save();
    canvas->scale(dpr, dpr);
    canvas->translate(view_.pan.x, view_.pan.y);
    canvas->scale(view_.zoom, view_.zoom);

    const auto &shapes = document_.Shapes();
    for (int i = static_cast<int>(shapes.size()) - 1; i >= 0; --i) {
        RenderShape(canvas, shapes[i]);
    }
    canvas->restore();

    if (const Shape *selectedShape = document_.SelectedShape()) {
        canvas->save();
        canvas->scale(dpr, dpr);
        transformer_.Draw(canvas, *selectedShape, view_);
        canvas->restore();
    }

    if (!snapGuides_.empty()) {
        canvas->save();
        canvas->scale(dpr, dpr);
        SkPaint guide;
        guide.setAntiAlias(true);
        guide.setColor(SkColorSetARGB(105, 255, 105, 125));
        guide.setStrokeWidth(1.0f);
        for (const SnapGuide &snap: snapGuides_) {
            if (snap.axis == SnapAxis::Vertical) {
                const float x = view_.WorldToScreen({snap.position, 0.0f}).x;
                for (float y = 0.0f; y < logicalHeight; y += 12.0f) {
                    canvas->drawLine(x, y, x, y + 6.0f, guide);
                }
            } else {
                const float y = view_.WorldToScreen({0.0f, snap.position}).y;
                for (float x = 0.0f; x < logicalWidth; x += 12.0f) {
                    canvas->drawLine(x, y, x + 6.0f, y, guide);
                }
            }
        }
        canvas->restore();
    }

    skia_.EndFrame();

    RenderPanels();
    RenderToolbar();
}

void Application::Shutdown() {
    imgui_.Shutdown();
    skia_.Shutdown();

    if (handCursor_) {
        glfwDestroyCursor(handCursor_);
        handCursor_ = nullptr;
    }
    if (defaultCursor_) {
        glfwDestroyCursor(defaultCursor_);
        defaultCursor_ = nullptr;
    }

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}
