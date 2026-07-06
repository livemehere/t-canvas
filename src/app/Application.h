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
        Image,
        Brush
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

    struct DocumentSnapshot {
        std::vector<Shape> shapes;
        int selectedShape = -1;
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
    bool isDrawingBrush_ = false;
    bool transformHistoryPushed_ = false;
    Tool drawingLineTool_ = Tool::Line;
    Vec2 lineStartWorld_;
    int editingTextIndex_ = -1;
    int textEditCursor_ = 0;
    bool focusTextEditor_ = false;
    std::array<char, 4096> textEditBuffer_{};
    bool hasCopiedShape_ = false;
    Shape copiedShape_;
    std::vector<DocumentSnapshot> undoStack_;
    std::vector<DocumentSnapshot> redoStack_;
    std::vector<SnapGuide> snapGuides_;

    void HandleInput();
    void HandleShortcuts();
    void Render(float dpr, int framebufferWidth, int framebufferHeight);
    void RenderPanels();
    void RenderToolbar();
    void RenderTextEditor();
    void RenderGridAndRulers(SkCanvas *canvas, float logicalWidth, float logicalHeight, bool drawGrid, bool drawRulers);
    void RenderShape(SkCanvas *canvas, const Shape &shape);
    void RenderBlurOverlays(SkCanvas *canvas, float dpr);
    void AddShapeFromTool(Tool tool);
    void AddImageFromClipboard();
    void CopySelection();
    void PasteSelectionOrClipboardImage();
    DocumentSnapshot CaptureDocumentSnapshot() const;
    void RestoreDocumentSnapshot(const DocumentSnapshot &snapshot);
    void PushHistory();
    void Undo();
    void Redo();
    void BeginTextEditing(int shapeIndex);
    void FinishTextEditing();
    void InsertTextEditorNewline();
    void BeginLineDrawing(Tool tool, Vec2 startWorld);
    void UpdateLineDrawing(Vec2 endWorld);
    void FinishLineDrawing();
    void BeginBrushStroke(Vec2 startWorld);
    void UpdateBrushStroke(Vec2 world);
    void FinishBrushStroke();
    void ApplySnapping(Shape &shape);
    bool IsSnapDisabled() const;
    Vec2 ViewCenterWorld() const;
};
