# TCanvas

TCanvas is a lightweight macOS canvas tool for annotating screenshots and visual notes. It is built with C++, GLFW,
OpenGL, ImGui, and Skia.

The app focuses on fast markup workflows: draw shapes, highlight areas, blur sensitive regions, copy the result, and
export multiple asset sizes.

<!-- Screenshot: Add a main app screenshot here. Show the empty canvas with grid/rulers, the bottom toolbar, the layer tree on the left, and inspector on the right. -->

## Features

- Draw rectangles, circles, lines, arrows, text, images, and blur brush strokes.
- Drag-to-create drawing for rectangles, circles, lines, and arrows.
- Transform selected shapes with resize handles.
- Hold `Shift` while resizing to preserve aspect ratio.
- Hold `Shift` while drawing lines or arrows to constrain them to horizontal or vertical.
- Pan the canvas by holding `Space` and dragging.
- Zoom with the mouse wheel, or use `Option + Wheel` for finer zoom control.
- Snap to grid and nearby shape edges/centers.
- Select multiple shapes with drag selection or `Cmd + A`.
- Copy/paste shapes with `Cmd + C` / `Cmd + V`.
- Copy selected content as an image for pasting into other apps.
- Paste screenshots or images from the system clipboard.
- Export selected content as PNG.
- Export multiple size variations from one selection.
- Hide, lock, reorder, and select shapes from the layer tree.
- Configure default styles per shape type.

<!-- Screenshot: Add a focused screenshot of drawing/transforming shapes. Show a selected rectangle or arrow with transformer handles. -->

## Canvas Tools

The bottom toolbar contains the main tools:

| Tool   | Shortcut | Description                                                                    |
|--------|----------|--------------------------------------------------------------------------------|
| Select | `S`      | Select, move, resize, and transform shapes.                                    |
| Pan    | `P`      | Move around the canvas. You can also hold `Space`.                             |
| Rect   | `R`      | Drag to create a rectangle.                                                    |
| Circle | `C`      | Drag to create a circle or ellipse.                                            |
| Line   | `L`      | Drag to create a line. Hold `Shift` for straight horizontal/vertical lines.    |
| Arrow  | `A`      | Drag to create an arrow. Hold `Shift` for straight horizontal/vertical arrows. |
| Text   | `T`      | Create a text box and edit its content.                                        |
| Image  | `I`      | Import an image file as a shape.                                               |
| Brush  | `B`      | Paint blur regions.                                                            |

<!-- Screenshot: Add a toolbar screenshot here. Capture the bottom floating toolbar and the brush size popover. -->

## Layers

The left layer panel shows every shape in z-order. The top item renders above lower items.

Layer actions:

- Click a layer to select it.
- `Shift + Click` a second layer to select a range.
- Drag layers to reorder z-index.
- Toggle visibility with the visibility button.
- Toggle locking with the lock button.

Locked layers cannot be selected from the canvas. Hidden layers are excluded from rendering, selection, blur, and
export.

<!-- Screenshot: Add a layer panel screenshot here. Show several layers with visibility and lock buttons. -->

## Inspector

The right inspector edits selected shape properties.

For a single selection, you can edit:

- Name
- Position
- Size
- Rotation
- Fill color
- Stroke color
- Stroke width
- Corner radius
- Background blur
- Blur radius
- Text content for text shapes
- Brush size for brush shapes

For multiple selections, common style properties can be edited together.

<!-- Screenshot: Add an inspector screenshot here. Show a selected text shape and the Content textarea. -->

## Blur Workflow

TCanvas supports blur in two ways:

- Use the Brush tool to paint blur masks.
- Enable Background Blur on any shape to blur the content behind that shape.

Blur regions are included when copying or exporting selected content.

<!-- Screenshot: Add a before/after blur screenshot here. A screenshot with sensitive text blurred works well. -->

## Clipboard

Clipboard behavior is designed for screenshot annotation workflows:

- `Cmd + C` copies selected shapes internally.
- The same `Cmd + C` also writes a PNG image to the system clipboard.
- Paste into Slack, Notes, Figma, or other apps to use the rendered image.
- If the system clipboard contains a screenshot/image, `Cmd + V` imports it as an image shape.
- If the clipboard still belongs to TCanvas, `Cmd + V` pastes duplicated shapes.

The bottom-right selection HUD shows the selected export/copy size as `width x height`.

<!-- Screenshot: Add a screenshot of the selection size HUD in the bottom-right corner. -->

## Export

Open Export from the toolbar to export the current selection as PNG.

Export supports:

- Custom width and height.
- Live PNG preview.
- Copy to clipboard.
- Save to file.
- Multiple size variations.

When multiple variations are added, TCanvas saves files with size suffixes, for example:

```text
tc_export_800x600.png
tc_export_1600x1200.png
```

<!-- Screenshot: Add an export dialog screenshot here. Show two or three size variations and the preview pane. -->

## Preferences

Open Settings from the toolbar to configure default style presets per shape type.

Preferences include:

- Fill color
- Stroke color
- Stroke width
- Corner radius
- Background blur
- Blur radius
- Brush size

Settings are stored locally at:

```text
~/Library/Application Support/TCanvas/preferences.ini
```

<!-- Screenshot: Add a settings dialog screenshot here. Show shape defaults and the preview cards. -->
