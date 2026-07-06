#pragma once

#include <array>
#include <string>

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

    struct ShapePreferences {
        Color fill;
        Color border;
        float borderWidth = 3.0f;
        float cornerRadius = 0.0f;
        bool blurBackground = false;
        float blurRadius = 12.0f;
        float brushSize = 52.0f;
    };

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
        std::vector<int> selectedShapes;
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
    bool isDrawingBox_ = false;
    bool isDrawingBrush_ = false;
    bool isSelectingArea_ = false;
    bool groupTransformActive_ = false;
    bool transformHistoryPushed_ = false;
    Vec2 selectionStartScreen_;
    Vec2 selectionCurrentScreen_;
    Vec2 groupStartMouseWorld_;
    Shape groupBounds_;
    Shape groupStartBounds_;
    std::vector<int> groupStartIndices_;
    std::vector<Shape> groupStartShapes_;
    Tool drawingLineTool_ = Tool::Line;
    Tool drawingBoxTool_ = Tool::Rect;
    Vec2 lineStartWorld_;
    Vec2 boxStartWorld_;
    int lastLayerSelectionIndex_ = -1;
    int editingTextIndex_ = -1;
    int textEditCursor_ = 0;
    bool focusTextEditor_ = false;
    std::array<char, 4096> textEditBuffer_{};
    bool hasCopiedShape_ = false;
    int copiedClipboardChangeCount_ = -1;
    std::vector<Shape> copiedShapes_;
    float brushToolSize_ = 52.0f;
    bool showPreferencesDialog_ = false;
    std::array<ShapePreferences, 7> shapePreferences_;
    bool showExportDialog_ = false;
    int exportWidth_ = 0;
    int exportHeight_ = 0;
    int cachedExportWidth_ = 0;
    int cachedExportHeight_ = 0;
    sk_sp<SkData> cachedExportData_;
    std::vector<DocumentSnapshot> undoStack_;
    std::vector<DocumentSnapshot> redoStack_;
    std::vector<SnapGuide> snapGuides_;

    void HandleInput();
    void HandleShortcuts();
    void Render(float dpr, int framebufferWidth, int framebufferHeight);
    void RenderPanels();
    void RenderToolbar();
    void RenderBrushControls();
    void RenderPreferencesDialog();
    void RenderTextEditor();
    void RenderExportDialog();
    void RenderGridAndRulers(SkCanvas *canvas, float logicalWidth, float logicalHeight, bool drawGrid, bool drawRulers);
    void RenderShape(SkCanvas *canvas, const Shape &shape) const;
    void RenderBlurOverlays(SkCanvas *canvas, float dpr);
    void RenderSelectionArea(SkCanvas *canvas, float dpr);
    void RenderBrushPreview(SkCanvas *canvas, float dpr);
    void RenderGroupTransformer(SkCanvas *canvas);
    void AddShapeFromTool(Tool tool);
    void ApplyPreferences(Shape &shape) const;
    void ResetDefaultPreferences();
    void LoadPreferences();
    void SavePreferences() const;
    std::string PreferencesPath() const;
    void AddImageFromClipboard();
    void CopySelection();
    void PasteSelectionOrClipboardImage();
    sk_sp<SkData> EncodeSelectionPng(int width, int height) const;
    void OpenExportDialog();
    bool SaveDataToFile(const std::string &path, sk_sp<SkData> data) const;
    DocumentSnapshot CaptureDocumentSnapshot() const;
    void RestoreDocumentSnapshot(const DocumentSnapshot &snapshot);
    void PushHistory();
    void Undo();
    void Redo();
    Shape SelectionBounds() const;
    std::vector<int> ShapesInSelectionArea() const;
    void BeginGroupTransform(DragMode mode, Vec2 mouseWorld);
    void UpdateGroupTransform(Vec2 mouseWorld, bool keepAspectRatio);
    void FinishGroupTransform();
    void BeginTextEditing(int shapeIndex);
    void FinishTextEditing();
    void InsertTextEditorNewline();
    void BeginLineDrawing(Tool tool, Vec2 startWorld);
    void UpdateLineDrawing(Vec2 endWorld);
    void FinishLineDrawing();
    void BeginBoxDrawing(Tool tool, Vec2 startWorld);
    void UpdateBoxDrawing(Vec2 endWorld);
    void FinishBoxDrawing();
    void BeginBrushStroke(Vec2 startWorld);
    void UpdateBrushStroke(Vec2 world);
    void FinishBrushStroke();
    void ApplySnapping(Shape &shape);
    bool IsSnapDisabled() const;
    Vec2 ViewCenterWorld() const;
};
