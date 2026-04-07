#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebKit/WebKit.h>
#import <objc/message.h>
#import <objc/runtime.h>
#include <stdlib.h>
#include <string>

#include <SDL3/SDL.h>

#include "webview_host.hpp"
#include "editor_state.hpp"
#include "engine/engine.hpp"
#include "engine/lua/lua_runtime.hpp"
#include "engine/scene_loader.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <cmath>
#include <nlohmann/json.hpp>

static nlohmann::json nsBridgeArgToJson(id v) {
    if (v == nil || v == [NSNull null])
        return nlohmann::json();
    CFTypeRef cf = (__bridge CFTypeRef)v;
    if (CFGetTypeID(cf) == CFBooleanGetTypeID())
        return nlohmann::json(CFBooleanGetValue((CFBooleanRef)cf) ? true : false);
    if ([v isKindOfClass:[NSNumber class]]) {
        double d = [v doubleValue];
        if (std::floor(d) == d && std::fabs(d) <= 9.0e18)
            return nlohmann::json((int64_t)d);
        return nlohmann::json(d);
    }
    if ([v isKindOfClass:[NSString class]])
        return nlohmann::json(std::string([v UTF8String]));
    if ([v isKindOfClass:[NSArray class]]) {
        nlohmann::json arr = nlohmann::json::array();
        for (id item in (NSArray*)v)
            arr.push_back(nsBridgeArgToJson(item));
        return arr;
    }
    if ([v isKindOfClass:[NSDictionary class]]) {
        nlohmann::json o    = nlohmann::json::object();
        NSDictionary* dict = (NSDictionary*)v;
        for (id key in dict) {
            if (![key isKindOfClass:[NSString class]])
                continue;
            o[std::string([(NSString*)key UTF8String])] = nsBridgeArgToJson(dict[key]);
        }
        return o;
    }
    return nlohmann::json();
}

// SDL3 property for NSWindow* (see SDL_PROP_WINDOW_COCOA_WINDOW_POINTER).
#ifndef SDL_PROP_WINDOW_COCOA_WINDOW_POINTER
#define SDL_PROP_WINDOW_COCOA_WINDOW_POINTER "SDL.window.cocoa.window"
#endif

// SDL3View registers a full-window cursor rect + mouseMoved invalidates when NSCursor != SDL's
// desired cursor. That repeatedly resets AppKit's cursor to the arrow and overrides WebKit's CSS
// cursors (pointer, text, etc.). When a WKWebView overlay exists, skip SDL's addCursorRect pass.
static IMP s_sdl_view_orig_reset_cursor_rects = nullptr;

static BOOL webview_content_view_has_wkwebviews(NSView* v) {
    if (!v)
        return NO;
    for (NSView* child in v.subviews) {
        if ([child isKindOfClass:[WKWebView class]])
            return YES;
    }
    return NO;
}

static void webview_sdl_view_reset_cursor_patched(id self, SEL _cmd) {
    typedef void (*reset_fn)(id, SEL);
    if (webview_content_view_has_wkwebviews(self)) {
        struct objc_super sup = { self, class_getSuperclass(object_getClass(self)) };
        void (*msg_super)(struct objc_super*, SEL) = (void (*)(struct objc_super*, SEL))objc_msgSendSuper;
        msg_super(&sup, _cmd);
        return;
    }
    if (s_sdl_view_orig_reset_cursor_rects)
        ((reset_fn)s_sdl_view_orig_reset_cursor_rects)(self, _cmd);
}

static void webview_install_sdl_cursor_coexistence_swizzle(NSView* sdlContentView) {
    if (!sdlContentView)
        return;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        Class cls = [sdlContentView class];
        SEL sel = @selector(resetCursorRects);
        Method m = class_getInstanceMethod(cls, sel);
        if (!m)
            return;
        s_sdl_view_orig_reset_cursor_rects = method_getImplementation(m);
        method_setImplementation(m, (IMP)webview_sdl_view_reset_cursor_patched);
    });
}

// Game viewport rect in CSS/points (top-left origin), updated from DOM layout.
// Used by Lune2DWebView to pass mouse events through to SDL in the viewport area.
static NSRect s_game_passthrough_rect = NSZeroRect;

// When a mouse button is held, route drags consistently to the surface that received the press:
// web-originated drags keep events in WKWebView even over the SDL game hole; game-originated
// clicks pass through normally. An NSEvent local monitor stamps the origin on mouseDown (before
// hitTest runs), and hitTest: clears it when all buttons are released.
static BOOL s_web_drag_active = NO;
static id   s_mouse_drag_monitor = nil;

@interface Lune2DWebView : WKWebView
@end

@implementation Lune2DWebView
// WKWebView overrides `-isFlipped` to YES: local Y matches DOM (top = 0, Y grows down).
// We must not use bottom-left math here; the previous height−y conversion never matched
// `getBoundingClientRect`, so hitTest never passed through and SDL saw no mouse events.
- (BOOL)lune2d_hitInGamePassthrough:(NSPoint)pointInSuperview {
    if (s_game_passthrough_rect.size.width <= 0 || s_game_passthrough_rect.size.height <= 0)
        return NO;
    if (!self.superview)
        return NO;
    NSPoint local = [self convertPoint:pointInSuperview fromView:self.superview];
    if (!NSPointInRect(local, self.bounds))
        return NO;

    NSRect css = s_game_passthrough_rect;
    NSRect gameLocal;
    if ([self isFlipped]) {
        gameLocal = css;
    } else {
        CGFloat H = self.bounds.size.height;
        gameLocal =
            NSMakeRect(css.origin.x, H - css.origin.y - css.size.height, css.size.width, css.size.height);
    }
    return NSPointInRect(local, gameLocal);
}

- (NSView*)hitTest:(NSPoint)point {
    if ([NSEvent pressedMouseButtons] == 0)
        s_web_drag_active = NO;

    if (s_web_drag_active)
        return [super hitTest:point];

    if ([self lune2d_hitInGamePassthrough:point])
        return nil;
    return [super hitTest:point];
}
@end

static Lune2DWebView*    s_webView = nil;
static NSWindow*         s_nsWindow = nil;
static WebViewGameRectFn s_rectCb   = nullptr;
static void*             s_rectUser = nullptr;
static bool              s_webview_visible = true;
/// Tracks NSWindow.firstResponder inside WKWebView; used to detect focus moves that skip SDL key-ups.
static bool              s_was_overlay_keyboard_focus = false;
static void              inject_sdl_ui_basis(WKWebView* webView);

static BOOL webview_overlay_has_keyboard_focus(void) {
    if (!s_webView || !s_nsWindow || !s_webview_visible)
        return NO;
    NSResponder* fr = s_nsWindow.firstResponder;
    if (!fr || ![fr isKindOfClass:[NSView class]])
        return NO;
    return [(NSView*)fr isDescendantOf:s_webView];
}

static std::string              s_lua_workspace;
static std::string              s_lua_engine_workspace;
static void (*s_on_script_reload)(void)                = nullptr;
static void (*s_on_script_set_paused)(bool paused)     = nullptr;
static void (*s_on_script_start_sim)(bool capture_editor_scene_snapshot) = nullptr;
static void (*s_behaviors_reload_fn)(void)             = nullptr;

@interface GameRectBridge : NSObject <WKScriptMessageHandler>
@end

@interface EngineScriptBridge : NSObject <WKScriptMessageHandler>
@end

static EngineScriptBridge* s_engineScriptBridge = nil;

// ── macOS menu + scene save (bridged to main loop) ───────────────────────────
static void (*s_on_quit_requested)(void)                 = nullptr;
static void (*s_on_save_scene_default)(void)               = nullptr;
static void (*s_on_save_scene_as)(const char* abs_utf8)  = nullptr;
static std::string         s_scenes_dir_for_save_panel;
static id                  s_key_cmd_q_monitor            = nil;

@interface Lune2dMenuHandler : NSObject
@end

@implementation Lune2dMenuHandler
- (void)menuQuit:(id)sender {
    (void)sender;
    if (s_on_quit_requested)
        s_on_quit_requested();
}

- (void)menuSaveScene:(id)sender {
    (void)sender;
    if (s_on_save_scene_default)
        s_on_save_scene_default();
}

- (void)menuSaveSceneAs:(id)sender {
    (void)sender;
    if (!s_on_save_scene_as)
        return;
    NSSavePanel* panel = [NSSavePanel savePanel];
    panel.title                     = @"Save Scene";
    panel.nameFieldStringValue      = @"scene.json";
    panel.canCreateDirectories      = YES;
    if (!s_scenes_dir_for_save_panel.empty()) {
        NSString* dir = [NSString stringWithUTF8String:s_scenes_dir_for_save_panel.c_str()];
        if (dir.length)
            panel.directoryURL = [NSURL fileURLWithPath:dir isDirectory:YES];
    }
    if (@available(macOS 11.0, *))
        panel.allowedContentTypes = @[ UTTypeJSON ];
    NSModalResponse resp = [panel runModal];
    if (resp != NSModalResponseOK)
        return;
    NSURL* url = panel.URL;
    if (!url || !url.path.length)
        return;
    s_on_save_scene_as([url.path UTF8String]);
}
@end

static Lune2dMenuHandler* s_menu_handler = nil;

void webview_host_set_macos_scenes_directory_for_save_panel(const char* scenes_dir_abs_utf8) {
    s_scenes_dir_for_save_panel = scenes_dir_abs_utf8 ? scenes_dir_abs_utf8 : "";
}

void webview_host_install_macos_app_menu(void (*on_quit_requested)(void),
                                         void (*on_save_scene_default)(void),
                                         void (*on_save_scene_as)(const char* absolute_path_utf8)) {
    @autoreleasepool {
        s_on_quit_requested       = on_quit_requested;
        s_on_save_scene_default   = on_save_scene_default;
        s_on_save_scene_as        = on_save_scene_as;
        if (!on_quit_requested || !on_save_scene_default || !on_save_scene_as)
            return;

        [NSApplication sharedApplication];
        if (!s_menu_handler)
            s_menu_handler = [Lune2dMenuHandler new];

        // Replace stub SDL menu so ⌘Q / standard shortcuts reach the responder chain.
        NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];

        NSString* appName = @"Lune2D";

        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [mainMenu addItem:appMenuItem];
        NSMenu* appMenu = [[NSMenu alloc] initWithTitle:appName];
        NSMenuItem* quitItem =
            [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Quit %@", appName]
                                       action:@selector(menuQuit:)
                                keyEquivalent:@"q"];
        [quitItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
        [quitItem setTarget:s_menu_handler];
        [appMenu addItem:quitItem];
        [appMenuItem setSubmenu:appMenu];

        NSMenuItem* fileMenuItem =
            [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
        [mainMenu addItem:fileMenuItem];
        NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
        NSMenuItem* saveItem =
            [[NSMenuItem alloc] initWithTitle:@"Save Scene"
                                       action:@selector(menuSaveScene:)
                                keyEquivalent:@"s"];
        [saveItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
        [saveItem setTarget:s_menu_handler];
        [fileMenu addItem:saveItem];
        NSMenuItem* saveAsItem =
            [[NSMenuItem alloc] initWithTitle:@"Save Scene As…"
                                       action:@selector(menuSaveSceneAs:)
                                keyEquivalent:@"S"];
        [saveAsItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
        [saveAsItem setTarget:s_menu_handler];
        [fileMenu addItem:saveAsItem];
        [fileMenuItem setSubmenu:fileMenu];

        [NSApp setMainMenu:mainMenu];
        [NSApp activateIgnoringOtherApps:YES];

        if (!s_key_cmd_q_monitor) {
            s_key_cmd_q_monitor = [NSEvent
                addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
                                             handler:^NSEvent*(NSEvent* event) {
                                                 NSUInteger flags =
                                                     event.modifierFlags
                                                     & NSEventModifierFlagDeviceIndependentFlagsMask;
                                                 if (flags != NSEventModifierFlagCommand)
                                                     return event;
                                                 NSString* key = event.charactersIgnoringModifiers;
                                                 if ([key caseInsensitiveCompare:@"q"] == NSOrderedSame) {
                                                     if (s_on_quit_requested)
                                                         s_on_quit_requested();
                                                     return nil;
                                                 }
                                                 return event;
                                             }];
        }
    }
}

void webview_host_set_lua_workspace(const char* lua_dir_abs_utf8) {
    s_lua_workspace = lua_dir_abs_utf8 ? lua_dir_abs_utf8 : "";
}

void webview_host_set_lua_engine_workspace(const char* engine_lua_dir_abs_utf8) {
    s_lua_engine_workspace = engine_lua_dir_abs_utf8 ? engine_lua_dir_abs_utf8 : "";
}

void webview_host_set_script_controls(void (*on_reload_request)(void),
                                      void (*on_set_paused)(bool paused),
                                      void (*on_start_sim_request)(bool capture_editor_scene_snapshot)) {
    s_on_script_reload      = on_reload_request;
    s_on_script_set_paused  = on_set_paused;
    s_on_script_start_sim   = on_start_sim_request;
}

void webview_host_set_behaviors_reload_fn(void (*reload_fn)(void)) {
    s_behaviors_reload_fn = reload_fn;
}

static BOOL project_list_dir_arg_ok(NSString* dir) {
    if (![dir isKindOfClass:[NSString class]])
        return NO;
    if ([dir containsString:@".."])
        return NO;
    if ([dir rangeOfString:@"\\"].location != NSNotFound)
        return NO;
    if ([dir hasPrefix:@"/"])
        return NO;
    return YES;
}

static NSString* string_join_path_components(NSString* root, NSString* rel) {
    if (!rel.length)
        return root;
    NSString* result = root;
    for (NSString* comp in [rel pathComponents]) {
        if ([comp isEqualToString:@"/"] || comp.length == 0)
            continue;
        result = [result stringByAppendingPathComponent:comp];
    }
    return result;
}

static BOOL path_is_under_root(NSString* fullPath, NSString* root) {
    NSString* canonR = [[root stringByResolvingSymlinksInPath] stringByStandardizingPath];
    NSString* canonF = [[fullPath stringByResolvingSymlinksInPath] stringByStandardizingPath];
    if (!canonR.length || !canonF.length)
        return NO;
    if (![canonF hasPrefix:canonR])
        return NO;
    if (canonF.length > canonR.length) {
        unichar c = [canonF characterAtIndex:canonR.length];
        return c == '/';
    }
    return YES;
}

/// Project-relative file path for read/write (non-empty). `editor/…` resolves under engine workspace.
static NSString* resolve_project_or_engine_file_path(NSString* rel, NSError** outErr) {
    if (![rel isKindOfClass:[NSString class]] || !rel.length) {
        if (outErr)
            *outErr = [NSError errorWithDomain:@"bridge" code:10
                                      userInfo:@{NSLocalizedDescriptionKey : @"Invalid path"}];
        return nil;
    }
    if ([rel containsString:@".."] || [rel rangeOfString:@"\\"].location != NSNotFound ||
        [rel hasPrefix:@"/"]) {
        if (outErr)
            *outErr = [NSError errorWithDomain:@"bridge" code:11
                                      userInfo:@{NSLocalizedDescriptionKey : @"Invalid path"}];
        return nil;
    }
    if (s_lua_workspace.empty()) {
        if (outErr)
            *outErr = [NSError errorWithDomain:@"bridge" code:12
                                      userInfo:@{NSLocalizedDescriptionKey : @"Project workspace not configured"}];
        return nil;
    }
    NSString* projRoot = [NSString stringWithUTF8String:s_lua_workspace.c_str()];
    NSString* root     = projRoot;
    if ([rel hasPrefix:@"editor/"] || [rel isEqualToString:@"editor"]) {
        if (s_lua_engine_workspace.empty()) {
            if (outErr)
                *outErr = [NSError errorWithDomain:@"bridge" code:13 userInfo:@{
                    NSLocalizedDescriptionKey : @"Engine Lua workspace not configured"
                }];
            return nil;
        }
        root = [NSString stringWithUTF8String:s_lua_engine_workspace.c_str()];
    }
    NSString* full = string_join_path_components(root, rel);
    if (!path_is_under_root(full, root)) {
        if (outErr)
            *outErr = [NSError errorWithDomain:@"bridge" code:14
                                      userInfo:@{NSLocalizedDescriptionKey : @"Path outside workspace"}];
        return nil;
    }
    return full;
}

static void script_bridge_send(NSDictionary* payload) {
    if (!s_webView || !payload)
        return;
    NSError* err = nil;
    NSData*  data = [NSJSONSerialization dataWithJSONObject:payload options:0 error:&err];
    if (!data) {
        NSLog(@"engineScript: JSON error %@", err);
        return;
    }
    NSString* json = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    NSString* js = [NSString
        stringWithFormat:@"window.__engineScriptBridge&&window.__engineScriptBridge.receive(%@);", json];
    dispatch_async(dispatch_get_main_queue(), ^{
        [s_webView evaluateJavaScript:js
                    completionHandler:^(__unused id _Nullable result, NSError* _Nullable error) {
                        if (error)
                            NSLog(@"engineScript reply: %@", error);
                    }];
    });
}

@implementation EngineScriptBridge
- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
    (void)userContentController;
    if (![message.body isKindOfClass:[NSDictionary class]]) {
        NSLog(@"engineScript: expected NSDictionary");
        return;
    }
    NSDictionary* d   = (NSDictionary*)message.body;
    NSString*     rid = d[@"requestId"];
    NSString*     op  = d[@"op"];
    if (![rid isKindOfClass:[NSString class]] || !rid.length || ![op isKindOfClass:[NSString class]])
        return;

    if ([op isEqualToString:@"listProjectDir"]) {
        if (s_lua_workspace.empty()) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Lua workspace not configured"});
            return;
        }
        NSString* dirArg = @"";
        if ([d[@"dir"] isKindOfClass:[NSString class]])
            dirArg = d[@"dir"];
        if (!project_list_dir_arg_ok(dirArg)) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Invalid dir"});
            return;
        }
        NSString* projRoot = [NSString stringWithUTF8String:s_lua_workspace.c_str()];
        if (!projRoot.length) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Lua workspace path invalid"});
            return;
        }
        NSFileManager* fm     = [NSFileManager defaultManager];
        NSError*       err    = nil;
        NSString*      listDisk;
        if (dirArg.length == 0) {
            listDisk = projRoot;
        } else if ([dirArg isEqualToString:@"editor"] || [dirArg hasPrefix:@"editor/"]) {
            if (s_lua_engine_workspace.empty()) {
                script_bridge_send(@{
                    @"requestId" : rid,
                    @"ok" : @NO,
                    @"error" : @"Engine Lua workspace not configured"
                });
                return;
            }
            NSString* eng = [NSString stringWithUTF8String:s_lua_engine_workspace.c_str()];
            listDisk      = string_join_path_components(eng, dirArg);
        } else {
            listDisk = string_join_path_components(projRoot, dirArg);
        }
        NSString* anchorRoot = projRoot;
        if ([dirArg isEqualToString:@"editor"] || [dirArg hasPrefix:@"editor/"])
            anchorRoot = [NSString stringWithUTF8String:s_lua_engine_workspace.c_str()];
        if (!path_is_under_root(listDisk, anchorRoot) && ![listDisk isEqualToString:anchorRoot]) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Path outside workspace"});
            return;
        }
        BOOL listIsDir = NO;
        if (![fm fileExistsAtPath:listDisk isDirectory:&listIsDir] || !listIsDir) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Not a directory"});
            return;
        }
        NSArray<NSString*>* names =
            [[fm contentsOfDirectoryAtPath:listDisk error:&err]
                sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        if (!names) {
            script_bridge_send(@{
                @"requestId" : rid,
                @"ok" : @NO,
                @"error" : err.localizedDescription ?: @"list failed"
            });
            return;
        }
        NSMutableArray* entries = [NSMutableArray array];
        NSISO8601DateFormatter* isoFmt = [[NSISO8601DateFormatter alloc] init];
        isoFmt.formatOptions = NSISO8601DateFormatWithInternetDateTime;
        for (NSString* name in names) {
            if ([name hasPrefix:@"."])
                continue;
            NSString* childFull = [listDisk stringByAppendingPathComponent:name];
            BOOL      isDir     = NO;
            if (![fm fileExistsAtPath:childFull isDirectory:&isDir])
                continue;
            NSString* attrsPath  = childFull;
            NSError*  attrErr    = nil;
            NSDictionary* attrs  = [fm attributesOfItemAtPath:attrsPath error:&attrErr];
            int64_t     fileSize = attrs ? [attrs fileSize] : 0;
            NSString* relPath =
                dirArg.length ? [NSString stringWithFormat:@"%@/%@", dirArg, name] : name;
            NSMutableDictionary* row = [@{
                @"name" : name,
                @"path" : relPath,
                @"isDirectory" : @(isDir),
                @"size" : @(fileSize),
            } mutableCopy];
            NSDate* mdate = [attrs fileModificationDate];
            if (mdate) {
                NSString* iso = [isoFmt stringFromDate:mdate];
                if (iso.length)
                    row[@"mtime"] = iso;
            }
            if (!isDir && [name.lowercaseString hasSuffix:@".png"]) {
                NSData* pngData = [NSData dataWithContentsOfFile:childFull options:0 error:&attrErr];
                if (pngData && pngData.length)
                    row[@"thumbnail"] = [pngData base64EncodedStringWithOptions:0];
            }
            [entries addObject:row];
        }
        if (dirArg.length == 0 && !s_lua_engine_workspace.empty()) {
            NSString* eng      = [NSString stringWithUTF8String:s_lua_engine_workspace.c_str()];
            NSString* edOnDisk = [eng stringByAppendingPathComponent:@"editor"];
            BOOL      edDirOk  = NO;
            if ([fm fileExistsAtPath:edOnDisk isDirectory:&edDirOk] && edDirOk) {
                BOOL hasEditor = NO;
                for (NSDictionary* e in entries) {
                    if ([[e[@"name"] lowercaseString] isEqualToString:@"editor"]) {
                        hasEditor = YES;
                        break;
                    }
                }
                if (!hasEditor) {
                    [entries addObject:@{
                        @"name" : @"editor",
                        @"path" : @"editor",
                        @"isDirectory" : @YES,
                        @"size" : @0,
                    }];
                }
            }
        }
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES, @"entries" : entries});
        return;
    }

    if ([op isEqualToString:@"readProjectFile"]) {
        NSString* relPath = d[@"path"];
        if (![relPath isKindOfClass:[NSString class]]) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Missing path"});
            return;
        }
        NSError*  resErr = nil;
        NSString* full   = resolve_project_or_engine_file_path(relPath, &resErr);
        if (!full) {
            script_bridge_send(@{
                @"requestId" : rid,
                @"ok" : @NO,
                @"error" : resErr.localizedDescription ?: @"Invalid path"
            });
            return;
        }
        NSString* enc = d[@"encoding"];
        if ([enc isKindOfClass:[NSString class]] && [enc isEqualToString:@"base64"]) {
            NSData* data = [NSData dataWithContentsOfFile:full options:0 error:&resErr];
            if (!data) {
                script_bridge_send(@{
                    @"requestId" : rid,
                    @"ok" : @NO,
                    @"error" : resErr.localizedDescription ?: @"read failed"
                });
                return;
            }
            NSString* b64 = [data base64EncodedStringWithOptions:0];
            script_bridge_send(
                @{@"requestId" : rid, @"ok" : @YES, @"content" : b64 ? b64 : @""});
            return;
        }
        NSError* err = nil;
        NSString* text =
            [NSString stringWithContentsOfFile:full encoding:NSUTF8StringEncoding error:&err];
        if (!text) {
            script_bridge_send(@{
                @"requestId" : rid,
                @"ok" : @NO,
                @"error" : err.localizedDescription ?: @"read failed"
            });
            return;
        }
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES, @"content" : text});
        return;
    }

    if ([op isEqualToString:@"writeProjectFile"]) {
        NSString* path    = d[@"path"];
        NSString* content = d[@"content"];
        if (![path isKindOfClass:[NSString class]] || !path.length) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Missing path"});
            return;
        }
        if (![content isKindOfClass:[NSString class]])
            content = @"";
        NSError*  resErr = nil;
        NSString* fullPath = resolve_project_or_engine_file_path(path, &resErr);
        if (!fullPath) {
            script_bridge_send(@{
                @"requestId" : rid,
                @"ok" : @NO,
                @"error" : resErr.localizedDescription ?: @"Invalid path"
            });
            return;
        }
        NSString*            parent = [fullPath stringByDeletingLastPathComponent];
        NSFileManager*       fmW    = [NSFileManager defaultManager];
        NSError*             werr   = nil;
        if (![fmW fileExistsAtPath:parent]) {
            if (![fmW createDirectoryAtPath:parent
                 withIntermediateDirectories:YES
                                  attributes:nil
                                       error:&werr]) {
                script_bridge_send(@{
                    @"requestId" : rid,
                    @"ok" : @NO,
                    @"error" : werr.localizedDescription ?: @"mkdir failed"
                });
                return;
            }
        }
        werr = nil;
        if (![content writeToFile:fullPath atomically:YES encoding:NSUTF8StringEncoding error:&werr]) {
            script_bridge_send(
                @{@"requestId" : rid, @"ok" : @NO, @"error" : werr.localizedDescription ?: @"write failed"});
            return;
        }
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"restartGame"]) {
        if (s_on_script_reload)
            s_on_script_reload();
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"setPaused"]) {
        NSNumber* pausedNum = d[@"paused"];
        bool      paused    = pausedNum ? pausedNum.boolValue : false;
        if (s_on_script_set_paused)
            s_on_script_set_paused(paused);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"startSimulation"]) {
        BOOL captureSnapshot = YES;
        id cap = d[@"captureEditorSnapshot"];
        if ([cap isKindOfClass:[NSNumber class]])
            captureSnapshot = [cap boolValue];
        if (s_on_script_start_sim)
            s_on_script_start_sim(captureSnapshot ? true : false);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"reloadBehaviors"]) {
        if (s_behaviors_reload_fn)
            s_behaviors_reload_fn();
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"listBehaviors"]) {
        auto sorted = eng_editor_behavior_dropdown_names();
        NSMutableArray* names = [NSMutableArray arrayWithCapacity:sorted.size()];
        for (const auto& b : sorted)
            [names addObject:[NSString stringWithUTF8String:b.c_str()]];
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES, @"behaviors" : names});
        return;
    }

    if ([op isEqualToString:@"editor.getSelectedEntityId"]) {
        uint32_t eid = eng_editor_selected_entity();
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES, @"result" : @(eid)});
        return;
    }

    if ([op isEqualToString:@"editor.setSelectedEntity"]) {
        NSArray* a = d[@"args"];
        uint32_t eid = 0;
        if ([a isKindOfClass:[NSArray class]] && [a count] > 0) {
            id v = a[0];
            if (v != nil && v != [NSNull null] && [v respondsToSelector:@selector(unsignedIntValue)])
                eid = (uint32_t)[v unsignedIntValue];
        }
        eng_editor_set_selected_entity(eid);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    // ── Scene mutation ops (two-way bridge) ──────────────────────────────
    NSArray* args = d[@"args"];

    if ([op isEqualToString:@"runtime.loadScene"]) {
        if (![args isKindOfClass:[NSArray class]] || [args count] < 1) {
            script_bridge_send(
                @{@"requestId" : rid, @"ok" : @NO, @"error" : @"runtime.loadScene: missing path"});
            return;
        }
        id p0 = args[0];
        if (![p0 isKindOfClass:[NSString class]] || ![p0 length]) {
            script_bridge_send(
                @{@"requestId" : rid, @"ok" : @NO, @"error" : @"runtime.loadScene: invalid path"});
            return;
        }
        NSError*  resErr = nil;
        NSString* full   = resolve_project_or_engine_file_path((NSString*)p0, &resErr);
        if (!full) {
            script_bridge_send(@{
                @"requestId" : rid,
                @"ok" : @NO,
                @"error" : resErr.localizedDescription ?: @"resolve path failed"
            });
            return;
        }
        std::string utf8 = std::string([full UTF8String]);
        g_scene.clear();
        if (!eng_load_scene(g_scene, utf8)) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"loadScene failed"});
            return;
        }
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"runtime.spawn"]) {
        NSString* name = ([args count] > 0 && [args[0] isKindOfClass:[NSString class]])
                             ? args[0] : @"Entity";
        uint32_t newId = g_scene.spawn([name UTF8String]);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES, @"result" : @(newId)});
        return;
    }

    if ([op isEqualToString:@"runtime.destroy"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        g_scene.destroy(eid);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"runtime.setName"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        const char* name = [args[1] UTF8String];
        g_scene.setName(eid, name);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"runtime.setActive"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        bool active  = [args[1] boolValue];
        g_scene.setActive(eid, active);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"runtime.setDrawOrder"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        int order    = [args[1] intValue];
        g_scene.setDrawOrder(eid, order);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"runtime.setUpdateOrder"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        int order    = [args[1] intValue];
        g_scene.setUpdateOrder(eid, order);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"runtime.addScript"]) {
        uint32_t eid     = [args[0] unsignedIntValue];
        const char* beh  = [args[1] UTF8String];
        bool ok = g_scene.addScript(eid, beh);
        script_bridge_send(ok
            ? @{@"requestId" : rid, @"ok" : @YES}
            : @{@"requestId" : rid, @"ok" : @NO, @"error" : @"addScript failed"});
        return;
    }

    if ([op isEqualToString:@"runtime.removeScript"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        int idx      = [args[1] intValue];
        bool ok = g_scene.removeScript(eid, idx);
        script_bridge_send(ok
            ? @{@"requestId" : rid, @"ok" : @YES}
            : @{@"requestId" : rid, @"ok" : @NO, @"error" : @"removeScript failed"});
        return;
    }

    if ([op isEqualToString:@"runtime.reorderScript"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        int from     = [args[1] intValue];
        int to       = [args[2] intValue];
        bool ok = g_scene.reorderScript(eid, from, to);
        script_bridge_send(ok
            ? @{@"requestId" : rid, @"ok" : @YES}
            : @{@"requestId" : rid, @"ok" : @NO, @"error" : @"reorderScript failed"});
        return;
    }

    if ([op isEqualToString:@"runtime.setParent"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        uint32_t pid = [args[1] unsignedIntValue];
        bool ok = g_scene.setParent(eid, pid);
        script_bridge_send(ok
            ? @{@"requestId" : rid, @"ok" : @YES}
            : @{@"requestId" : rid, @"ok" : @NO, @"error" : @"setParent failed (invalid or cycle)"});
        return;
    }

    if ([op isEqualToString:@"runtime.removeParent"]) {
        uint32_t eid = [args[0] unsignedIntValue];
        g_scene.removeParent(eid);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"runtime.setTransform"]) {
        uint32_t eid     = [args[0] unsignedIntValue];
        const char* fld  = [args[1] UTF8String];
        float val        = [args[2] floatValue];
        g_scene.setTransformField(eid, fld, val);
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES});
        return;
    }

    if ([op isEqualToString:@"runtime.setScriptProperty"]) {
        if (!g_eng_lua_vm) {
            script_bridge_send(
                @{@"requestId" : rid, @"ok" : @NO, @"error" : @"Lua VM not bound"});
            return;
        }
        uint32_t eid           = [args[0] unsignedIntValue];
        int      si            = [args[1] intValue];
        NSString* keyStr       = ([args count] > 2) ? args[2] : nil;
        id        val          = ([args count] > 3) ? args[3] : [NSNull null];
        bool      erase        = (val == nil || val == [NSNull null]);
        nlohmann::json jValue  = erase ? nlohmann::json() : nsBridgeArgToJson(val);
        bool ok                = eng_scene_mut_set_script_property(
            g_eng_lua_vm, eid, si, [keyStr UTF8String], erase, jValue);
        script_bridge_send(ok ? @{@"requestId" : rid, @"ok" : @YES}
                                : @{@"requestId" : rid, @"ok" : @NO, @"error" : @"setScriptProperty failed"});
        return;
    }

    script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Unknown op"});
}
@end

// WKWebView still paints its internal NSScrollView white unless these are cleared.
static void clear_opaque_subviews(NSView* v) {
    if ([v isKindOfClass:[NSScrollView class]]) {
        NSScrollView* sv = (NSScrollView*)v;
        sv.drawsBackground = NO;
        sv.backgroundColor = NSColor.clearColor;
    }
    if ([v isKindOfClass:[NSClipView class]]) {
        NSClipView* cv = (NSClipView*)v;
        cv.drawsBackground = NO;
        cv.backgroundColor = NSColor.clearColor;
    }
    for (NSView* child in v.subviews)
        clear_opaque_subviews(child);
}

static void inject_sdl_ui_basis(WKWebView* webView) {
    if (!webView)
        return;
    NSSize z = webView.bounds.size;
    NSString* js = [NSString stringWithFormat:
                             @"window.__sdlUiBasis={w:%d,h:%d};",
                             (int)lround(z.width),
                             (int)lround(z.height)];
    [webView evaluateJavaScript:js completionHandler:nil];
}

static void apply_webview_transparency(WKWebView* wv) {
    if (!wv)
        return;
    [wv setValue:@NO forKey:@"drawsBackground"];
    if (@available(macOS 12.0, *)) {
        wv.underPageBackgroundColor = NSColor.clearColor;
    }
    clear_opaque_subviews(wv);
}

@interface TransNavDelegate : NSObject <WKNavigationDelegate>
@end

@implementation TransNavDelegate
- (void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)navigation {
    (void)navigation;
    apply_webview_transparency(webView);
    inject_sdl_ui_basis(webView);
}
@end

static TransNavDelegate* s_nav = nil;

@implementation GameRectBridge
- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
    (void)userContentController;
    if (!s_rectCb || !s_webview_visible)
        return;
    if (![message.body isKindOfClass:[NSDictionary class]]) {
        NSLog(@"gameRect: expected NSDictionary, got %@", NSStringFromClass([(id)message.body class]));
        return;
    }
    NSDictionary* d = (NSDictionary*)message.body;
    NSNumber *nx = d[@"x"], *ny = d[@"y"], *nw = d[@"w"], *nh = d[@"h"];
    NSNumber *nuiw = d[@"uiw"], *nuih = d[@"uih"];
    if (!nx || !ny || !nw || !nh)
        return;
    int x = nx.intValue, y = ny.intValue, w = nw.intValue, h = nh.intValue;
    int uiw = nuiw ? nuiw.intValue : 0;
    int uih = nuih ? nuih.intValue : 0;
    if (w > 0 && h > 0) {
        s_game_passthrough_rect = NSMakeRect(x, y, w, h);
        s_rectCb(x, y, w, h, uiw, uih, s_rectUser);
    }
}
@end

void webview_host_set_game_rect_callback(WebViewGameRectFn fn, void* user) {
    s_rectCb = fn;
    s_rectUser = user;
}

bool webview_host_toggle_visibility() {
    @autoreleasepool {
        if (!s_webView)
            return false;
        s_webview_visible = !s_webview_visible;
        s_webView.hidden = s_webview_visible ? NO : YES;
        if (s_webview_visible) {
            inject_sdl_ui_basis(s_webView);
        } else {
            s_game_passthrough_rect = NSZeroRect;
            s_web_drag_active = NO;
            SDL_ResetKeyboard();
            s_was_overlay_keyboard_focus = false;
        }
        return s_webview_visible;
    }
}

bool webview_host_web_overlay_visible() {
    return s_webView != nil && s_webview_visible;
}

// Same geometry as web/index.html postRect — returned JSON survives file:// where postMessage may not.
static NSString* const kDomLayoutPollJS =
    @"(function(){var s=document.getElementById('game-surface');if(!s)return '';var r=s.getBoundingClientRect();"
     "var root=document.documentElement.getBoundingClientRect();var b=window.__sdlUiBasis||null;"
     "var docW=document.documentElement.clientWidth,docH=document.documentElement.clientHeight;"
     "var uiw=b?Math.min(b.w,docW||b.w):docW;var uih=b?Math.min(b.h,docH||b.h):docH;"
     "return JSON.stringify({x:Math.round(r.left-root.left),y:Math.round(r.top-root.top),"
     "w:Math.round(r.width),h:Math.round(r.height),uiw:Math.round(uiw),uih:Math.round(uih)});})()";

void webview_host_poll_dom_layout(void) {
    @autoreleasepool {
        if (!s_webView || !s_rectCb || !s_webview_visible)
            return;

        static BOOL inflight = NO;
        if (inflight)
            return;
        inflight = YES;

        [s_webView evaluateJavaScript:kDomLayoutPollJS
            completionHandler:^(id _Nullable result, NSError* _Nullable error) {
                inflight = NO;
                if (error) {
                    NSLog(@"poll_dom_layout: %@", error);
                    return;
                }
                if (![result isKindOfClass:[NSString class]])
                    return;
                NSString* json = (NSString*)result;
                if (json.length == 0)
                    return;
                NSData* data = [json dataUsingEncoding:NSUTF8StringEncoding];
                NSError* je = nil;
                id obj = [NSJSONSerialization JSONObjectWithData:data options:0 error:&je];
                if (![obj isKindOfClass:[NSDictionary class]])
                    return;
                NSDictionary* d = (NSDictionary*)obj;
                NSNumber *nx = d[@"x"], *ny = d[@"y"], *nw = d[@"w"], *nh = d[@"h"];
                NSNumber *nuiw = d[@"uiw"], *nuih = d[@"uih"];
                if (!nx || !ny || !nw || !nh)
                    return;
                int x = nx.intValue, y = ny.intValue, w = nw.intValue, h = nh.intValue;
                int uiw = nuiw ? nuiw.intValue : 0;
                int uih = nuih ? nuih.intValue : 0;
                if (w > 0 && h > 0) {
                    s_game_passthrough_rect = NSMakeRect(x, y, w, h);
                    s_rectCb(x, y, w, h, uiw, uih, s_rectUser);
                }
            }];
    }
}

void webview_host_get_layout_basis(int* basis_w, int* basis_h) {
    if (basis_w)
        *basis_w = 0;
    if (basis_h)
        *basis_h = 0;
    if (!s_webView)
        return;
    NSSize z = s_webView.bounds.size;
    if (basis_w)
        *basis_w = (int)lround(z.width);
    if (basis_h)
        *basis_h = (int)lround(z.height);
}

void webview_host_on_window_resized(int pixel_w, int pixel_h) {
    if (!s_webView || !s_nsWindow)
        return;
    (void)pixel_w;
    (void)pixel_h;
    NSView* content = s_nsWindow.contentView;
    if (content) {
        s_webView.frame = content.bounds;
        inject_sdl_ui_basis(s_webView);
    }
}

bool webview_host_init(SDL_Window* window, const char* web_root_utf8) {
    @autoreleasepool {
        if (!window || !web_root_utf8)
            return false;

        SDL_PropertiesID props = SDL_GetWindowProperties(window);
        void* pwin = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        if (!pwin)
            return false;

        s_nsWindow = (__bridge NSWindow*)pwin;
        // CSS cursor:* updates need mouse-moved delivery; NSWindow defaults to NO and SDL
        // does not enable this, so hover cursors (e.g. pointer) stay stuck as the arrow.
        s_nsWindow.acceptsMouseMovedEvents = YES;
        NSView* contentView = s_nsWindow.contentView;
        if (!contentView)
            return false;

        WKWebViewConfiguration* cfg = [[WKWebViewConfiguration alloc] init];
        [cfg setValue:@NO forKey:@"drawsBackground"];
        WKUserContentController* uc = cfg.userContentController;
        GameRectBridge* bridge = [GameRectBridge new];
        [uc addScriptMessageHandler:bridge name:@"gameRect"];
        s_engineScriptBridge = [EngineScriptBridge new];
        [uc addScriptMessageHandler:s_engineScriptBridge name:@"engineScript"];

        Lune2DWebView* wv =
            [[Lune2DWebView alloc] initWithFrame:contentView.bounds configuration:cfg];
        wv.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

        // Web Inspector: Safari → Develop → [Mac name] → page (macOS 13.3+).
        // Run with WEBVIEW_INSPECTABLE=1 so WKWebView is attachable (off by default).
        const char* inspEnv = getenv("WEBVIEW_INSPECTABLE");
        if (inspEnv && inspEnv[0] != '\0' && inspEnv[0] != '0' &&
            (inspEnv[0] != 'f' && inspEnv[0] != 'F')) {
            if (@available(macOS 13.3, *))
                wv.inspectable = YES;
        }

        s_nav = [TransNavDelegate new];
        wv.navigationDelegate = s_nav;

        apply_webview_transparency(wv);

        webview_install_sdl_cursor_coexistence_swizzle(contentView);
        [contentView addSubview:wv];
        s_webView = wv;
        s_webview_visible = true;

        // Local event monitor: stamp web drag origin on mouseDown BEFORE hitTest runs.
        // NSEvent local monitors fire inside [NSApp sendEvent:] before the event reaches
        // the window, so s_web_drag_active is set by the time hitTest: is called.
        if (!s_mouse_drag_monitor) {
            NSEventMask mask = NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown |
                               NSEventMaskOtherMouseDown;
            s_mouse_drag_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask
                handler:^NSEvent*(NSEvent* event) {
                    if (event.window != s_nsWindow || !s_webView || !s_webview_visible)
                        return event;
                    NSPoint windowPt = event.locationInWindow;
                    NSPoint superPt = [s_webView.superview convertPoint:windowPt fromView:nil];
                    s_web_drag_active = ![s_webView lune2d_hitInGamePassthrough:superPt];
                    return event;
                }];
        }

        // Child scroll views often appear after layout; fix again next tick.
        dispatch_async(dispatch_get_main_queue(), ^{
            apply_webview_transparency(s_webView);
            inject_sdl_ui_basis(s_webView);
        });

        NSString* root = [NSString stringWithUTF8String:web_root_utf8];
        if (!root)
            return false;
        NSURL* remote = [NSURL URLWithString:root];
        if (remote && remote.scheme &&
            ([remote.scheme caseInsensitiveCompare:@"http"] == NSOrderedSame ||
             [remote.scheme caseInsensitiveCompare:@"https"] == NSOrderedSame)) {
            [wv loadRequest:[NSURLRequest requestWithURL:remote]];
        } else {
            NSURL* base = [NSURL fileURLWithPath:root isDirectory:YES];
            NSURL* html = [base URLByAppendingPathComponent:@"index.html"];
            [wv loadFileURL:html allowingReadAccessToURL:base];
        }

        return true;
    }
}

bool webview_host_capture_composite_png(SDL_Window* window, const char* pathUtf8) {
    if (!window || !pathUtf8)
        return false;
    @autoreleasepool {
        SDL_PropertiesID props = SDL_GetWindowProperties(window);
        void* pwin = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        if (!pwin)
            return false;
        NSWindow* nsWindow = (__bridge NSWindow*)pwin;
        NSInteger wid = nsWindow.windowNumber;
        NSString* path =
            [[NSString stringWithUTF8String:pathUtf8] stringByStandardizingPath];
        if (!path.length)
            return false;

        // Prefer screencapture (full composited window, including WKWebView).
        NSTask* task = [[NSTask alloc] init];
        task.launchPath = @"/usr/sbin/screencapture";
        task.arguments =
            @[ @"-l", [NSString stringWithFormat:@"%ld", (long)wid], @"-x", path ];
        @try {
            [task launch];
            [task waitUntilExit];
            if (task.terminationStatus == 0)
                return true;
        } @catch (NSException* e) {
            (void)e;
        }

        CGImageRef img = CGWindowListCreateImage(
            CGRectNull,
            kCGWindowListOptionIncludingWindow,
            (CGWindowID)wid,
            kCGWindowImageDefault);
        if (!img)
            return false;

        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:img];
        CGImageRelease(img);
        if (!rep)
            return false;

        NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        return png && [png writeToFile:path atomically:YES];
    }
}

void webview_host_publish_entities_json(const char* json_utf8) {
    if (!json_utf8 || !s_webView)
        return;
    @autoreleasepool {
        NSString* payload = [NSString stringWithUTF8String:json_utf8];
        if (!payload.length)
            return;
        NSString* js =
            [NSString stringWithFormat:@"if(window.__engineOnEntities)window.__engineOnEntities(%@);", payload];
        [s_webView evaluateJavaScript:js
                    completionHandler:^(__unused id _Nullable result, NSError* _Nullable error) {
                        if (error)
                            NSLog(@"publish_entities: %@", error);
                    }];
    }
}

void webview_host_notify_selected_entity(uint32_t entity_id) {
    if (!s_webView)
        return;
    @autoreleasepool {
        NSString* arg = entity_id > 0 ? [NSString stringWithFormat:@"%u", (unsigned)entity_id] : @"null";
        NSString* js = [NSString stringWithFormat:
                                @"if(typeof window.__engineSelectEntity==='function')window.__engineSelectEntity(%@);",
                                arg];
        [s_webView evaluateJavaScript:js
                    completionHandler:^(__unused id _Nullable result, NSError* _Nullable error) {
                        if (error)
                            NSLog(@"notify_selected_entity: %@", error);
                    }];
    }
}

void webview_host_sync_sdl_keyboard_state(void) {
    if (!s_webView || !s_nsWindow)
        return;
    BOOL now = webview_overlay_has_keyboard_focus();
    if (now && !s_was_overlay_keyboard_focus)
        SDL_ResetKeyboard();
    s_was_overlay_keyboard_focus = now;
}

void webview_host_shutdown() {
    @autoreleasepool {
        if (s_key_cmd_q_monitor) {
            [NSEvent removeMonitor:s_key_cmd_q_monitor];
            s_key_cmd_q_monitor = nil;
        }
        if (s_mouse_drag_monitor) {
            [NSEvent removeMonitor:s_mouse_drag_monitor];
            s_mouse_drag_monitor = nil;
        }
        s_web_drag_active = NO;
        s_menu_handler = nil;

        WKWebView* wv = s_webView;
        s_webView = nil;
        if (wv) {
            wv.navigationDelegate = nil;
            WKUserContentController* uc = wv.configuration.userContentController;
            [uc removeScriptMessageHandlerForName:@"gameRect"];
            [uc removeScriptMessageHandlerForName:@"engineScript"];
            [wv removeFromSuperview];
        }
        s_engineScriptBridge = nil;
        s_nav = nil;
        s_nsWindow = nil;
        s_webview_visible = true;
    }
}
