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
    Shape *SelectedShape();
    const Shape *SelectedShape() const;
    void SelectShape(int index);
    int AddRectangle();

private:
    std::vector<Shape> shapes_;
    int selectedShape_ = -1;
};
