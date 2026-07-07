# TCanvas

[한국어](./README.ko.md)

TCanvas is a lightweight macOS canvas tool for annotating screenshots and visual notes. It is built with C++, GLFW,
OpenGL, ImGui, and Skia.

The app focuses on fast markup workflows: draw shapes, highlight areas, blur sensitive regions, copy the result, and
export multiple asset sizes.

<img width="1555" height="909" alt="image" src="https://github.com/user-attachments/assets/4961d174-f1ae-4b58-8549-757857cfde38" />

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
- Select every visible shape including locked shapes with `Cmd + Shift + A`.
- Copy/paste shapes with `Cmd + C` / `Cmd + V`.
- Copy selected content as an image for pasting into other apps.
- Paste screenshots or images from the system clipboard.
- Export selected content as PNG.
- Export multiple size variations from one selection.
- Hide, lock, reorder, and select shapes from the layer tree.
- Configure default styles per shape type.

<img width="1556" height="906" alt="image" src="https://github.com/user-attachments/assets/d97ea751-e3fe-44aa-8b13-2898a4573ea4" />

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

<img width="941" height="177" alt="image" src="https://github.com/user-attachments/assets/3727ff81-f79e-4df0-a830-465aebc5fb54" />

## Layers

The left layer panel shows every shape in z-order. The top item renders above lower items.

Layer actions:

- Click a layer to select it.
- `Shift + Click` a second layer to select a range.
- Drag layers to reorder z-index.
- Press `]` to move selected layers one step toward the front.
- Press `[` to move selected layers one step toward the back.
- Press `Shift + ]` to move selected layers to the front.
- Press `Shift + [` to move selected layers to the back.
- Toggle selected layer visibility with `V` or the visibility button.
- Toggle selected layer locking with `K` or the lock button.

Locked layers cannot be selected from the canvas. Hidden layers are excluded from rendering, selection, blur, and
export.

<img width="622" height="687" alt="image" src="https://github.com/user-attachments/assets/310696fe-68f1-42f3-91ee-d7539a31061c" />

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

<img width="2230" height="1402" alt="image" src="https://github.com/user-attachments/assets/36a49940-3c3c-453e-8f92-0e8e827578bb" />

## Blur Workflow

TCanvas supports blur in two ways:

- Use the Brush tool to paint blur masks.
- Enable Background Blur on any shape to blur the content behind that shape.

Blur regions are included when copying or exporting selected content.

<img width="703" height="457" alt="image" src="https://github.com/user-attachments/assets/25b0c177-7243-4009-b13b-7799f7e2c179" />

## Clipboard

Clipboard behavior is designed for screenshot annotation workflows:

- `Cmd + C` copies selected shapes internally.
- The same `Cmd + C` also writes a PNG image to the system clipboard.
- Paste into Slack, Notes, Figma, or other apps to use the rendered image.
- If the system clipboard contains a screenshot/image, `Cmd + V` imports it as an image shape.
- If the clipboard still belongs to TCanvas, `Cmd + V` pastes duplicated shapes.

The bottom-right selection HUD shows the selected export/copy size as `width x height`.

<img width="2058" height="1440" alt="image" src="https://github.com/user-attachments/assets/7ab5b822-209f-4bcf-a34f-dcd7dfc96d8a" />

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

<img width="3104" height="1634" alt="image" src="https://github.com/user-attachments/assets/ffb9e515-5438-4d7c-901a-0f0087920da9" />

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

<img width="960" height="759" alt="image" src="https://github.com/user-attachments/assets/eba80097-f9f1-4b57-afd4-1752f86f6f42" />
