#define GL_SILENCE_DEPRECATION

#include "Application.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <sstream>
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
#include <skia/core/SkImageInfo.h>
#include <skia/core/SkM44.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkPixmap.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkPathUtils.h>
#include <skia/core/SkRRect.h>
#include <skia/core/SkRect.h>
#include <skia/core/SkSamplingOptions.h>
#include <skia/core/SkSurface.h>
#include <skia/core/SkTileMode.h>
#include <skia/core/SkTypeface.h>
#include <skia/effects/SkImageFilters.h>
#include <skia/encode/SkPngEncoder.h>
#include <skia/src/base/SkUTF.h>
#include <skia/ports/SkFontMgr_mac_ct.h>

#include "../platform/FileDialog.h"

namespace {
constexpr float kLeftPanelWidth = 240.0f;
constexpr float kRightPanelWidth = 300.0f;
constexpr Color kAccentRed{1.0f, 0.22f, 0.20f, 1.0f};

GLFWcursor *CreateAsciiCursor(const std::array<const char *, 24> &rows, int hotX, int hotY) {
    constexpr int width = 24;
    constexpr int height = 24;
    std::array<unsigned char, width * height * 4> pixels{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const char pixel = rows[y][x];
            const int offset = (y * width + x) * 4;
            if (pixel == 'X') {
                pixels[offset + 0] = 28;
                pixels[offset + 1] = 30;
                pixels[offset + 2] = 34;
                pixels[offset + 3] = 255;
            } else if (pixel == '.') {
                pixels[offset + 0] = 248;
                pixels[offset + 1] = 250;
                pixels[offset + 2] = 252;
                pixels[offset + 3] = 255;
            }
        }
    }
    GLFWimage image{width, height, pixels.data()};
    return glfwCreateCursor(&image, hotX, hotY);
}

GLFWcursor *CreateGrabCursor() {
    return CreateAsciiCursor({
        "                        ",
        "                        ",
        "        .XX.            ",
        "       .X..X.           ",
        "       .X..X.           ",
        "    .XX.X..X.XX.        ",
        "   .X..XX..XX..X.       ",
        "   .X............X.     ",
        "   .X............X.     ",
        "    .X...........X.     ",
        "     .X..........X.     ",
        "      .X........X.      ",
        "       .X......X.       ",
        "        .XXXXXX.        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        "
    }, 11, 8);
}

GLFWcursor *CreateGrabbingCursor() {
    return CreateAsciiCursor({
        "                        ",
        "                        ",
        "                        ",
        "     .XXXXXXXXXXXX.     ",
        "    .X............X.    ",
        "   .X..............X.   ",
        "  .X................X.  ",
        "  .X................X.  ",
        "  .X................X.  ",
        "   .X..............X.   ",
        "    .X............X.    ",
        "     .XXXXXXXXXXXX.     ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        ",
        "                        "
    }, 11, 7);
}

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
            for (const char *family: {"Apple SD Gothic Neo", "SF Pro Text", "Helvetica Neue", "Helvetica"}) {
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

sk_sp<SkFontMgr> CanvasFontMgr() {
    static sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_CoreText(nullptr);
    return fontMgr;
}

sk_sp<SkTypeface> TypefaceForCharacter(SkUnichar character, sk_sp<SkTypeface> preferred) {
    SkFont preferredFont(preferred);
    if (preferredFont.unicharToGlyph(character) != 0) {
        return preferred;
    }

    sk_sp<SkFontMgr> fontMgr = CanvasFontMgr();
    if (!fontMgr) {
        return preferred;
    }

    const char *languages[] = {"ko", "en"};
    sk_sp<SkTypeface> matched = fontMgr->matchFamilyStyleCharacter(
        nullptr,
        SkFontStyle::Normal(),
        languages,
        2,
        character
    );
    return matched ? matched : preferred;
}

float DrawTextWithFallback(SkCanvas *canvas, const std::string &line, float x, float baseline, float size, const SkPaint &paint) {
    if (line.empty()) {
        return 0.0f;
    }

    const char *start = line.data();
    const char *cursor = start;
    const char *end = start + line.size();
    sk_sp<SkTypeface> currentTypeface;
    std::string run;
    float advance = 0.0f;
    sk_sp<SkTypeface> preferred = CanvasTypeface();

    auto flush = [&] {
        if (run.empty()) {
            return;
        }
        SkFont font(currentTypeface ? currentTypeface : preferred, size);
        canvas->drawSimpleText(
            run.data(),
            run.size(),
            SkTextEncoding::kUTF8,
            x + advance,
            baseline,
            font,
            paint
        );
        advance += font.measureText(run.data(), run.size(), SkTextEncoding::kUTF8);
        run.clear();
    };

    while (cursor < end) {
        const char *before = cursor;
        SkUnichar character = SkUTF::NextUTF8WithReplacement(&cursor, end);
        sk_sp<SkTypeface> typeface = TypefaceForCharacter(character, preferred);
        if (!currentTypeface || currentTypeface.get() != typeface.get()) {
            flush();
            currentTypeface = typeface;
        }
        run.append(before, static_cast<size_t>(cursor - before));
    }
    flush();
    return advance;
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
        case Application::Tool::Brush:
            return "Brush";
    }
    return "";
}

const char *ToolShortcut(Application::Tool tool) {
    switch (tool) {
        case Application::Tool::Select:
            return "S";
        case Application::Tool::Pan:
            return "P / Space";
        case Application::Tool::Rect:
            return "R";
        case Application::Tool::Circle:
            return "C";
        case Application::Tool::Line:
            return "L";
        case Application::Tool::Arrow:
            return "A";
        case Application::Tool::Text:
            return "T";
        case Application::Tool::Image:
            return "I";
        case Application::Tool::Brush:
            return "B";
    }
    return "";
}

bool IsLineTool(Application::Tool tool) {
    return tool == Application::Tool::Line || tool == Application::Tool::Arrow;
}

bool IsBoxTool(Application::Tool tool) {
    return tool == Application::Tool::Rect || tool == Application::Tool::Circle;
}

int ShapePreferenceIndex(ShapeType type) {
    switch (type) {
        case ShapeType::Rect:
            return 0;
        case ShapeType::Circle:
            return 1;
        case ShapeType::Line:
            return 2;
        case ShapeType::Arrow:
            return 3;
        case ShapeType::Text:
            return 4;
        case ShapeType::Image:
            return 5;
        case ShapeType::Brush:
            return 6;
    }
    return 0;
}

const char *ShapePreferenceKey(ShapeType type) {
    switch (type) {
        case ShapeType::Rect:
            return "rect";
        case ShapeType::Circle:
            return "circle";
        case ShapeType::Line:
            return "line";
        case ShapeType::Arrow:
            return "arrow";
        case ShapeType::Text:
            return "text";
        case ShapeType::Image:
            return "image";
        case ShapeType::Brush:
            return "brush";
    }
    return "rect";
}

const char *ShapePreferenceLabel(ShapeType type) {
    switch (type) {
        case ShapeType::Rect:
            return "Rect";
        case ShapeType::Circle:
            return "Circle";
        case ShapeType::Line:
            return "Line";
        case ShapeType::Arrow:
            return "Arrow";
        case ShapeType::Text:
            return "Text";
        case ShapeType::Image:
            return "Image";
        case ShapeType::Brush:
            return "Brush";
    }
    return "Rect";
}

void DrawPreferencePreview(ShapeType type, const Application::ShapePreferences &prefs, ImVec2 size) {
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 end{pos.x + size.x, pos.y + size.y};
    const ImU32 panel = ImGui::GetColorU32(ImVec4(0.075f, 0.080f, 0.090f, 1.0f));
    const ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(prefs.fill.r, prefs.fill.g, prefs.fill.b, prefs.fill.a));
    const ImU32 stroke = ImGui::ColorConvertFloat4ToU32(ImVec4(prefs.border.r, prefs.border.g, prefs.border.b, prefs.border.a));
    const float strokeWidth = std::max(1.0f, prefs.borderWidth);
    const ImVec2 center{pos.x + size.x * 0.5f, pos.y + size.y * 0.5f};

    drawList->AddRectFilled(pos, end, panel, 6.0f);
    drawList->AddRect(pos, end, ImGui::GetColorU32(ImVec4(0.18f, 0.20f, 0.23f, 1.0f)), 6.0f);

    if (type == ShapeType::Circle) {
        const float radius = std::min(size.x, size.y) * 0.28f;
        if (prefs.fillEnabled) {
            drawList->AddCircleFilled(center, radius, fill, 48);
        }
        if (prefs.borderEnabled) {
            drawList->AddCircle(center, radius, stroke, 48, strokeWidth);
        }
    } else if (type == ShapeType::Line || type == ShapeType::Arrow) {
        const ImVec2 a{pos.x + size.x * 0.22f, center.y};
        const ImVec2 b{pos.x + size.x * 0.78f, center.y};
        if (prefs.borderEnabled) {
            drawList->AddLine(a, b, stroke, strokeWidth);
        }
        if (type == ShapeType::Arrow && prefs.borderEnabled) {
            const float arrowLength = Clamp(prefs.arrowHeadSize, 6.0f, 120.0f) * 0.85f;
            const float arrowHalfHeight = Clamp(prefs.arrowHeadSize, 6.0f, 120.0f) * 0.28f;
            drawList->AddTriangleFilled(
                b,
                ImVec2{b.x - arrowLength, b.y - arrowHalfHeight},
                ImVec2{b.x - arrowLength, b.y + arrowHalfHeight},
                stroke
            );
        }
    } else if (type == ShapeType::Text) {
        if (prefs.fillEnabled) {
            drawList->AddText(ImVec2{pos.x + 18.0f, center.y - 9.0f}, fill, "Text");
        }
    } else if (type == ShapeType::Brush) {
        const float radius = std::min(size.x, size.y) * 0.22f * (prefs.brushSize / 52.0f);
        if (prefs.fillEnabled) {
            drawList->AddCircleFilled(center, Clamp(radius, 8.0f, 34.0f), fill, 48);
        }
        if (prefs.borderEnabled) {
            drawList->AddCircle(center, Clamp(radius, 8.0f, 34.0f), stroke, 48, std::max(1.0f, strokeWidth));
        }
    } else {
        const ImVec2 rectMin{pos.x + size.x * 0.22f, pos.y + size.y * 0.25f};
        const ImVec2 rectMax{pos.x + size.x * 0.78f, pos.y + size.y * 0.75f};
        const float radius = type == ShapeType::Rect || type == ShapeType::Image ? std::min(prefs.cornerRadius * 0.25f, 18.0f) : 0.0f;
        if (prefs.fillEnabled || type == ShapeType::Image) {
            drawList->AddRectFilled(rectMin, rectMax, fill, radius);
        }
        if (prefs.borderEnabled) {
            drawList->AddRect(rectMin, rectMax, stroke, radius, 0, strokeWidth);
        }
        if (type == ShapeType::Image) {
            drawList->AddLine(
                ImVec2{rectMin.x + 12.0f, rectMax.y - 14.0f},
                ImVec2{rectMin.x + 32.0f, rectMax.y - 34.0f},
                stroke,
                1.0f
            );
            drawList->AddLine(
                ImVec2{rectMin.x + 32.0f, rectMax.y - 34.0f},
                ImVec2{rectMax.x - 12.0f, rectMax.y - 14.0f},
                stroke,
                1.0f
            );
        }
    }

    ImGui::Dummy(size);
}

bool ParseColor(const std::string &value, Color &color) {
    std::stringstream stream(value);
    std::string part;
    float values[4] = {};
    for (int i = 0; i < 4; ++i) {
        if (!std::getline(stream, part, ',')) {
            return false;
        }
        values[i] = std::stof(part);
    }
    color = {values[0], values[1], values[2], values[3]};
    return true;
}

std::string FormatColor(Color color) {
    char buffer[128] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%.4f,%.4f,%.4f,%.4f",
        color.r,
        color.g,
        color.b,
        color.a
    );
    return buffer;
}

void SetLineGeometry(Shape &shape, Vec2 start, Vec2 end) {
    const Vec2 delta = end - start;
    const float length = std::max(2.0f, std::sqrt(delta.x * delta.x + delta.y * delta.y));
    shape.position = Midpoint(start, end);
    shape.size = {length, 1.0f};
    shape.rotation = RadiansToDegrees(std::atan2(delta.y, delta.x));
}

Vec2 ConstrainLineEnd(Vec2 start, Vec2 end) {
    const Vec2 delta = end - start;
    if (std::abs(delta.x) >= std::abs(delta.y)) {
        return {end.x, start.y};
    }
    return {start.x, end.y};
}

int TrackTextEditCursor(ImGuiInputTextCallbackData *data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && data->UserData) {
        *static_cast<int *>(data->UserData) = data->CursorPos;
    }
    return 0;
}

SkPoint DevicePoint(Vec2 world, const CanvasView &view, float dpr) {
    const Vec2 screen = view.WorldToScreen(world);
    return SkPoint::Make(screen.x * dpr, screen.y * dpr);
}

SkPoint ExportPoint(Vec2 world, float left, float top, float scaleX, float scaleY) {
    return SkPoint::Make((world.x - left) * scaleX, (world.y - top) * scaleY);
}

void AddRectMask(const Shape &shape, SkPath &path, const std::function<SkPoint(Vec2)> &mapPoint) {
    const auto corners = GetShapeCorners(shape);
    path.moveTo(mapPoint(corners[0]));
    for (int i = 1; i < 4; ++i) {
        path.lineTo(mapPoint(corners[i]));
    }
    path.close();
}

void AddBrushMask(
    const Shape &shape,
    SkPath &path,
    const std::function<SkPoint(Vec2)> &mapPoint,
    float scale
) {
    if (shape.brushPoints.empty()) {
        return;
    }

    SkPath stroke;
    SkPoint p = mapPoint(LocalToWorld(shape.brushPoints.front(), shape));
    stroke.moveTo(p);
    if (shape.brushPoints.size() == 1) {
        path.addCircle(p.x(), p.y(), shape.brushSize * 0.5f * scale);
        return;
    }

    for (size_t i = 1; i < shape.brushPoints.size(); ++i) {
        stroke.lineTo(mapPoint(LocalToWorld(shape.brushPoints[i], shape)));
    }

    SkPaint strokePaint;
    strokePaint.setAntiAlias(true);
    strokePaint.setStyle(SkPaint::kStroke_Style);
    strokePaint.setStrokeCap(SkPaint::kRound_Cap);
    strokePaint.setStrokeJoin(SkPaint::kRound_Join);
    strokePaint.setStrokeWidth(shape.brushSize * scale);
    skpathutils::FillPathWithPaint(stroke, strokePaint, &path);
}

void ApplyBlurShape(
    SkCanvas *canvas,
    const Shape &shape,
    const std::function<SkPoint(Vec2)> &mapPoint,
    float scale
) {
    SkSurface *surface = canvas->getSurface();
    if (!surface || !shape.visible || !shape.blurBackground) {
        return;
    }

    sk_sp<SkImage> snapshot = surface->makeImageSnapshot();
    if (!snapshot) {
        return;
    }

    SkPath mask;
    if (shape.type == ShapeType::Brush) {
        AddBrushMask(shape, mask, mapPoint, scale);
    } else {
        AddRectMask(shape, mask, mapPoint);
    }
    if (mask.isEmpty()) {
        return;
    }

    SkPaint blurPaint;
    blurPaint.setAntiAlias(true);
    const float sigma = std::max(0.0f, shape.blurRadius) * scale;
    blurPaint.setImageFilter(SkImageFilters::Blur(sigma, sigma, SkTileMode::kClamp, nullptr));

    SkPaint clearPaint;
    clearPaint.setAntiAlias(true);
    clearPaint.setBlendMode(SkBlendMode::kClear);

    canvas->save();
    canvas->clipPath(mask, true);
    canvas->drawPath(mask, clearPaint);
    canvas->drawImage(snapshot, 0.0f, 0.0f, SkSamplingOptions(), &blurPaint);
    canvas->restore();
}
} // namespace

bool Application::Init() {
    ResetDefaultPreferences();
    LoadPreferences();

    glfwSetErrorCallback(GlfwErrorCallback);

    if (!glfwInit()) {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    window_ = glfwCreateWindow(preferredWindowWidth_, preferredWindowHeight_, "TCanvas", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    defaultCursor_ = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    grabCursor_ = CreateGrabCursor();
    grabbingCursor_ = CreateGrabbingCursor();

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
        TrackWindowSizePreference();
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
    if (isPanningCanvas_ && grabbingCursor_) {
        glfwSetCursor(window_, grabbingCursor_);
    } else if (panToolActive && grabCursor_) {
        glfwSetCursor(window_, grabCursor_);
    } else {
        glfwSetCursor(window_, defaultCursor_);
    }

    HandleShortcuts();

    if (io.WantCaptureMouse) {
        return;
    }

    const Vec2 mouse{io.MousePos.x, io.MousePos.y};
    const Vec2 mouseWorld = view_.ScreenToWorld(mouse);

    if (editingTextIndex_ < 0 && !io.WantCaptureKeyboard &&
        (ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Delete))) {
        if (!document_.SelectedShapeIndices().empty()) {
            PushHistory();
        }
        document_.RemoveSelectedShapes();
        transformer_.EndDrag();
        snapGuides_.clear();
        isDrawingLine_ = false;
        isDrawingBox_ = false;
        isDrawingBrush_ = false;
        groupTransformActive_ = false;
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

    if (isSelectingArea_) {
        selectionCurrentScreen_ = mouse;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            document_.SelectShapes(ShapesInSelectionArea());
            isSelectingArea_ = false;
        }
        return;
    }

    if (isDrawingLine_) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            UpdateLineDrawing(io.KeyShift ? ConstrainLineEnd(lineStartWorld_, mouseWorld) : mouseWorld);
            return;
        }
        FinishLineDrawing();
        return;
    }

    if (isDrawingBox_) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            UpdateBoxDrawing(mouseWorld);
            return;
        }
        FinishBoxDrawing();
        return;
    }

    if (isDrawingBrush_) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            UpdateBrushStroke(mouseWorld);
            return;
        }
        FinishBrushStroke();
        return;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (activeTool_ != Tool::Select) {
            if (IsLineTool(activeTool_)) {
                BeginLineDrawing(activeTool_, mouseWorld);
            } else if (IsBoxTool(activeTool_)) {
                BeginBoxDrawing(activeTool_, mouseWorld);
            } else if (activeTool_ == Tool::Brush) {
                BeginBrushStroke(mouseWorld);
            } else if (activeTool_ == Tool::Text) {
                AddShapeFromTool(activeTool_);
                if (Shape *shape = document_.SelectedShape()) {
                    shape->position = mouseWorld;
                    shape->text.clear();
                }
                BeginTextEditing(document_.SelectedShapeIndex());
            }
            return;
        }

        DragMode mode = DragMode::None;
        Shape *selectedShape = document_.SelectedShape();
        int hitShapeIndex = -1;
        const auto &shapes = document_.Shapes();
        for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
            if (!shapes[i].visible || shapes[i].locked) {
                continue;
            }
            if (PointInShape(mouseWorld, shapes[i])) {
                hitShapeIndex = i;
                break;
            }
        }

        if (io.KeyShift && hitShapeIndex >= 0) {
            std::vector<int> selected = document_.SelectedShapeIndices();
            const auto found = std::find(selected.begin(), selected.end(), hitShapeIndex);
            if (found == selected.end()) {
                selected.push_back(hitShapeIndex);
            } else if (selected.size() > 1) {
                selected.erase(found);
            }
            document_.SelectShapes(std::move(selected));
            transformer_.EndDrag();
            return;
        }

        const bool hasMultiSelection = document_.SelectedShapeIndices().size() > 1;
        if (hasMultiSelection) {
            groupBounds_ = SelectionBounds();
            mode = transformer_.HitTest(mouse, groupBounds_, view_);
            if (mode == DragMode::Rotate) {
                mode = DragMode::None;
            }
            if (mode == DragMode::Move && hitShapeIndex >= 0 && !document_.IsShapeSelected(hitShapeIndex)) {
                mode = DragMode::None;
            }
            if (mode != DragMode::None) {
                BeginGroupTransform(mode, mouseWorld);
                return;
            }
        }

        if (selectedShape && !hasMultiSelection) {
            mode = transformer_.HitTest(mouse, *selectedShape, view_);
            if (mode == DragMode::Move && hitShapeIndex >= 0 && hitShapeIndex != document_.SelectedShapeIndex()) {
                mode = DragMode::None;
            }
        }

        if (mode == DragMode::None) {
            if (hitShapeIndex >= 0) {
                document_.SelectShape(hitShapeIndex);
                selectedShape = document_.SelectedShape();
                mode = DragMode::Move;
            }
        }

        if (mode == DragMode::None) {
            isSelectingArea_ = true;
            selectionStartScreen_ = mouse;
            selectionCurrentScreen_ = mouse;
            document_.SelectShape(-1);
            transformer_.EndDrag();
            selectedShape = nullptr;
        }

        if (selectedShape && mode != DragMode::None) {
            transformHistoryPushed_ = false;
            transformer_.BeginDrag(mode, mouseWorld, *selectedShape);
        }
    }

    if (Shape *selectedShape = document_.SelectedShape();
        selectedShape && !groupTransformActive_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) &&
        transformer_.ActiveMode() != DragMode::None) {
        if (!transformHistoryPushed_) {
            PushHistory();
            transformHistoryPushed_ = true;
        }
        transformer_.UpdateDrag(mouseWorld, *selectedShape, io.KeyShift);
        if (transformer_.ActiveMode() == DragMode::Move) {
            ApplySnapping(*selectedShape);
        } else {
            snapGuides_.clear();
        }
    }

    if (groupTransformActive_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        if (!transformHistoryPushed_) {
            PushHistory();
            transformHistoryPushed_ = true;
        }
        UpdateGroupTransform(mouseWorld, io.KeyShift);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        transformer_.EndDrag();
        FinishGroupTransform();
        transformHistoryPushed_ = false;
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
    const bool shiftDown =
        glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS ||
        io.KeyShift;

    if (macCommandDown && shiftDown && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        Redo();
        return;
    }
    if (macCommandDown && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        Undo();
        return;
    }
    if (macCommandDown && shiftDown && ImGui::IsKeyPressed(ImGuiKey_A)) {
        SelectAllShapes(true);
        transformer_.EndDrag();
        return;
    }
    if (macCommandDown && ImGui::IsKeyPressed(ImGuiKey_A)) {
        SelectAllShapes(false);
        transformer_.EndDrag();
        return;
    }
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

    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
        ReorderSelectedShapes(false, shiftDown);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
        ReorderSelectedShapes(true, shiftDown);
        return;
    }

    if (io.KeySuper || io.KeyAlt || io.KeyCtrl) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_V)) {
        ToggleSelectedVisibility();
    } else if (ImGui::IsKeyPressed(ImGuiKey_K)) {
        ToggleSelectedLock();
    } else if (ImGui::IsKeyPressed(ImGuiKey_S)) {
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
    } else if (ImGui::IsKeyPressed(ImGuiKey_B)) {
        activeTool_ = Tool::Brush;
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

    auto &shapes = document_.Shapes();
    for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
        Shape &layerShape = shapes[i];
        const bool selected = document_.IsShapeSelected(i);
        ImGui::PushID(i);
        const float rowStartX = ImGui::GetCursorPosX();
        const float buttonSize = 24.0f;
        const float buttonGap = 4.0f;
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        const float labelWidth = std::max(40.0f, rowWidth - buttonSize * 2.0f - buttonGap * 2.0f);
        ImGui::BeginDisabled(!layerShape.visible || layerShape.locked);
        if (ImGui::Selectable(layerShape.name.c_str(), selected, 0, ImVec2(labelWidth, 0.0f))) {
            if (ImGui::GetIO().KeyShift && lastLayerSelectionIndex_ >= 0) {
                std::vector<int> range;
                const int start = std::min(lastLayerSelectionIndex_, i);
                const int end = std::max(lastLayerSelectionIndex_, i);
                range.reserve(static_cast<size_t>(end - start + 1));
                for (int index = start; index <= end; ++index) {
                    if (shapes[index].visible && !shapes[index].locked) {
                        range.push_back(index);
                    }
                }
                document_.SelectShapes(std::move(range));
            } else {
                document_.SelectShape(i);
                lastLayerSelectionIndex_ = i;
            }
            transformer_.EndDrag();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select layer\nShift + Click: select range");
        }
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("SHAPE_INDEX", &i, sizeof(int));
            ImGui::TextUnformatted(layerShape.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SHAPE_INDEX")) {
                const int from = *static_cast<const int *>(payload->Data);
                PushHistory();
                document_.MoveShape(from, i);
                transformer_.EndDrag();
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(rowStartX + labelWidth + buttonGap);
        if (ImGui::Button(layerShape.visible ? "V" : "-", ImVec2(buttonSize, 22.0f))) {
            PushHistory();
            layerShape.visible = !layerShape.visible;
            if (!layerShape.visible && document_.IsShapeSelected(i)) {
                document_.SelectShape(-1);
                transformer_.EndDrag();
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nShortcut: V", layerShape.visible ? "Hide" : "Show");
        }
        ImGui::SameLine(rowStartX + labelWidth + buttonGap + buttonSize + buttonGap);
        if (ImGui::Button(layerShape.locked ? "L" : "U", ImVec2(buttonSize, 22.0f))) {
            PushHistory();
            layerShape.locked = !layerShape.locked;
            if (layerShape.locked && document_.IsShapeSelected(i)) {
                document_.SelectShape(-1);
                transformer_.EndDrag();
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nShortcut: K", layerShape.locked ? "Unlock" : "Lock");
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
    const std::vector<int> selectedIndices = document_.SelectedShapeIndices();
    if (selectedIndices.size() > 1) {
        ImGui::Text("%zu selected", selectedIndices.size());
        ImGui::SeparatorText("Common");

        Color fill = document_.Shapes()[selectedIndices.front()].fill;
        Color border = document_.Shapes()[selectedIndices.front()].border;
        bool fillEnabled = document_.Shapes()[selectedIndices.front()].fillEnabled;
        bool borderEnabled = document_.Shapes()[selectedIndices.front()].borderEnabled;
        float borderWidth = document_.Shapes()[selectedIndices.front()].borderWidth;
        float radius = document_.Shapes()[selectedIndices.front()].cornerRadius;
        bool blurBackground = document_.Shapes()[selectedIndices.front()].blurBackground;
        float blurRadius = document_.Shapes()[selectedIndices.front()].blurRadius;

        auto applySelected = [&](auto fn) {
            PushHistory();
            for (int index: selectedIndices) {
                fn(document_.Shapes()[index]);
            }
        };

        if (ImGui::ColorEdit4("Fill", &fill.r)) {
            applySelected([&](Shape &target) { target.fill = fill; });
        }
        if (ImGui::Checkbox("Show Fill", &fillEnabled)) {
            applySelected([&](Shape &target) { target.fillEnabled = fillEnabled; });
        }
        if (ImGui::ColorEdit4("Border", &border.r)) {
            applySelected([&](Shape &target) { target.border = border; });
        }
        if (ImGui::Checkbox("Show Border", &borderEnabled)) {
            applySelected([&](Shape &target) { target.borderEnabled = borderEnabled; });
        }
        if (ImGui::DragFloat("Border Width", &borderWidth, 0.25f, 0.0f, 100.0f)) {
            borderWidth = Clamp(borderWidth, 0.0f, 100.0f);
            applySelected([&](Shape &target) { target.borderWidth = borderWidth; });
        }
        if (ImGui::DragFloat("Radius", &radius, 0.5f, 0.0f, 1000.0f)) {
            radius = Clamp(radius, 0.0f, 1000.0f);
            applySelected([&](Shape &target) { target.cornerRadius = radius; });
        }
        if (ImGui::Checkbox("Background Blur", &blurBackground)) {
            applySelected([&](Shape &target) { target.blurBackground = blurBackground; });
        }
        if (ImGui::DragFloat("Blur Radius", &blurRadius, 0.25f, 0.0f, 80.0f)) {
            blurRadius = Clamp(blurRadius, 0.0f, 80.0f);
            applySelected([&](Shape &target) { target.blurRadius = blurRadius; });
        }

        ImGui::BeginDisabled();
        ImGui::SeparatorText("Transform");
        float disabledVec[2] = {};
        ImGui::DragFloat2("Position", disabledVec);
        ImGui::DragFloat2("Size", disabledVec);
        float disabledRotation = 0.0f;
        ImGui::DragFloat("Rotation", &disabledRotation);
        ImGui::EndDisabled();
    } else if (shape) {
        char nameBuffer[128] = {};
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", shape->name.c_str());
        bool captureInspectorEdit = false;
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            shape->name = nameBuffer;
        }
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();

        ImGui::SeparatorText("Transform");
        ImGui::DragFloat2("Position", &shape->position.x, 1.0f);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        ImGui::DragFloat2("Size", &shape->size.x, 1.0f, 20.0f, 4000.0f);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        ImGui::DragFloat("Rotation", &shape->rotation, 0.5f, -360.0f, 360.0f);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();

        ImGui::SeparatorText("Appearance");
        if (shape->type == ShapeType::Text) {
            std::array<char, 4096> contentBuffer{};
            std::strncpy(contentBuffer.data(), shape->text.c_str(), contentBuffer.size() - 1);
            ImGui::InputTextMultiline(
                "Content",
                contentBuffer.data(),
                contentBuffer.size(),
                ImVec2(-1.0f, 96.0f)
            );
            if (ImGui::IsItemActivated()) {
                PushHistory();
            }
            if (ImGui::IsItemEdited()) {
                shape->text = contentBuffer.data();
            }
        }
        ImGui::Checkbox("Show Fill", &shape->fillEnabled);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        ImGui::ColorEdit4("Fill", &shape->fill.r);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        ImGui::Checkbox("Show Border", &shape->borderEnabled);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        ImGui::ColorEdit4("Border", &shape->border.r);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        ImGui::DragFloat("Border Width", &shape->borderWidth, 0.25f, 0.0f, 100.0f);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        if (shape->type == ShapeType::Arrow) {
            ImGui::DragFloat("Arrow Head", &shape->arrowHeadSize, 0.5f, 6.0f, 120.0f);
            captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        }
        ImGui::DragFloat("Radius", &shape->cornerRadius, 0.5f, 0.0f, 1000.0f);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();

        ImGui::SeparatorText("Effects");
        ImGui::Checkbox("Background Blur", &shape->blurBackground);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        ImGui::DragFloat("Blur Radius", &shape->blurRadius, 0.25f, 0.0f, 80.0f);
        captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        if (shape->type == ShapeType::Brush) {
            ImGui::DragFloat("Brush Size", &shape->brushSize, 0.5f, 4.0f, 240.0f);
            captureInspectorEdit = captureInspectorEdit || ImGui::IsItemActivated();
        }

        if (captureInspectorEdit) {
            PushHistory();
        }

        shape->size.x = Clamp(shape->size.x, 20.0f, 4000.0f);
        shape->size.y = Clamp(shape->size.y, 20.0f, 4000.0f);
        shape->borderWidth = Clamp(shape->borderWidth, 0.0f, 100.0f);
        shape->arrowHeadSize = Clamp(shape->arrowHeadSize, 6.0f, 120.0f);
        shape->cornerRadius = Clamp(shape->cornerRadius, 0.0f, 1000.0f);
        shape->blurRadius = Clamp(shape->blurRadius, 0.0f, 80.0f);
        shape->brushSize = Clamp(shape->brushSize, 4.0f, 240.0f);
    } else {
        ImGui::TextUnformatted("No selection");
    }

    ImGui::End();
}

void Application::RenderToolbar() {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    constexpr float toolbarWidth = 890.0f;
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
            } else {
                activeTool_ = tool;
            }
            transformer_.EndDrag();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s (%s)", label, ToolShortcut(tool));
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
    toolButton(Tool::Brush, "Brush");
    if (ImGui::Button("Export", ImVec2(74.0f, 32.0f))) {
        OpenExportDialog();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Export selection");
    }
    ImGui::SameLine();
    if (ImGui::Button("Settings", ImVec2(76.0f, 32.0f))) {
        showPreferencesDialog_ = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Settings");
    }

    ImGui::End();
}

void Application::RenderBrushControls() {
    if (activeTool_ != Tool::Brush) {
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    constexpr float panelWidth = 272.0f;
    constexpr float panelHeight = 78.0f;
    const ImVec2 pos{
        viewport->WorkPos.x + (viewport->WorkSize.x - panelWidth) * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y - 52.0f - panelHeight - 28.0f
    };

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::Begin("BrushControls", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Brush");
    ImGui::SameLine();
    ImGui::SetCursorPosX(panelWidth - 76.0f);
    ImGui::SetNextItemWidth(56.0f);
    ImGui::InputFloat("##BrushSizeInput", &brushToolSize_, 1.0f, 8.0f, "%.0f");
    brushToolSize_ = Clamp(brushToolSize_, 4.0f, 240.0f);

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##BrushSizeSlider", &brushToolSize_, 4.0f, 240.0f, "Size %.0f");
    brushToolSize_ = Clamp(brushToolSize_, 4.0f, 240.0f);

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
    const bool deactivated = ImGui::IsItemDeactivated();

    ImGuiIO &io = ImGui::GetIO();
    if (submitted && (io.KeyShift || io.KeySuper)) {
        InsertTextEditorNewline();
        focusTextEditor_ = true;
    }

    shape->text = textEditBuffer_.data();

    if (submitted && !io.KeyShift && !io.KeySuper) {
        FinishTextEditing();
    } else if (deactivated) {
        FinishTextEditing();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        FinishTextEditing();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::End();
}

void Application::RenderExportDialog() {
    if (!showExportDialog_) {
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 dialogSize{viewport->WorkSize.x * 0.8f, viewport->WorkSize.y * 0.8f};
    ImGui::SetNextWindowPos(
        ImVec2{
            viewport->WorkPos.x + (viewport->WorkSize.x - dialogSize.x) * 0.5f,
            viewport->WorkPos.y + (viewport->WorkSize.y - dialogSize.y) * 0.5f
        },
        ImGuiCond_Appearing
    );
    ImGui::SetNextWindowSize(dialogSize, ImGuiCond_Appearing);
    ImGui::Begin("Export Selection", &showExportDialog_,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    if (exportVariations_.empty()) {
        OpenExportDialog();
    }
    if (exportVariations_.empty()) {
        showExportDialog_ = false;
        ImGui::End();
        return;
    }
    activeExportVariation_ = Clamp(activeExportVariation_, 0, static_cast<int>(exportVariations_.size()) - 1);

    ImGui::BeginChild("ExportOptions", ImVec2(360.0f, 0.0f), true);
    ImGui::TextUnformatted("Variations");
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(exportVariations_.size()); ++i) {
        ExportVariation &variation = exportVariations_[i];
        ImGui::PushID(i);
        const bool selected = activeExportVariation_ == i;
        if (ImGui::Selectable("##VariationSelect", selected, 0, ImVec2(22.0f, 26.0f))) {
            activeExportVariation_ = i;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(84.0f);
        if (ImGui::InputInt("W", &variation.width, 0, 0)) {
            variation.width = std::max(1, variation.width);
            variation.data = nullptr;
            variation.cachedWidth = 0;
            exportPreviewTextureData_ = nullptr;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(84.0f);
        if (ImGui::InputInt("H", &variation.height, 0, 0)) {
            variation.height = std::max(1, variation.height);
            variation.data = nullptr;
            variation.cachedHeight = 0;
            exportPreviewTextureData_ = nullptr;
        }
        sk_sp<SkData> data = ExportVariationData(variation);
        ImGui::SameLine();
        ImGui::TextDisabled("%.1f KB", data ? static_cast<float>(data->size()) / 1024.0f : 0.0f);
        ImGui::PopID();
    }

    if (ImGui::Button("Add", ImVec2(82.0f, 30.0f))) {
        const ExportVariation seed = exportVariations_[activeExportVariation_];
        exportVariations_.push_back({seed.width * 2, seed.height * 2});
        activeExportVariation_ = static_cast<int>(exportVariations_.size()) - 1;
        exportPreviewTextureData_ = nullptr;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(exportVariations_.size() <= 1);
    if (ImGui::Button("Remove", ImVec2(92.0f, 30.0f))) {
        exportVariations_.erase(exportVariations_.begin() + activeExportVariation_);
        activeExportVariation_ = Clamp(activeExportVariation_, 0, static_cast<int>(exportVariations_.size()) - 1);
        exportPreviewTextureData_ = nullptr;
    }
    ImGui::EndDisabled();

    ExportVariation &active = exportVariations_[activeExportVariation_];
    sk_sp<SkData> activeData = ExportVariationData(active);

    ImGui::Separator();
    const bool canExport = activeData != nullptr;
    ImGui::BeginDisabled(!canExport);
    if (ImGui::Button("Copy to Clipboard", ImVec2(-1.0f, 32.0f))) {
        WriteClipboardImageData(activeData);
    }
    if (ImGui::Button(exportVariations_.size() > 1 ? "Save All Files" : "Save File", ImVec2(-1.0f, 32.0f))) {
        const std::string path = SavePngFileDialog();
        if (!path.empty()) {
            const bool multiple = exportVariations_.size() > 1;
            for (ExportVariation &variation: exportVariations_) {
                SaveDataToFile(
                    ExportVariantPath(path, variation.width, variation.height, multiple),
                    ExportVariationData(variation)
                );
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("ExportPreview", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Preview");
    ImGui::Separator();
    const ImVec2 previewAvail = ImGui::GetContentRegionAvail();
    RenderExportPreview(activeData, {previewAvail.x, previewAvail.y});
    ImGui::EndChild();

    ImGui::End();
}

void Application::RenderExportPreview(sk_sp<SkData> data, Vec2 size) {
    if (!data || size.x <= 1.0f || size.y <= 1.0f) {
        ImGui::TextDisabled("No preview");
        return;
    }

    if (exportPreviewTexture_ == 0 || exportPreviewTextureData_ != data.get()) {
        sk_sp<SkImage> image = SkImages::DeferredFromEncodedData(data);
        if (!image) {
            ImGui::TextDisabled("Preview unavailable");
            return;
        }

        const SkImageInfo info = SkImageInfo::Make(
            image->width(),
            image->height(),
            kRGBA_8888_SkColorType,
            kPremul_SkAlphaType
        );
        std::vector<unsigned char> pixels(static_cast<size_t>(info.computeMinByteSize()));
        if (!image->readPixels(info, pixels.data(), info.minRowBytes(), 0, 0)) {
            ImGui::TextDisabled("Preview unavailable");
            return;
        }

        if (exportPreviewTexture_ == 0) {
            glGenTextures(1, &exportPreviewTexture_);
        }
        glBindTexture(GL_TEXTURE_2D, exportPreviewTexture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            image->width(),
            image->height(),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pixels.data()
        );
        glBindTexture(GL_TEXTURE_2D, 0);
        exportPreviewTextureWidth_ = image->width();
        exportPreviewTextureHeight_ = image->height();
        exportPreviewTextureData_ = data.get();
    }

    const float scale = std::min(
        size.x / static_cast<float>(std::max(1, exportPreviewTextureWidth_)),
        size.y / static_cast<float>(std::max(1, exportPreviewTextureHeight_))
    );
    const ImVec2 imageSize{
        static_cast<float>(exportPreviewTextureWidth_) * scale,
        static_cast<float>(exportPreviewTextureHeight_) * scale
    };
    ImGui::Image(
        static_cast<ImTextureID>(static_cast<uintptr_t>(exportPreviewTexture_)),
        imageSize
    );
}

void Application::RenderPreferencesDialog() {
    if (!showPreferencesDialog_) {
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 dialogSize{viewport->WorkSize.x * 0.8f, viewport->WorkSize.y * 0.8f};
    ImGui::SetNextWindowPos(
        ImVec2{
            viewport->WorkPos.x + (viewport->WorkSize.x - dialogSize.x) * 0.5f,
            viewport->WorkPos.y + (viewport->WorkSize.y - dialogSize.y) * 0.5f
        },
        ImGuiCond_Appearing
    );
    ImGui::SetNextWindowSize(dialogSize, ImGuiCond_Appearing);
    ImGui::Begin("Preferences", &showPreferencesDialog_,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextUnformatted("Default Shape Styles");
    ImGui::Separator();

    bool changed = false;
    constexpr ShapeType types[] = {
        ShapeType::Rect,
        ShapeType::Circle,
        ShapeType::Line,
        ShapeType::Arrow,
        ShapeType::Text,
        ShapeType::Image,
        ShapeType::Brush
    };

    for (ShapeType type: types) {
        ShapePreferences &prefs = shapePreferences_[ShapePreferenceIndex(type)];
        ImGui::PushID(ShapePreferenceKey(type));
        if (ImGui::CollapsingHeader(ShapePreferenceLabel(type), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginGroup();
            changed = ImGui::Checkbox("Show Fill", &prefs.fillEnabled) || changed;
            changed = ImGui::ColorEdit4("Fill", &prefs.fill.r) || changed;
            changed = ImGui::Checkbox("Show Stroke", &prefs.borderEnabled) || changed;
            changed = ImGui::ColorEdit4("Stroke", &prefs.border.r) || changed;
            changed = ImGui::DragFloat("Stroke Width", &prefs.borderWidth, 0.25f, 0.0f, 100.0f) || changed;
            if (type == ShapeType::Arrow) {
                changed = ImGui::DragFloat("Arrow Head", &prefs.arrowHeadSize, 0.5f, 6.0f, 120.0f) || changed;
            }
            if (type == ShapeType::Rect || type == ShapeType::Image) {
                changed = ImGui::DragFloat("Corner Radius", &prefs.cornerRadius, 0.5f, 0.0f, 1000.0f) || changed;
            }
            if (type == ShapeType::Brush) {
                changed = ImGui::DragFloat("Brush Size", &prefs.brushSize, 0.5f, 4.0f, 240.0f) || changed;
            }
            changed = ImGui::Checkbox("Background Blur", &prefs.blurBackground) || changed;
            changed = ImGui::DragFloat("Blur Radius", &prefs.blurRadius, 0.25f, 0.0f, 80.0f) || changed;

            prefs.borderWidth = Clamp(prefs.borderWidth, 0.0f, 100.0f);
            prefs.arrowHeadSize = Clamp(prefs.arrowHeadSize, 6.0f, 120.0f);
            prefs.cornerRadius = Clamp(prefs.cornerRadius, 0.0f, 1000.0f);
            prefs.blurRadius = Clamp(prefs.blurRadius, 0.0f, 80.0f);
            prefs.brushSize = Clamp(prefs.brushSize, 4.0f, 240.0f);
            ImGui::EndGroup();

            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextUnformatted("Preview");
            DrawPreferencePreview(type, prefs, ImVec2(116.0f, 78.0f));
            ImGui::EndGroup();
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("Reset Defaults", ImVec2(132.0f, 30.0f))) {
        ResetDefaultPreferences();
        SavePreferences();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", PreferencesPath().c_str());

    if (changed) {
        brushToolSize_ = shapePreferences_[ShapePreferenceIndex(ShapeType::Brush)].brushSize;
        SavePreferences();
    }

    ImGui::End();
}

std::string Application::PreferencesPath() const {
    const char *home = std::getenv("HOME");
    const std::filesystem::path base = home && *home
        ? std::filesystem::path(home) / "Library" / "Application Support" / "TCanvas"
        : std::filesystem::temp_directory_path() / "TCanvas";
    return (base / "preferences.ini").string();
}

void Application::ResetDefaultPreferences() {
    ShapePreferences rect;
    rect.fill = kAccentRed;
    rect.border = kAccentRed;
    rect.fillEnabled = false;
    rect.borderEnabled = true;
    rect.borderWidth = 3.0f;
    rect.cornerRadius = 0.0f;

    ShapePreferences circle = rect;
    circle.cornerRadius = 1000.0f;

    ShapePreferences line = rect;
    line.borderWidth = 4.0f;

    ShapePreferences arrow = line;
    arrow.arrowHeadSize = 28.0f;

    ShapePreferences text = rect;
    text.fill = kAccentRed;
    text.border = kAccentRed;
    text.fillEnabled = true;
    text.borderEnabled = false;
    text.borderWidth = 0.0f;

    ShapePreferences image = rect;
    image.fill = {1.0f, 1.0f, 1.0f, 1.0f};
    image.fillEnabled = true;
    image.borderWidth = 3.0f;

    ShapePreferences brush = rect;
    brush.fillEnabled = false;
    brush.borderEnabled = false;
    brush.borderWidth = 0.0f;
    brush.blurBackground = true;
    brush.blurRadius = 14.0f;
    brush.brushSize = 52.0f;

    shapePreferences_[ShapePreferenceIndex(ShapeType::Rect)] = rect;
    shapePreferences_[ShapePreferenceIndex(ShapeType::Circle)] = circle;
    shapePreferences_[ShapePreferenceIndex(ShapeType::Line)] = line;
    shapePreferences_[ShapePreferenceIndex(ShapeType::Arrow)] = arrow;
    shapePreferences_[ShapePreferenceIndex(ShapeType::Text)] = text;
    shapePreferences_[ShapePreferenceIndex(ShapeType::Image)] = image;
    shapePreferences_[ShapePreferenceIndex(ShapeType::Brush)] = brush;
    brushToolSize_ = brush.brushSize;
}

void Application::LoadPreferences() {
    std::ifstream file(PreferencesPath());
    if (!file) {
        lastSavedWindowWidth_ = preferredWindowWidth_;
        lastSavedWindowHeight_ = preferredWindowHeight_;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const size_t equals = line.find('=');
        const size_t dot = line.find('.');
        if (equals == std::string::npos || dot == std::string::npos || dot > equals) {
            continue;
        }

        const std::string section = line.substr(0, dot);
        const std::string key = line.substr(dot + 1, equals - dot - 1);
        const std::string value = line.substr(equals + 1);

        if (section == "window") {
            try {
                if (key == "width") {
                    preferredWindowWidth_ = std::max(640, std::stoi(value));
                } else if (key == "height") {
                    preferredWindowHeight_ = std::max(480, std::stoi(value));
                }
            } catch (...) {
            }
            continue;
        }

        ShapeType type = ShapeType::Rect;
        bool matched = false;
        for (ShapeType candidate: {ShapeType::Rect, ShapeType::Circle, ShapeType::Line, ShapeType::Arrow,
                                  ShapeType::Text, ShapeType::Image, ShapeType::Brush}) {
            if (section == ShapePreferenceKey(candidate)) {
                type = candidate;
                matched = true;
                break;
            }
        }
        if (!matched) {
            continue;
        }

        ShapePreferences &prefs = shapePreferences_[ShapePreferenceIndex(type)];
        try {
            if (key == "fill") {
                ParseColor(value, prefs.fill);
            } else if (key == "fill_enabled") {
                prefs.fillEnabled = value == "1";
            } else if (key == "stroke") {
                ParseColor(value, prefs.border);
            } else if (key == "stroke_enabled") {
                prefs.borderEnabled = value == "1";
            } else if (key == "stroke_width") {
                prefs.borderWidth = std::stof(value);
            } else if (key == "arrow_head") {
                prefs.arrowHeadSize = std::stof(value);
            } else if (key == "radius") {
                prefs.cornerRadius = std::stof(value);
            } else if (key == "blur") {
                prefs.blurBackground = value == "1";
            } else if (key == "blur_radius") {
                prefs.blurRadius = std::stof(value);
            } else if (key == "brush_size") {
                prefs.brushSize = std::stof(value);
            }
        } catch (...) {
        }
    }

    brushToolSize_ = shapePreferences_[ShapePreferenceIndex(ShapeType::Brush)].brushSize;
    lastSavedWindowWidth_ = preferredWindowWidth_;
    lastSavedWindowHeight_ = preferredWindowHeight_;
}

void Application::SavePreferences() const {
    const std::filesystem::path path = PreferencesPath();
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path);
    if (!file) {
        return;
    }

    file << "# TCanvas preferences\n";
    file << "window.width=" << preferredWindowWidth_ << "\n";
    file << "window.height=" << preferredWindowHeight_ << "\n";
    for (ShapeType type: {ShapeType::Rect, ShapeType::Circle, ShapeType::Line, ShapeType::Arrow,
                         ShapeType::Text, ShapeType::Image, ShapeType::Brush}) {
        const ShapePreferences &prefs = shapePreferences_[ShapePreferenceIndex(type)];
        const char *key = ShapePreferenceKey(type);
        file << key << ".fill=" << FormatColor(prefs.fill) << "\n";
        file << key << ".fill_enabled=" << (prefs.fillEnabled ? 1 : 0) << "\n";
        file << key << ".stroke=" << FormatColor(prefs.border) << "\n";
        file << key << ".stroke_enabled=" << (prefs.borderEnabled ? 1 : 0) << "\n";
        file << key << ".stroke_width=" << prefs.borderWidth << "\n";
        file << key << ".arrow_head=" << prefs.arrowHeadSize << "\n";
        file << key << ".radius=" << prefs.cornerRadius << "\n";
        file << key << ".blur=" << (prefs.blurBackground ? 1 : 0) << "\n";
        file << key << ".blur_radius=" << prefs.blurRadius << "\n";
        file << key << ".brush_size=" << prefs.brushSize << "\n";
    }
}

void Application::TrackWindowSizePreference() {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(window_, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }

    if (width == lastSavedWindowWidth_ && height == lastSavedWindowHeight_) {
        return;
    }

    preferredWindowWidth_ = width;
    preferredWindowHeight_ = height;
    lastSavedWindowWidth_ = width;
    lastSavedWindowHeight_ = height;
    SavePreferences();
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

void Application::SelectAllShapes(bool includeLocked) {
    std::vector<int> all;
    const auto &shapes = document_.Shapes();
    all.reserve(shapes.size());
    for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
        if (!shapes[i].visible) {
            continue;
        }
        if (!includeLocked && shapes[i].locked) {
            continue;
        }
        all.push_back(i);
    }
    document_.SelectShapes(std::move(all));
}

void Application::ReorderSelectedShapes(bool towardFront, bool allTheWay) {
    std::vector<int> selected = document_.SelectedShapeIndices();
    if (selected.empty()) {
        return;
    }

    auto &shapes = document_.Shapes();
    if (shapes.size() <= 1) {
        return;
    }

    PushHistory();
    std::sort(selected.begin(), selected.end());
    std::vector<Shape> picked;
    picked.reserve(selected.size());
    for (int index: selected) {
        picked.push_back(shapes[index]);
    }

    if (allTheWay) {
        std::vector<Shape> rest;
        rest.reserve(shapes.size() - selected.size());
        for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
            if (!std::binary_search(selected.begin(), selected.end(), i)) {
                rest.push_back(std::move(shapes[i]));
            }
        }

        shapes.clear();
        std::vector<int> nextSelected;
        nextSelected.reserve(picked.size());
        if (towardFront) {
            shapes.insert(shapes.end(), picked.begin(), picked.end());
            shapes.insert(shapes.end(), rest.begin(), rest.end());
            for (int i = 0; i < static_cast<int>(picked.size()); ++i) {
                nextSelected.push_back(i);
            }
        } else {
            shapes.insert(shapes.end(), rest.begin(), rest.end());
            const int start = static_cast<int>(shapes.size());
            shapes.insert(shapes.end(), picked.begin(), picked.end());
            for (int i = 0; i < static_cast<int>(picked.size()); ++i) {
                nextSelected.push_back(start + i);
            }
        }
        document_.SelectShapes(std::move(nextSelected));
        transformer_.EndDrag();
        return;
    }

    std::vector<bool> isSelected(shapes.size(), false);
    for (int index: selected) {
        isSelected[index] = true;
    }

    if (towardFront) {
        for (int index: selected) {
            if (index > 0 && !isSelected[index - 1]) {
                std::swap(shapes[index], shapes[index - 1]);
                isSelected[index] = false;
                isSelected[index - 1] = true;
            }
        }
    } else {
        for (auto it = selected.rbegin(); it != selected.rend(); ++it) {
            const int index = *it;
            if (index + 1 < static_cast<int>(shapes.size()) && !isSelected[index + 1]) {
                std::swap(shapes[index], shapes[index + 1]);
                isSelected[index] = false;
                isSelected[index + 1] = true;
            }
        }
    }

    std::vector<int> nextSelected;
    for (int i = 0; i < static_cast<int>(isSelected.size()); ++i) {
        if (isSelected[i]) {
            nextSelected.push_back(i);
        }
    }
    document_.SelectShapes(std::move(nextSelected));
    transformer_.EndDrag();
}

void Application::ToggleSelectedVisibility() {
    const std::vector<int> selected = document_.SelectedShapeIndices();
    if (selected.empty()) {
        return;
    }

    const auto &shapes = document_.Shapes();
    const bool nextVisible = std::any_of(selected.begin(), selected.end(), [&](int index) {
        return index >= 0 && index < static_cast<int>(shapes.size()) && !shapes[index].visible;
    });

    PushHistory();
    for (int index: selected) {
        if (index >= 0 && index < static_cast<int>(document_.Shapes().size())) {
            document_.Shapes()[index].visible = nextVisible;
        }
    }
    transformer_.EndDrag();
}

void Application::ToggleSelectedLock() {
    const std::vector<int> selected = document_.SelectedShapeIndices();
    if (selected.empty()) {
        return;
    }

    const auto &shapes = document_.Shapes();
    const bool nextLocked = std::any_of(selected.begin(), selected.end(), [&](int index) {
        return index >= 0 && index < static_cast<int>(shapes.size()) && !shapes[index].locked;
    });

    PushHistory();
    for (int index: selected) {
        if (index >= 0 && index < static_cast<int>(document_.Shapes().size())) {
            document_.Shapes()[index].locked = nextLocked;
        }
    }
    transformer_.EndDrag();
}

void Application::ApplyPreferences(Shape &shape) const {
    const ShapePreferences &prefs = shapePreferences_[ShapePreferenceIndex(shape.type)];
    shape.fill = prefs.fill;
    shape.border = prefs.border;
    shape.fillEnabled = prefs.fillEnabled;
    shape.borderEnabled = prefs.borderEnabled;
    shape.borderWidth = prefs.borderWidth;
    shape.arrowHeadSize = prefs.arrowHeadSize;
    shape.cornerRadius = prefs.cornerRadius;
    shape.blurBackground = prefs.blurBackground;
    shape.blurRadius = prefs.blurRadius;
    if (shape.type == ShapeType::Brush) {
        shape.brushSize = prefs.brushSize;
    }
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
        case Tool::Brush:
            shape.type = ShapeType::Brush;
            shape.name = "Blur Brush " + std::to_string(document_.Shapes().size() + 1);
            break;
        case Tool::Select:
        case Tool::Pan:
            return;
    }

    ApplyPreferences(shape);
    if (shape.type == ShapeType::Brush) {
        shape.size = {shape.brushSize, shape.brushSize};
    }

    PushHistory();
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
    ApplyPreferences(shape);
    PushHistory();
    document_.AddShape(std::move(shape));
}

void Application::CopySelection() {
    const auto &selected = document_.SelectedShapeIndices();
    if (selected.empty()) {
        return;
    }

    copiedShapes_.clear();
    copiedShapes_.reserve(selected.size());
    for (int index: selected) {
        copiedShapes_.push_back(document_.Shapes()[index]);
    }
    hasCopiedShape_ = !copiedShapes_.empty();

    const Shape bounds = SelectionBounds();
    const int width = static_cast<int>(std::ceil(bounds.size.x));
    const int height = static_cast<int>(std::ceil(bounds.size.y));
    if (WriteClipboardImageData(EncodeSelectionPng(width, height))) {
        copiedClipboardChangeCount_ = ClipboardChangeCount();
    }
}

void Application::PasteSelectionOrClipboardImage() {
    const bool clipboardStillInternal =
        hasCopiedShape_ &&
        copiedClipboardChangeCount_ >= 0 &&
        ClipboardChangeCount() == copiedClipboardChangeCount_;

    if (clipboardStillInternal) {
        std::vector<Shape> pastedShapes;
        pastedShapes.reserve(copiedShapes_.size());
        for (Shape shape: copiedShapes_) {
            shape.name += " Copy";
            shape.position = shape.position + Vec2{24.0f, 24.0f};
            pastedShapes.push_back(std::move(shape));
        }

        PushHistory();
        document_.Shapes().insert(document_.Shapes().begin(), pastedShapes.begin(), pastedShapes.end());
        std::vector<int> selected;
        selected.reserve(pastedShapes.size());
        for (int i = 0; i < static_cast<int>(pastedShapes.size()); ++i) {
            selected.push_back(i);
        }
        document_.SelectShapes(std::move(selected));
        copiedShapes_ = std::move(pastedShapes);
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
        PushHistory();
        document_.AddShape(std::move(shape));
        transformer_.EndDrag();
        return;
    }

    if (hasCopiedShape_) {
        std::vector<Shape> pastedShapes;
        pastedShapes.reserve(copiedShapes_.size());
        for (Shape shape: copiedShapes_) {
            shape.name += " Copy";
            shape.position = shape.position + Vec2{24.0f, 24.0f};
            pastedShapes.push_back(std::move(shape));
        }

        PushHistory();
        document_.Shapes().insert(document_.Shapes().begin(), pastedShapes.begin(), pastedShapes.end());
        std::vector<int> selected;
        selected.reserve(pastedShapes.size());
        for (int i = 0; i < static_cast<int>(pastedShapes.size()); ++i) {
            selected.push_back(i);
        }
        document_.SelectShapes(std::move(selected));
        copiedShapes_ = std::move(pastedShapes);
        transformer_.EndDrag();
    }
}

sk_sp<SkData> Application::EncodeSelectionPng(int width, int height) const {
    const auto &selected = document_.SelectedShapeIndices();
    if (selected.empty() || width <= 0 || height <= 0) {
        return nullptr;
    }

    const Shape bounds = SelectionBounds();
    if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) {
        return nullptr;
    }

    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
    if (!surface) {
        return nullptr;
    }

    SkCanvas *canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    const float left = bounds.position.x - bounds.size.x * 0.5f;
    const float top = bounds.position.y - bounds.size.y * 0.5f;
    const float scaleX = static_cast<float>(width) / bounds.size.x;
    const float scaleY = static_cast<float>(height) / bounds.size.y;
    canvas->save();
    canvas->scale(scaleX, scaleY);
    canvas->translate(-left, -top);

    const auto &shapes = document_.Shapes();
    for (int i = static_cast<int>(shapes.size()) - 1; i >= 0; --i) {
        if (!shapes[i].visible || std::find(selected.begin(), selected.end(), i) == selected.end()) {
            continue;
        }
        if (shapes[i].blurBackground) {
            canvas->restore();
            ApplyBlurShape(
                canvas,
                shapes[i],
                [&](Vec2 world) { return ExportPoint(world, left, top, scaleX, scaleY); },
                (scaleX + scaleY) * 0.5f
            );
            canvas->save();
            canvas->scale(scaleX, scaleY);
            canvas->translate(-left, -top);
        }
        RenderShape(canvas, shapes[i]);
    }
    canvas->restore();

    SkPixmap pixmap;
    if (!surface->peekPixels(&pixmap)) {
        return nullptr;
    }

    SkPngEncoder::Options options;
    return SkPngEncoder::Encode(pixmap, options);
}

void Application::OpenExportDialog() {
    if (document_.SelectedShapeIndices().empty()) {
        return;
    }

    const Shape bounds = SelectionBounds();
    exportVariations_.clear();
    exportVariations_.push_back({
        std::max(1, static_cast<int>(std::ceil(bounds.size.x))),
        std::max(1, static_cast<int>(std::ceil(bounds.size.y)))
    });
    activeExportVariation_ = 0;
    exportPreviewTextureData_ = nullptr;
    showExportDialog_ = true;
}

sk_sp<SkData> Application::ExportVariationData(ExportVariation &variation) {
    if (!variation.data ||
        variation.cachedWidth != variation.width ||
        variation.cachedHeight != variation.height ||
        variation.cachedDocumentVersion != documentVersion_) {
        variation.width = std::max(1, variation.width);
        variation.height = std::max(1, variation.height);
        variation.data = EncodeSelectionPng(variation.width, variation.height);
        variation.cachedWidth = variation.width;
        variation.cachedHeight = variation.height;
        variation.cachedDocumentVersion = documentVersion_;
    }
    return variation.data;
}

std::string Application::ExportVariantPath(const std::string &path, int width, int height, bool multiple) const {
    if (!multiple) {
        return path;
    }

    const std::filesystem::path original(path);
    const std::string stem = original.stem().string();
    const std::string ext = original.extension().empty() ? ".png" : original.extension().string();
    const std::string filename = stem + "_" + std::to_string(width) + "x" + std::to_string(height) + ext;
    return (original.parent_path() / filename).string();
}

bool Application::SaveDataToFile(const std::string &path, sk_sp<SkData> data) const {
    if (path.empty() || !data) {
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write(static_cast<const char *>(data->data()), static_cast<std::streamsize>(data->size()));
    return file.good();
}

Application::DocumentSnapshot Application::CaptureDocumentSnapshot() const {
    return {document_.Shapes(), document_.SelectedShapeIndices()};
}

void Application::RestoreDocumentSnapshot(const DocumentSnapshot &snapshot) {
    document_.ReplaceContents(snapshot.shapes, snapshot.selectedShapes);
    transformer_.EndDrag();
    snapGuides_.clear();
    isDrawingLine_ = false;
    isDrawingBox_ = false;
    isDrawingBrush_ = false;
    isSelectingArea_ = false;
    groupTransformActive_ = false;
    editingTextIndex_ = -1;
    MarkDocumentChanged();
}

void Application::PushHistory() {
    undoStack_.push_back(CaptureDocumentSnapshot());
    redoStack_.clear();
    constexpr size_t maxHistory = 100;
    if (undoStack_.size() > maxHistory) {
        undoStack_.erase(undoStack_.begin());
    }
    MarkDocumentChanged();
}

void Application::MarkDocumentChanged() {
    ++documentVersion_;
    InvalidateExportCache();
}

void Application::InvalidateExportCache() {
    for (ExportVariation &variation: exportVariations_) {
        variation.data = nullptr;
        variation.cachedWidth = 0;
        variation.cachedHeight = 0;
        variation.cachedDocumentVersion = -1;
    }
    exportPreviewTextureData_ = nullptr;
}

void Application::Undo() {
    if (undoStack_.empty()) {
        return;
    }

    redoStack_.push_back(CaptureDocumentSnapshot());
    const DocumentSnapshot snapshot = std::move(undoStack_.back());
    undoStack_.pop_back();
    RestoreDocumentSnapshot(snapshot);
}

void Application::Redo() {
    if (redoStack_.empty()) {
        return;
    }

    undoStack_.push_back(CaptureDocumentSnapshot());
    const DocumentSnapshot snapshot = std::move(redoStack_.back());
    redoStack_.pop_back();
    RestoreDocumentSnapshot(snapshot);
}

Shape Application::SelectionBounds() const {
    Shape bounds;
    bounds.type = ShapeType::Rect;
    bounds.rotation = 0.0f;

    const auto &indices = document_.SelectedShapeIndices();
    if (indices.empty()) {
        bounds.size = {0.0f, 0.0f};
        return bounds;
    }

    bool hasBounds = false;
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    for (int index: indices) {
        const Shape &shape = document_.Shapes()[index];
        const auto corners = GetShapeCorners(shape);
        const float shapeLeft = std::min({corners[0].x, corners[1].x, corners[2].x, corners[3].x});
        const float shapeRight = std::max({corners[0].x, corners[1].x, corners[2].x, corners[3].x});
        const float shapeTop = std::min({corners[0].y, corners[1].y, corners[2].y, corners[3].y});
        const float shapeBottom = std::max({corners[0].y, corners[1].y, corners[2].y, corners[3].y});

        if (!hasBounds) {
            left = shapeLeft;
            right = shapeRight;
            top = shapeTop;
            bottom = shapeBottom;
            hasBounds = true;
        } else {
            left = std::min(left, shapeLeft);
            right = std::max(right, shapeRight);
            top = std::min(top, shapeTop);
            bottom = std::max(bottom, shapeBottom);
        }
    }

    bounds.position = {(left + right) * 0.5f, (top + bottom) * 0.5f};
    bounds.size = {std::max(1.0f, right - left), std::max(1.0f, bottom - top)};
    return bounds;
}

std::vector<int> Application::ShapesInSelectionArea() const {
    const float left = std::min(selectionStartScreen_.x, selectionCurrentScreen_.x);
    const float right = std::max(selectionStartScreen_.x, selectionCurrentScreen_.x);
    const float top = std::min(selectionStartScreen_.y, selectionCurrentScreen_.y);
    const float bottom = std::max(selectionStartScreen_.y, selectionCurrentScreen_.y);

    std::vector<int> selected;
    const auto &shapes = document_.Shapes();
    for (int i = 0; i < static_cast<int>(shapes.size()); ++i) {
        if (!shapes[i].visible || shapes[i].locked) {
            continue;
        }
        const auto corners = GetShapeCorners(shapes[i]);
        float shapeLeft = view_.WorldToScreen(corners[0]).x;
        float shapeRight = shapeLeft;
        float shapeTop = view_.WorldToScreen(corners[0]).y;
        float shapeBottom = shapeTop;
        for (int c = 1; c < 4; ++c) {
            const Vec2 screen = view_.WorldToScreen(corners[c]);
            shapeLeft = std::min(shapeLeft, screen.x);
            shapeRight = std::max(shapeRight, screen.x);
            shapeTop = std::min(shapeTop, screen.y);
            shapeBottom = std::max(shapeBottom, screen.y);
        }

        const bool overlaps = shapeLeft <= right && shapeRight >= left && shapeTop <= bottom && shapeBottom >= top;
        if (overlaps) {
            selected.push_back(i);
        }
    }
    return selected;
}

void Application::BeginGroupTransform(DragMode mode, Vec2 mouseWorld) {
    groupTransformActive_ = true;
    groupStartMouseWorld_ = mouseWorld;
    groupBounds_ = SelectionBounds();
    groupStartBounds_ = groupBounds_;
    groupStartIndices_ = document_.SelectedShapeIndices();
    groupStartShapes_.clear();
    groupStartShapes_.reserve(groupStartIndices_.size());
    for (int index: groupStartIndices_) {
        groupStartShapes_.push_back(document_.Shapes()[index]);
    }
    transformHistoryPushed_ = false;
    transformer_.BeginDrag(mode, mouseWorld, groupBounds_);
}

void Application::UpdateGroupTransform(Vec2 mouseWorld, bool keepAspectRatio) {
    Shape nextBounds = groupStartBounds_;
    transformer_.UpdateDrag(mouseWorld, nextBounds, keepAspectRatio);

    const float sx = groupStartBounds_.size.x == 0.0f ? 1.0f : nextBounds.size.x / groupStartBounds_.size.x;
    const float sy = groupStartBounds_.size.y == 0.0f ? 1.0f : nextBounds.size.y / groupStartBounds_.size.y;

    for (size_t i = 0; i < groupStartIndices_.size(); ++i) {
        Shape next = groupStartShapes_[i];
        const Vec2 offset = groupStartShapes_[i].position - groupStartBounds_.position;
        next.position = nextBounds.position + Vec2{offset.x * sx, offset.y * sy};
        next.size = {std::max(1.0f, groupStartShapes_[i].size.x * std::abs(sx)),
                     std::max(1.0f, groupStartShapes_[i].size.y * std::abs(sy))};
        next.brushSize = std::max(1.0f, groupStartShapes_[i].brushSize * (std::abs(sx) + std::abs(sy)) * 0.5f);
        if (next.type == ShapeType::Brush) {
            next.brushPoints.clear();
            for (Vec2 point: groupStartShapes_[i].brushPoints) {
                next.brushPoints.push_back({point.x * sx, point.y * sy});
            }
        }
        document_.Shapes()[groupStartIndices_[i]] = std::move(next);
    }

    groupBounds_ = nextBounds;
}

void Application::FinishGroupTransform() {
    groupTransformActive_ = false;
    groupStartIndices_.clear();
    groupStartShapes_.clear();
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
    ApplyPreferences(shape);
    lineStartWorld_ = startWorld;
    drawingLineTool_ = tool;
    SetLineGeometry(shape, startWorld, startWorld);
    PushHistory();
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
    transformer_.EndDrag();
}

void Application::BeginBoxDrawing(Tool tool, Vec2 startWorld) {
    Shape shape;
    shape.type = tool == Tool::Circle ? ShapeType::Circle : ShapeType::Rect;
    shape.name = std::string(tool == Tool::Circle ? "Circle " : "Rectangle ") +
        std::to_string(document_.Shapes().size() + 1);
    if (tool == Tool::Circle) {
        shape.cornerRadius = 1000.0f;
    }
    ApplyPreferences(shape);

    boxStartWorld_ = startWorld;
    drawingBoxTool_ = tool;
    shape.position = startWorld;
    shape.size = {1.0f, 1.0f};

    PushHistory();
    document_.AddShape(std::move(shape));
    isDrawingBox_ = true;
    transformer_.EndDrag();
}

void Application::UpdateBoxDrawing(Vec2 endWorld) {
    Shape *shape = document_.SelectedShape();
    if (!shape || !IsBoxTool(drawingBoxTool_)) {
        isDrawingBox_ = false;
        return;
    }

    const float left = std::min(boxStartWorld_.x, endWorld.x);
    const float right = std::max(boxStartWorld_.x, endWorld.x);
    const float top = std::min(boxStartWorld_.y, endWorld.y);
    const float bottom = std::max(boxStartWorld_.y, endWorld.y);
    shape->position = {(left + right) * 0.5f, (top + bottom) * 0.5f};
    shape->size = {std::max(1.0f, right - left), std::max(1.0f, bottom - top)};
}

void Application::FinishBoxDrawing() {
    isDrawingBox_ = false;
    transformer_.EndDrag();
}

void Application::BeginBrushStroke(Vec2 startWorld) {
    Shape shape;
    shape.type = ShapeType::Brush;
    shape.name = "Blur Brush " + std::to_string(document_.Shapes().size() + 1);
    shape.position = startWorld;
    shape.brushSize = brushToolSize_;
    ApplyPreferences(shape);
    shape.brushSize = brushToolSize_;
    shape.size = {shape.brushSize, shape.brushSize};
    shape.brushPoints.push_back({0.0f, 0.0f});

    PushHistory();
    document_.AddShape(std::move(shape));
    isDrawingBrush_ = true;
    transformer_.EndDrag();
}

void Application::UpdateBrushStroke(Vec2 world) {
    Shape *shape = document_.SelectedShape();
    if (!shape || shape->type != ShapeType::Brush) {
        isDrawingBrush_ = false;
        return;
    }

    const Vec2 local = WorldToLocal(world, *shape);
    if (!shape->brushPoints.empty() && Distance(shape->brushPoints.back(), local) < 2.0f / view_.zoom) {
        return;
    }

    shape->brushPoints.push_back(local);
}

void Application::FinishBrushStroke() {
    isDrawingBrush_ = false;
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

void Application::RenderShape(SkCanvas *canvas, const Shape &shape) const {
    if (shape.type == ShapeType::Brush) {
        return;
    }

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
        if (shape.fillEnabled) {
            canvas->drawOval(rect, fill);
        }
        if (shape.borderEnabled && shape.borderWidth > 0.0f) {
            canvas->drawOval(rect, border);
        }
    } else if (shape.type == ShapeType::Line || shape.type == ShapeType::Arrow) {
        if (!shape.borderEnabled || shape.borderWidth <= 0.0f) {
            canvas->restore();
            return;
        }
        const float halfWidth = shape.size.x * 0.5f;
        const float arrowLength = Clamp(shape.arrowHeadSize, 6.0f, 120.0f);
        const float arrowHalfHeight = arrowLength * 0.32f;
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
            if (shape.fillEnabled && !line.empty()) {
                DrawTextWithFallback(canvas, line, rect.left(), baseline, font.getSize(), fill);
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
        } else if (shape.fillEnabled) {
            if (radius > 0.0f) {
                canvas->drawRRect(SkRRect::MakeRectXY(rect, radius, radius), fill);
            } else {
                canvas->drawRect(rect, fill);
            }
        }

        if (shape.borderEnabled && shape.borderWidth > 0.0f) {
            if (radius > 0.0f) {
                canvas->drawRRect(SkRRect::MakeRectXY(rect, radius, radius), border);
            } else {
                canvas->drawRect(rect, border);
            }
        }
    }

    canvas->restore();
}

void Application::RenderSelectionArea(SkCanvas *canvas, float dpr) {
    if (!isSelectingArea_) {
        return;
    }

    const float left = std::min(selectionStartScreen_.x, selectionCurrentScreen_.x);
    const float right = std::max(selectionStartScreen_.x, selectionCurrentScreen_.x);
    const float top = std::min(selectionStartScreen_.y, selectionCurrentScreen_.y);
    const float bottom = std::max(selectionStartScreen_.y, selectionCurrentScreen_.y);
    const SkRect rect = SkRect::MakeLTRB(left * dpr, top * dpr, right * dpr, bottom * dpr);

    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setStyle(SkPaint::kFill_Style);
    fill.setColor(SkColorSetARGB(34, 120, 170, 230));
    canvas->drawRect(rect, fill);

    SkPaint stroke;
    stroke.setAntiAlias(true);
    stroke.setStyle(SkPaint::kStroke_Style);
    stroke.setStrokeWidth(1.0f * dpr);
    stroke.setColor(SkColorSetARGB(180, 150, 190, 240));
    canvas->drawRect(rect, stroke);
}

void Application::RenderBrushPreview(SkCanvas *canvas, float dpr) {
    if (activeTool_ != Tool::Brush) {
        return;
    }

    const ImGuiIO &io = ImGui::GetIO();
    const float radius = std::max(2.0f, brushToolSize_ * view_.zoom * dpr * 0.5f);
    const SkPoint center = SkPoint::Make(io.MousePos.x * dpr, io.MousePos.y * dpr);

    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setStyle(SkPaint::kFill_Style);
    fill.setColor(SkColorSetARGB(26, 245, 248, 255));
    canvas->drawCircle(center, radius, fill);

    SkPaint stroke;
    stroke.setAntiAlias(true);
    stroke.setStyle(SkPaint::kStroke_Style);
    stroke.setStrokeWidth(1.0f * dpr);
    stroke.setColor(SkColorSetARGB(165, 235, 240, 248));
    canvas->drawCircle(center, radius, stroke);
}

void Application::RenderSelectionSizeHud(SkCanvas *canvas, float logicalWidth, float logicalHeight) {
    if (document_.SelectedShapeIndices().empty()) {
        return;
    }

    const Shape bounds = SelectionBounds();
    char label[64] = {};
    std::snprintf(
        label,
        sizeof(label),
        "%d x %d",
        static_cast<int>(std::round(bounds.size.x)),
        static_cast<int>(std::round(bounds.size.y))
    );

    SkFont font(CanvasTypeface(), 12.0f);
    SkPaint textPaint;
    textPaint.setAntiAlias(true);
    textPaint.setColor(SkColorSetARGB(225, 235, 238, 244));

    SkRect textBounds;
    font.measureText(label, std::strlen(label), SkTextEncoding::kUTF8, &textBounds);

    constexpr float padX = 10.0f;
    constexpr float padY = 7.0f;
    const float width = textBounds.width() + padX * 2.0f;
    const float height = 26.0f;
    const float canvasRight = std::max(kLeftPanelWidth, logicalWidth - kRightPanelWidth);
    const float x = canvasRight - width - 18.0f;
    const float y = logicalHeight - height - 18.0f;

    SkPaint bg;
    bg.setAntiAlias(true);
    bg.setColor(SkColorSetARGB(205, 22, 24, 28));
    canvas->drawRRect(SkRRect::MakeRectXY(SkRect::MakeXYWH(x, y, width, height), 6.0f, 6.0f), bg);

    SkPaint border;
    border.setAntiAlias(true);
    border.setStyle(SkPaint::kStroke_Style);
    border.setStrokeWidth(1.0f);
    border.setColor(SkColorSetARGB(95, 220, 225, 235));
    canvas->drawRRect(SkRRect::MakeRectXY(SkRect::MakeXYWH(x, y, width, height), 6.0f, 6.0f), border);

    canvas->drawString(label, x + padX, y + 17.0f, font, textPaint);
}

void Application::RenderGroupTransformer(SkCanvas *canvas) {
    if (document_.SelectedShapeIndices().size() <= 1) {
        return;
    }

    const Shape bounds = groupTransformActive_ ? groupBounds_ : SelectionBounds();
    const auto corners = GetShapeCorners(bounds);

    SkPaint outline;
    outline.setAntiAlias(true);
    outline.setColor(SkColorSetARGB(155, 170, 185, 205));
    outline.setStyle(SkPaint::kStroke_Style);
    outline.setStrokeWidth(1.0f);

    SkPath path;
    Vec2 p = view_.WorldToScreen(corners[0]);
    path.moveTo(p.x, p.y);
    for (int i = 1; i < 4; ++i) {
        p = view_.WorldToScreen(corners[i]);
        path.lineTo(p.x, p.y);
    }
    path.close();
    canvas->drawPath(path, outline);

    SkPaint handle;
    handle.setAntiAlias(true);
    handle.setColor(SkColorSetARGB(230, 224, 232, 242));
    handle.setStyle(SkPaint::kFill_Style);

    constexpr float half = 4.0f;
    for (Vec2 corner: corners) {
        const Vec2 screen = view_.WorldToScreen(corner);
        canvas->drawRect(SkRect::MakeXYWH(screen.x - half, screen.y - half, half * 2.0f, half * 2.0f), handle);
    }
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
        if (!shapes[i].visible) {
            continue;
        }
        if (shapes[i].blurBackground) {
            canvas->restore();
            ApplyBlurShape(
                canvas,
                shapes[i],
                [&](Vec2 world) { return DevicePoint(world, view_, dpr); },
                view_.zoom * dpr
            );
            canvas->save();
            canvas->scale(dpr, dpr);
            canvas->translate(view_.pan.x, view_.pan.y);
            canvas->scale(view_.zoom, view_.zoom);
        }
        RenderShape(canvas, shapes[i]);
    }
    canvas->restore();

    if (document_.SelectedShapeIndices().size() > 1) {
        canvas->save();
        canvas->scale(dpr, dpr);
        RenderGroupTransformer(canvas);
        canvas->restore();
    } else if (const Shape *selectedShape = document_.SelectedShape()) {
        if (selectedShape->visible && !selectedShape->locked) {
            canvas->save();
            canvas->scale(dpr, dpr);
            transformer_.Draw(canvas, *selectedShape, view_);
            canvas->restore();
        }
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

    RenderSelectionArea(canvas, dpr);
    RenderBrushPreview(canvas, dpr);
    canvas->save();
    canvas->scale(dpr, dpr);
    RenderSelectionSizeHud(canvas, logicalWidth, logicalHeight);
    canvas->restore();

    skia_.EndFrame();

    RenderPanels();
    RenderToolbar();
    RenderBrushControls();
    RenderPreferencesDialog();
    RenderTextEditor();
    RenderExportDialog();
}

void Application::Shutdown() {
    if (exportPreviewTexture_ != 0) {
        glDeleteTextures(1, &exportPreviewTexture_);
        exportPreviewTexture_ = 0;
    }

    imgui_.Shutdown();
    skia_.Shutdown();

    if (grabbingCursor_) {
        glfwDestroyCursor(grabbingCursor_);
        grabbingCursor_ = nullptr;
    }
    if (grabCursor_) {
        glfwDestroyCursor(grabCursor_);
        grabCursor_ = nullptr;
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
