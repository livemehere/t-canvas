#pragma once

#include <cstddef>
#include <vector>

#include "Shape.h"

class CanvasDocument {
public:
    CanvasDocument();

    std::vector<Shape> &Shapes();
    const std::vector<Shape> &Shapes() const;

    int SelectedShapeIndex() const;
    const std::vector<int> &SelectedShapeIndices() const;
    bool IsShapeSelected(int index) const;
    Shape *SelectedShape();
    const Shape *SelectedShape() const;
    void SelectShape(int index);
    void SelectShapes(std::vector<int> indices);
    void ReplaceContents(std::vector<Shape> shapes, std::vector<int> selectedShapes);
    int AddRectangle();
    int AddShape(Shape shape);
    void RemoveSelectedShapes();
    void MoveShape(int fromIndex, int toIndex);

private:
    std::vector<Shape> shapes_;
    int selectedShape_ = -1;
    std::vector<int> selectedShapes_;
};
