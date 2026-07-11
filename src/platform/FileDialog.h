#pragma once

#include <string>

#include <skia/core/SkData.h>

std::string OpenImageFileDialog();
std::string SavePngFileDialog();
std::string OpenTCanvasFileDialog();
std::string SaveTCanvasFileDialog();
sk_sp<SkData> ReadClipboardImageData();
bool WriteClipboardImageData(sk_sp<SkData> data);
int ClipboardChangeCount();
