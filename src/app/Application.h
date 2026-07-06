#pragma once

#include <array>

#include <GLFW/glfw3.h>
#include <vector>

#include "../canvas/CanvasDocument.h"
#include "../canvas/CanvasView.h"
#include "../canvas/Transformer.h"
#include "../render/ImGuiLayer.h"
#include "../render/SkiaRenderer.h"

class Application {
public:
    enum class Tool {
        Select,
        Pan,
        Rect,
        Circle,
        Line,
        Arrow,
        Text,
        Image
    };

    bool Init();
    void Run();
    void Shutdown();

private:
    enum class SnapAxis {
        Vertical,
        Horizontal
    };

    struct SnapGuide {
        SnapAxis axis;
        float position = 0.0f;
    };

    GLFWwindow *window_ = nullptr;
    GLFWcursor *defaultCursor_ = nullptr;
    GLFWcursor *handCursor_ = nullptr;
    SkiaRenderer skia_;
    ImGuiLayer imgui_;
    CanvasDocument document_;
    CanvasView view_;
    Transformer transformer_;
    Tool activeTool_ = Tool::Select;
    Tool previousTool_ = Tool::Select;
    bool spacePanActive_ = false;
    bool isPanningCanvas_ = false;
    bool isDrawingLine_ = false;
    Tool drawingLineTool_ = Tool::Line;
    Vec2 lineStartWorld_;
    int editingTextIndex_ = -1;
    int textEditCursor_ = 0;
    bool focusTextEditor_ = false;
    std::array<char, 4096> textEditBuffer_{};
    std::vector<SnapGuide> snapGuides_;

    void HandleInput();
    void Render(float dpr, int framebufferWidth, int framebufferHeight);
    void RenderPanels();
    void RenderToolbar();
    void RenderTextEditor();
    void RenderGridAndRulers(SkCanvas *canvas, float logicalWidth, float logicalHeight);
    void RenderShape(SkCanvas *canvas, const Shape &shape);
    void AddShapeFromTool(Tool tool);
    void BeginTextEditing(int shapeIndex);
    void FinishTextEditing();
    void InsertTextEditorNewline();
    void BeginLineDrawing(Tool tool, Vec2 startWorld);
    void UpdateLineDrawing(Vec2 endWorld);
    void FinishLineDrawing();
    void ApplySnapping(Shape &shape);
    bool IsSnapDisabled() const;
    Vec2 ViewCenterWorld() const;
};
