#include "CanvasDocument.h"

#include <string>
#include <utility>

CanvasDocument::CanvasDocument() {
    Shape rect;
    rect.name = "Rectangle 1";
    rect.type = ShapeType::Rect;
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
    rect.type = ShapeType::Rect;
    rect.name = "Rectangle " + std::to_string(shapes_.size() + 1);
    rect.position = {
        180.0f + static_cast<float>(shapes_.size()) * 32.0f,
        130.0f + static_cast<float>(shapes_.size()) * 32.0f
    };
    shapes_.push_back(rect);
    selectedShape_ = static_cast<int>(shapes_.size()) - 1;
    return selectedShape_;
}

int CanvasDocument::AddShape(Shape shape) {
    shapes_.insert(shapes_.begin(), std::move(shape));
    selectedShape_ = 0;
    return selectedShape_;
}

void CanvasDocument::RemoveSelectedShape() {
    if (selectedShape_ < 0 || selectedShape_ >= static_cast<int>(shapes_.size())) {
        return;
    }

    shapes_.erase(shapes_.begin() + selectedShape_);
    if (shapes_.empty()) {
        selectedShape_ = -1;
    } else if (selectedShape_ >= static_cast<int>(shapes_.size())) {
        selectedShape_ = static_cast<int>(shapes_.size()) - 1;
    }
}

void CanvasDocument::MoveShape(int fromIndex, int toIndex) {
    if (fromIndex < 0 || toIndex < 0 ||
        fromIndex >= static_cast<int>(shapes_.size()) ||
        toIndex >= static_cast<int>(shapes_.size()) ||
        fromIndex == toIndex) {
        return;
    }

    Shape moved = std::move(shapes_[fromIndex]);
    shapes_.erase(shapes_.begin() + fromIndex);
    shapes_.insert(shapes_.begin() + toIndex, std::move(moved));

    if (selectedShape_ == fromIndex) {
        selectedShape_ = toIndex;
    } else if (fromIndex < selectedShape_ && selectedShape_ <= toIndex) {
        --selectedShape_;
    } else if (toIndex <= selectedShape_ && selectedShape_ < fromIndex) {
        ++selectedShape_;
    }
}
