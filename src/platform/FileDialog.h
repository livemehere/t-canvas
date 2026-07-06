#pragma once

#include <string>

#include <skia/core/SkData.h>

std::string OpenImageFileDialog();
std::string SavePngFileDialog();
sk_sp<SkData> ReadClipboardImageData();
bool WriteClipboardImageData(sk_sp<SkData> data);
int ClipboardChangeCount();
