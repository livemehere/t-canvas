#pragma once

#include <string>

#include <skia/core/SkData.h>

std::string OpenImageFileDialog();
sk_sp<SkData> ReadClipboardImageData();
