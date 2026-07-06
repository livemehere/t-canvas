#include "FileDialog.h"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

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

sk_sp<SkData> ReadClipboardImageData() {
    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        NSData *data = [pasteboard dataForType:NSPasteboardTypePNG];
        if (data) {
            return SkData::MakeWithCopy(data.bytes, data.length);
        }

        NSArray<NSImage *> *images = [pasteboard readObjectsForClasses:@[[NSImage class]] options:@{}];
        NSImage *image = images.firstObject;
        if (!image) {
            NSData *tiff = [pasteboard dataForType:NSPasteboardTypeTIFF];
            if (tiff) {
                image = [[NSImage alloc] initWithData:tiff];
            }
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
            return nullptr;
        }

        NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        return png ? SkData::MakeWithCopy(png.bytes, png.length) : nullptr;
    }
}
