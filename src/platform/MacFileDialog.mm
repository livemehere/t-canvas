#include "FileDialog.h"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <cmath>

#include <skia/core/SkData.h>

std::string OpenImageFileDialog() {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        if (@available(macOS 11.0, *)) {
            panel.allowedContentTypes = @[
                [UTType typeWithFilenameExtension:@"png"],
                [UTType typeWithFilenameExtension:@"jpg"],
                [UTType typeWithFilenameExtension:@"jpeg"],
                [UTType typeWithFilenameExtension:@"webp"],
                [UTType typeWithFilenameExtension:@"bmp"],
                [UTType typeWithFilenameExtension:@"gif"]
            ];
        } else {
            panel.allowedFileTypes = @[@"png", @"jpg", @"jpeg", @"webp", @"bmp", @"gif"];
        }

        if ([panel runModal] != NSModalResponseOK) {
            return {};
        }

        NSString *path = panel.URL.path;
        return path ? std::string(path.UTF8String) : std::string();
    }
}

std::string SavePngFileDialog() {
    @autoreleasepool {
        NSSavePanel *panel = [NSSavePanel savePanel];
        panel.canCreateDirectories = YES;
        panel.nameFieldStringValue = @"tc-export.png";
        if (@available(macOS 11.0, *)) {
            panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"png"]];
        } else {
            panel.allowedFileTypes = @[@"png"];
        }

        if ([panel runModal] != NSModalResponseOK) {
            return {};
        }

        NSString *path = panel.URL.path;
        return path ? std::string(path.UTF8String) : std::string();
    }
}

std::string OpenTCanvasFileDialog() {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        if (@available(macOS 11.0, *)) {
            panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"tcanvas"]];
        } else {
            panel.allowedFileTypes = @[@"tcanvas"];
        }
        if ([panel runModal] != NSModalResponseOK) {
            return {};
        }
        NSString *path = panel.URL.path;
        return path ? std::string(path.UTF8String) : std::string();
    }
}

std::string SaveTCanvasFileDialog() {
    @autoreleasepool {
        NSSavePanel *panel = [NSSavePanel savePanel];
        panel.canCreateDirectories = YES;
        panel.nameFieldStringValue = @"Untitled.tcanvas";
        if (@available(macOS 11.0, *)) {
            panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"tcanvas"]];
        } else {
            panel.allowedFileTypes = @[@"tcanvas"];
        }
        if ([panel runModal] != NSModalResponseOK) {
            return {};
        }
        NSString *path = panel.URL.path;
        return path ? std::string(path.UTF8String) : std::string();
    }
}

sk_sp<SkData> ReadClipboardImageData() {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

        NSArray<NSPasteboardType> *encodedTypes = @[
            NSPasteboardTypePNG,
            NSPasteboardTypeTIFF,
            @"public.png",
            @"public.tiff",
            @"com.apple.tiff"
        ];
        NSPasteboardType availableType = [pasteboard availableTypeFromArray:encodedTypes];
        if (availableType) {
            NSData *encoded = [pasteboard dataForType:availableType];
            if (encoded && [availableType isEqualToString:NSPasteboardTypePNG]) {
                return SkData::MakeWithCopy(encoded.bytes, encoded.length);
            }
            if (encoded) {
                NSImage *encodedImage = [[NSImage alloc] initWithData:encoded];
                if (encodedImage) {
                    NSBitmapImageRep *bitmap = [NSBitmapImageRep imageRepWithData:encodedImage.TIFFRepresentation];
                    NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
                    if (png) {
                        return SkData::MakeWithCopy(png.bytes, png.length);
                    }
                }
            }
        }

        NSArray<NSImage *> *images = [pasteboard readObjectsForClasses:@[[NSImage class]] options:@{}];
        NSImage *image = images.firstObject;
        if (!image) {
            image = [[NSImage alloc] initWithPasteboard:pasteboard];
        }
        if (!image) {
            return nullptr;
        }

        NSBitmapImageRep *bitmap = nil;
        for (NSImageRep *rep in image.representations) {
            if ([rep isKindOfClass:[NSBitmapImageRep class]]) {
                bitmap = static_cast<NSBitmapImageRep *>(rep);
                break;
            }
        }
        if (!bitmap && image.TIFFRepresentation) {
            bitmap = [NSBitmapImageRep imageRepWithData:image.TIFFRepresentation];
        }
        if (!bitmap) {
            NSSize size = image.size;
            if (size.width <= 0.0 || size.height <= 0.0) {
                return nullptr;
            }
            bitmap = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:nullptr
                              pixelsWide:static_cast<NSInteger>(std::ceil(size.width))
                              pixelsHigh:static_cast<NSInteger>(std::ceil(size.height))
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSCalibratedRGBColorSpace
                             bytesPerRow:0
                            bitsPerPixel:0];
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap]];
            [image drawInRect:NSMakeRect(0.0, 0.0, size.width, size.height)];
            [NSGraphicsContext restoreGraphicsState];
        }

        NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        return png ? SkData::MakeWithCopy(png.bytes, png.length) : nullptr;
    }
}

bool WriteClipboardImageData(sk_sp<SkData> data) {
    if (!data) {
        return false;
    }

    @autoreleasepool {
        NSData *png = [NSData dataWithBytes:data->data() length:data->size()];
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        return [pasteboard setData:png forType:NSPasteboardTypePNG] == YES;
    }
}

int ClipboardChangeCount() {
    @autoreleasepool {
        return static_cast<int>([NSPasteboard generalPasteboard].changeCount);
    }
}
