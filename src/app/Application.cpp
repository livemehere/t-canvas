#define GL_SILENCE_DEPRECATION

#include "Application.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include <imgui.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColor.h>
#include <skia/core/SkData.h>
#include <skia/core/SkFont.h>
#include <skia/core/SkFontMgr.h>
#include <skia/core/SkFontStyle.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkRRect.h>
#include <skia/core/SkRect.h>
#include <skia/core/SkSamplingOptions.h>
#include <skia/core/SkTypeface.h>
#include <skia/ports/SkFontMgr_mac_ct.h>

#include "../platform/FileDialog.h"

namespace {
constexpr float kLeftPanelWidth = 240.0f;
constexpr float kRightPanelWidth = 300.0f;

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

sk_sp<SkTypeface> CanvasTypeface() {
    static sk_sp<SkTypeface> typeface = [] {
        sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_CoreText(nullptr);
        if (fontMgr) {
            for (const char *family: {"SF Pro Text", "Helvetica Neue", "Helvetica", "Apple SD Gothic Neo"}) {
                sk_sp<SkTypeface> matched = fontMgr->matchFamilyStyle(family, SkFontStyle::Normal());
                if (matched) {
                    return matched;
                }
            }
            sk_sp<SkTypeface> fallback = fontMgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
            if (fallback) {
                return fallback;
            }
        }

        const char *fontPaths[] = {
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/AppleSDGothicNeo.ttc"
        };
        if (!fontMgr) {
            fontMgr = SkFontMgr::RefEmpty();
        }
        for (const char *path: fontPaths) {
            sk_sp<SkTypeface> loaded = fontMgr->makeFromFile(path);
            if (loaded) {
                return loaded;
            }
        }
        return sk_sp<SkTypeface>();
    }();
    return typeface;
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

int TrackTextEditCursor(ImGuiInputTextCallbackData *data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && data->UserData) {
        *static_cast<int *>(data->UserData) = data->CursorPos;
    }
    return 0;
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

    HandleShortcuts();

    if (io.WantCaptureMouse) {
        return;
    }

    const Vec2 mouse{io.MousePos.x, io.MousePos.y};
    const Vec2 mouseWorld = view_.ScreenToWorld(mouse);

    if (editingTextIndex_ < 0 && !io.WantCaptureKeyboard &&
        (ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Delete))) {
        document_.RemoveSelectedShape();
        transformer_.EndDrag();
        snapGuides_.clear();
        isDrawingLine_ = false;
    }

    if (io.MouseWheel != 0.0f && mouse.x >= 0.0f && mouse.y >= 0.0f) {
        view_.ZoomAt(mouse, io.MouseWheel, io.KeyAlt ? 0.25f : 1.0f);
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
            } else if (activeTool_ == Tool::Text) {
                AddShapeFromTool(activeTool_);
                if (Shape *shape = document_.SelectedShape()) {
                    shape->position = mouseWorld;
                    shape->text.clear();
                }
                BeginTextEditing(document_.SelectedShapeIndex());
                activeTool_ = Tool::Select;
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
        transformer_.UpdateDrag(mouseWorld, *selectedShape, io.KeyShift);
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

void Application::HandleShortcuts() {
    ImGuiIO &io = ImGui::GetIO();
    if (editingTextIndex_ >= 0) {
        return;
    }

    const bool macCommandDown =
        glfwGetKey(window_, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
        glfwGetKey(window_, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS;

    if (macCommandDown && ImGui::IsKeyPressed(ImGuiKey_C)) {
        CopySelection();
        return;
    }
    if (macCommandDown && ImGui::IsKeyPressed(ImGuiKey_V)) {
        PasteSelectionOrClipboardImage();
        return;
    }

    if (io.WantCaptureKeyboard) {
        return;
    }

    if (io.KeySuper || io.KeyAlt || io.KeyCtrl || io.KeyShift) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_S)) {
        activeTool_ = Tool::Select;
    } else if (ImGui::IsKeyPressed(ImGuiKey_P)) {
        activeTool_ = Tool::Pan;
    } else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
        activeTool_ = Tool::Rect;
    } else if (ImGui::IsKeyPressed(ImGuiKey_C)) {
        activeTool_ = Tool::Circle;
    } else if (ImGui::IsKeyPressed(ImGuiKey_L)) {
        activeTool_ = Tool::Line;
    } else if (ImGui::IsKeyPressed(ImGuiKey_A)) {
        activeTool_ = Tool::Arrow;
    } else if (ImGui::IsKeyPressed(ImGuiKey_T)) {
        activeTool_ = Tool::Text;
    } else if (ImGui::IsKeyPressed(ImGuiKey_I)) {
        AddShapeFromTool(Tool::Image);
        activeTool_ = Tool::Select;
    }

    if (activeTool_ != Tool::Select) {
        transformer_.EndDrag();
    }
}

void Application::RenderPanels() {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    constexpr ImGuiWindowFlags panelFlags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(kLeftPanelWidth, viewport->WorkSize.y));
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

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - kRightPanelWidth, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(kRightPanelWidth, viewport->WorkSize.y));
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

void Application::RenderTextEditor() {
    if (editingTextIndex_ < 0 || editingTextIndex_ != document_.SelectedShapeIndex()) {
        return;
    }

    Shape *shape = document_.SelectedShape();
    if (!shape || shape->type != ShapeType::Text) {
        editingTextIndex_ = -1;
        return;
    }

    const Vec2 screen = view_.WorldToScreen(shape->position);
    const ImVec2 size{
        std::max(180.0f, shape->size.x * view_.zoom),
        std::max(48.0f, shape->size.y * view_.zoom)
    };
    const ImVec2 pos{
        screen.x - size.x * 0.5f,
        screen.y - size.y * 0.5f
    };

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("TextEditorOverlay", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBackground);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.09f, 0.10f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.10f, 0.11f, 0.13f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.11f, 0.13f, 0.98f));

    if (focusTextEditor_) {
        ImGui::SetKeyboardFocusHere();
        focusTextEditor_ = false;
    }

    const ImGuiInputTextFlags flags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CtrlEnterForNewLine |
        ImGuiInputTextFlags_CallbackAlways;
    const bool submitted = ImGui::InputTextMultiline(
        "##TextEdit",
        textEditBuffer_.data(),
        textEditBuffer_.size(),
        ImGui::GetContentRegionAvail(),
        flags,
        TrackTextEditCursor,
        &textEditCursor_
    );

    ImGuiIO &io = ImGui::GetIO();
    if (submitted && (io.KeyShift || io.KeySuper)) {
        InsertTextEditorNewline();
        focusTextEditor_ = true;
    }

    shape->text = textEditBuffer_.data();

    if (submitted && !io.KeyShift && !io.KeySuper) {
        FinishTextEditing();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        FinishTextEditing();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
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
            shape.text.clear();
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

void Application::AddImageFromClipboard() {
    sk_sp<SkData> data = ReadClipboardImageData();
    sk_sp<SkImage> image = data ? SkImages::DeferredFromEncodedData(data) : nullptr;
    if (!image) {
        return;
    }

    Shape shape;
    shape.type = ShapeType::Image;
    shape.name = "Image " + std::to_string(document_.Shapes().size() + 1);
    shape.image = image;
    shape.position = ViewCenterWorld();
    shape.size = {static_cast<float>(image->width()), static_cast<float>(image->height())};
    shape.fill = {1.0f, 1.0f, 1.0f, 1.0f};
    shape.borderWidth = 0.0f;
    document_.AddShape(std::move(shape));
}

void Application::CopySelection() {
    const Shape *shape = document_.SelectedShape();
    if (!shape) {
        return;
    }

    copiedShape_ = *shape;
    hasCopiedShape_ = true;
}

void Application::PasteSelectionOrClipboardImage() {
    if (hasCopiedShape_) {
        Shape pasted = copiedShape_;
        pasted.name += " Copy";
        pasted.position = pasted.position + Vec2{24.0f, 24.0f};
        document_.AddShape(std::move(pasted));
        copiedShape_ = *document_.SelectedShape();
        transformer_.EndDrag();
        return;
    }

    sk_sp<SkData> data = ReadClipboardImageData();
    sk_sp<SkImage> image = data ? SkImages::DeferredFromEncodedData(data) : nullptr;
    if (image) {
        Shape shape;
        shape.type = ShapeType::Image;
        shape.name = "Image " + std::to_string(document_.Shapes().size() + 1);
        shape.image = image;
        shape.position = ViewCenterWorld();
        shape.size = {static_cast<float>(image->width()), static_cast<float>(image->height())};
        shape.fill = {1.0f, 1.0f, 1.0f, 1.0f};
        shape.borderWidth = 0.0f;
        document_.AddShape(std::move(shape));
        transformer_.EndDrag();
        return;
    }
}

void Application::BeginTextEditing(int shapeIndex) {
    Shape *shape = document_.SelectedShape();
    if (shapeIndex < 0 || !shape || shape->type != ShapeType::Text) {
        editingTextIndex_ = -1;
        return;
    }

    editingTextIndex_ = shapeIndex;
    textEditBuffer_.fill('\0');
    std::strncpy(textEditBuffer_.data(), shape->text.c_str(), textEditBuffer_.size() - 1);
    textEditCursor_ = static_cast<int>(std::strlen(textEditBuffer_.data()));
    focusTextEditor_ = true;
    transformer_.EndDrag();
}

void Application::FinishTextEditing() {
    Shape *shape = document_.SelectedShape();
    if (shape && editingTextIndex_ == document_.SelectedShapeIndex() && shape->type == ShapeType::Text) {
        shape->text = textEditBuffer_.data();
    }
    editingTextIndex_ = -1;
    textEditCursor_ = 0;
    focusTextEditor_ = false;
}

void Application::InsertTextEditorNewline() {
    const int length = static_cast<int>(std::strlen(textEditBuffer_.data()));
    if (length + 1 >= static_cast<int>(textEditBuffer_.size())) {
        return;
    }

    const int cursor = Clamp(textEditCursor_, 0, length);
    std::memmove(
        textEditBuffer_.data() + cursor + 1,
        textEditBuffer_.data() + cursor,
        static_cast<size_t>(length - cursor + 1)
    );
    textEditBuffer_[cursor] = '\n';
    textEditCursor_ = cursor + 1;
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

void Application::RenderGridAndRulers(
    SkCanvas *canvas,
    float logicalWidth,
    float logicalHeight,
    bool drawGrid,
    bool drawRulers
) {
    const float canvasLeft = kLeftPanelWidth;
    const float canvasRight = std::max(canvasLeft, logicalWidth - kRightPanelWidth);
    constexpr float rulerWidth = 36.0f;
    constexpr float rulerHeight = 24.0f;

    float worldStep = 10.0f;
    while (worldStep * view_.zoom < 36.0f) {
        worldStep *= 2.0f;
    }
    while (worldStep * view_.zoom > 96.0f) {
        worldStep *= 0.5f;
    }

    const Vec2 worldTopLeft = view_.ScreenToWorld({canvasLeft, 0.0f});
    const Vec2 worldBottomRight = view_.ScreenToWorld({canvasRight, logicalHeight});
    const float firstWorldX = std::floor(worldTopLeft.x / worldStep) * worldStep;
    const float firstWorldY = std::floor(worldTopLeft.y / worldStep) * worldStep;

    if (drawGrid) {
        SkPaint grid;
        grid.setAntiAlias(false);
        grid.setColor(SkColorSetARGB(14, 255, 255, 255));
        grid.setStrokeWidth(1.0f);

        for (float worldX = firstWorldX; worldX <= worldBottomRight.x + worldStep; worldX += worldStep) {
            const float x = worldX * view_.zoom + view_.pan.x;
            if (x >= canvasLeft && x <= canvasRight) {
                canvas->drawLine(x, 0.0f, x, logicalHeight, grid);
            }
        }
        for (float worldY = firstWorldY; worldY <= worldBottomRight.y + worldStep; worldY += worldStep) {
            const float y = worldY * view_.zoom + view_.pan.y;
            canvas->drawLine(canvasLeft, y, canvasRight, y, grid);
        }
    }

    if (!drawRulers) {
        return;
    }

    SkPaint rulerBg;
    rulerBg.setColor(SkColorSetARGB(210, 24, 26, 30));
    canvas->drawRect(SkRect::MakeLTRB(canvasLeft, 0.0f, canvasRight, rulerHeight), rulerBg);
    canvas->drawRect(SkRect::MakeXYWH(canvasLeft, 0.0f, rulerWidth, logicalHeight), rulerBg);

    SkPaint tick;
    tick.setColor(SkColorSetARGB(120, 210, 214, 220));
    tick.setStrokeWidth(1.0f);

    SkPaint labelPaint;
    labelPaint.setAntiAlias(true);
    labelPaint.setColor(SkColorSetARGB(190, 210, 214, 220));
    SkFont font(CanvasTypeface(), 9.0f);

    char label[32] = {};
    for (float worldX = firstWorldX; worldX <= worldBottomRight.x + worldStep; worldX += worldStep) {
        const float x = worldX * view_.zoom + view_.pan.x;
        if (x >= canvasLeft + rulerWidth && x <= canvasRight) {
            canvas->drawLine(x, 16.0f, x, rulerHeight, tick);
            std::snprintf(label, sizeof(label), "%d", static_cast<int>(std::round(worldX)));
            canvas->drawString(label, x + 3.0f, 12.0f, font, labelPaint);
        }
    }
    for (float worldY = firstWorldY; worldY <= worldBottomRight.y + worldStep; worldY += worldStep) {
        const float y = worldY * view_.zoom + view_.pan.y;
        canvas->drawLine(canvasLeft + 28.0f, y, canvasLeft + rulerWidth, y, tick);
        std::snprintf(label, sizeof(label), "%d", static_cast<int>(std::round(worldY)));
        canvas->drawString(label, canvasLeft + 4.0f, std::max(11.0f, y - 3.0f), font, labelPaint);
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
        constexpr float arrowLength = 18.0f;
        constexpr float arrowHalfHeight = 4.8f;
        const float lineEnd = shape.type == ShapeType::Arrow ? halfWidth - arrowLength + 1.0f : halfWidth;
        canvas->drawLine(-halfWidth, 0.0f, lineEnd, 0.0f, border);
        if (shape.type == ShapeType::Arrow) {
            SkPaint arrowFill = border;
            arrowFill.setStyle(SkPaint::kFill_Style);
            SkPath arrow;
            arrow.moveTo(halfWidth, 0.0f);
            arrow.lineTo(halfWidth - arrowLength, -arrowHalfHeight);
            arrow.lineTo(halfWidth - arrowLength, arrowHalfHeight);
            arrow.close();
            canvas->drawPath(arrow, arrowFill);
        }
    } else if (shape.type == ShapeType::Text) {
        SkFont font(CanvasTypeface(), std::max(12.0f, shape.size.y * 0.55f));
        const float lineHeight = font.getSize() * 1.25f;
        float baseline = rect.top() + font.getSize();
        size_t lineStart = 0;
        while (lineStart <= shape.text.size()) {
            const size_t lineEnd = shape.text.find('\n', lineStart);
            const std::string line = shape.text.substr(
                lineStart,
                lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart
            );
            if (!line.empty()) {
                canvas->drawString(line.c_str(), rect.left(), baseline, font, fill);
            }
            if (lineEnd == std::string::npos) {
                break;
            }
            lineStart = lineEnd + 1;
            baseline += lineHeight;
        }
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
    RenderGridAndRulers(canvas, logicalWidth, logicalHeight, true, false);
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

    canvas->save();
    canvas->scale(dpr, dpr);
    RenderGridAndRulers(canvas, logicalWidth, logicalHeight, false, true);
    canvas->restore();

    skia_.EndFrame();

    RenderPanels();
    RenderToolbar();
    RenderTextEditor();
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
