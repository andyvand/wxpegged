// macOS Help Book launcher. Called from the Help menu handler.
// Uses NSHelpManager / showHelp: so the bundled Pegged.help book
// is opened in Help Viewer, keyed off CFBundleHelpBookName in the
// app's Info.plist.

// Work around a Homebrew e2fsprogs-libs conflict: /usr/local/include/uuid/uuid.h
// (shipped by Homebrew's libuuid) defines the _UUID_UUID_H guard but does
// NOT define Apple's uuid_string_t. When the macOS SDK's hfs_format.h
// (pulled in via AppKit -> CoreServices -> CarbonCore -> HFSVolumes.h)
// then does #include <uuid/uuid.h>, the guard short-circuits Apple's
// real header and uuid_string_t stays undefined, breaking the build.
//
// Pre-define the typedef ourselves so the symbol is visible regardless
// of which uuid/uuid.h the preprocessor picks up first.
#include <sys/_types.h>
#ifndef _UUID_STRING_T
#define _UUID_STRING_T
typedef __darwin_uuid_string_t uuid_string_t;
#endif

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

// NSHelpManager has -registerBooksInBundle:error: at runtime on macOS 10.6+,
// but it is not declared in the public SDK headers. Forward-declare so the
// compiler knows the signature; gate the call with respondsToSelector:.
@interface NSHelpManager (PeggedPrivate)
- (BOOL)registerBooksInBundle:(NSBundle *)bundle error:(NSError **)outError;
@end

static NSURL *LocalHelpBookAccessURL(NSBundle *bundle)
{
    NSString *folder = [bundle objectForInfoDictionaryKey:@"CFBundleHelpBookFolder"];
    if (!folder) return nil;

    NSURL *helpBundleURL = [[bundle resourceURL] URLByAppendingPathComponent:folder];
    NSBundle *helpBundle = [NSBundle bundleWithURL:helpBundleURL];
    if (!helpBundle) return nil;

    NSString *access = [helpBundle objectForInfoDictionaryKey:@"HPDBookAccessPath"];
    if (!access) access = @"index.html";

    // Access path is relative to the lproj; prefer a localized lproj, else
    // fall back to the development region, else use Resources/ directly.
    NSURL *resURL = [helpBundle resourceURL];
    NSArray *prefs = [NSBundle preferredLocalizationsFromArray:[helpBundle localizations]];
    for (NSString *loc in prefs) {
        NSURL *cand = [[resURL URLByAppendingPathComponent:
                        [loc stringByAppendingString:@".lproj"]]
                       URLByAppendingPathComponent:access];
        if ([cand checkResourceIsReachableAndReturnError:NULL]) return cand;
    }
    NSString *dev = [helpBundle objectForInfoDictionaryKey:@"CFBundleDevelopmentRegion"];
    if (dev) {
        NSURL *cand = [[resURL URLByAppendingPathComponent:
                        [dev stringByAppendingString:@".lproj"]]
                       URLByAppendingPathComponent:access];
        if ([cand checkResourceIsReachableAndReturnError:NULL]) return cand;
    }
    NSURL *flat = [resURL URLByAppendingPathComponent:access];
    return [flat checkResourceIsReachableAndReturnError:NULL] ? flat : nil;
}

extern "C" void ShowMacHelpBook(const char *anchor)
{
    @autoreleasepool {
        NSBundle *bundle = [NSBundle mainBundle];
        NSHelpManager *mgr = [NSHelpManager sharedHelpManager];

        // Force-register the help book in this app bundle. Without this,
        // helpd's launch-time scan may miss it (unsigned/ad-hoc builds, apps
        // run from a DMG/build dir, or after the bundle has been moved),
        // and showHelp: silently falls back to the Mac User Guide.
        BOOL registered = NO;
        if ([mgr respondsToSelector:@selector(registerBooksInBundle:error:)]) {
            NSError *err = nil;
            registered = [mgr registerBooksInBundle:bundle error:&err];
            if (!registered) {
                NSLog(@"Pegged: registerBooksInBundle failed: %@", err);
            }
        }

        NSString *book = [bundle objectForInfoDictionaryKey:@"CFBundleHelpBookName"];

        if (registered && book) {
            NSString *target = (anchor && *anchor)
                ? [NSString stringWithUTF8String:anchor]
                : @"pegged";
            [mgr openHelpAnchor:target inBook:book];
            return;
        }

        // Fallback: force-open the local access page in Help Viewer itself.
        // We go through /usr/bin/open -b com.apple.helpviewer so that the
        // file is opened by Help Viewer (not Safari), bypassing helpd's
        // book-registry lookup when it has failed.
        NSURL *url = LocalHelpBookAccessURL(bundle);
        if (url) {
            NSTask *task = [[NSTask alloc] init];
            task.launchPath = @"/usr/bin/open";
            task.arguments  = @[@"-b", @"com.apple.helpviewer", url.path];
            @try { [task launch]; }
            @catch (NSException *e) {
                NSLog(@"Pegged: launching Help Viewer failed: %@", e);
                if (book) [mgr openHelpAnchor:@"pegged" inBook:book];
                else [[NSApplication sharedApplication] showHelp:nil];
            }
        } else if (book) {
            [mgr openHelpAnchor:@"pegged" inBook:book];
        } else {
            [[NSApplication sharedApplication] showHelp:nil];
        }
    }
}
