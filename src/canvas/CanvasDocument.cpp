#include "CanvasDocument.h"

CanvasDocument::CanvasDocument() {
    Shape rect;
    rect.name = "Rectangle 1";
    shapes_.push_back(rect);
    selectedShape_ = 0;
}

std::vector<Shape> &CanvasDocument::Shapes() {
    return shapes_;
}

const std::vector<Shape> &CanvasDocument::Shapes() const {
    return shapes_;
}

int CanvasDocument::SelectedShapeIndex() const {
    return selectedShape_;
}

Shape *CanvasDocument::SelectedShape() {
    if (selectedShape_ < 0 || selectedShape_ >= static_cast<int>(shapes_.size())) {
        return nullptr;
    }
    return &shapes_[selectedShape_];
}

const Shape *CanvasDocument::SelectedShape() const {
    if (selectedShape_ < 0 || selectedShape_ >= static_cast<int>(shapes_.size())) {
        return nullptr;
    }
    return &shapes_[selectedShape_];
}

void CanvasDocument::SelectShape(int index) {
    if (index < 0 || index >= static_cast<int>(shapes_.size())) {
        selectedShape_ = -1;
        return;
    }
    selectedShape_ = index;
}

int CanvasDocument::AddRectangle() {
    Shape rect;
    rect.name = "Rectangle " + std::to_string(shapes_.size() + 1);
    rect.position = {
        180.0f + static_cast<float>(shapes_.size()) * 32.0f,
        130.0f + static_cast<float>(shapes_.size()) * 32.0f
    };
    shapes_.push_back(rect);
    selectedShape_ = static_cast<int>(shapes_.size()) - 1;
    return selectedShape_;
}
