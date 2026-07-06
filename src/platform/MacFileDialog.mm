#include "FileDialog.h"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

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
