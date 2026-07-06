#include "CanvasDocument.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

CanvasDocument::CanvasDocument() {
    Shape rect;
    rect.name = "Rectangle 1";
    rect.type = ShapeType::Rect;
    shapes_.push_back(rect);
    selectedShape_ = 0;
    selectedShapes_ = {0};
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

const std::vector<int> &CanvasDocument::SelectedShapeIndices() const {
    return selectedShapes_;
}

bool CanvasDocument::IsShapeSelected(int index) const {
    return std::find(selectedShapes_.begin(), selectedShapes_.end(), index) != selectedShapes_.end();
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
        selectedShapes_.clear();
        return;
    }
    selectedShape_ = index;
    selectedShapes_ = {index};
}

void CanvasDocument::SelectShapes(std::vector<int> indices) {
    indices.erase(
        std::remove_if(indices.begin(), indices.end(), [&](int index) {
            return index < 0 || index >= static_cast<int>(shapes_.size());
        }),
        indices.end()
    );
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    selectedShapes_ = std::move(indices);
    selectedShape_ = selectedShapes_.empty() ? -1 : selectedShapes_.front();
}

void CanvasDocument::ReplaceContents(std::vector<Shape> shapes, std::vector<int> selectedShapes) {
    shapes_ = std::move(shapes);
    SelectShapes(std::move(selectedShapes));
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
    selectedShapes_ = {selectedShape_};
    return selectedShape_;
}

int CanvasDocument::AddShape(Shape shape) {
    shapes_.insert(shapes_.begin(), std::move(shape));
    selectedShape_ = 0;
    selectedShapes_ = {0};
    return selectedShape_;
}

void CanvasDocument::RemoveSelectedShapes() {
    if (selectedShapes_.empty()) {
        return;
    }

    std::vector<int> indices = selectedShapes_;
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    for (int index: indices) {
        if (index >= 0 && index < static_cast<int>(shapes_.size())) {
            shapes_.erase(shapes_.begin() + index);
        }
    }

    if (shapes_.empty()) {
        selectedShape_ = -1;
        selectedShapes_.clear();
    } else {
        selectedShape_ = std::min(indices.back(), static_cast<int>(shapes_.size()) - 1);
        selectedShapes_ = {selectedShape_};
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

    for (int &index: selectedShapes_) {
        if (index == fromIndex) {
            index = toIndex;
        } else if (fromIndex < index && index <= toIndex) {
            --index;
        } else if (toIndex <= index && index < fromIndex) {
            ++index;
        }
    }
    std::sort(selectedShapes_.begin(), selectedShapes_.end());
}
