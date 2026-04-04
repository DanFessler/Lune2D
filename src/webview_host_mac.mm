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
#include "engine/engine.hpp"

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

static WKWebView*        s_webView = nil;
static NSWindow*         s_nsWindow = nil;
static WebViewGameRectFn s_rectCb   = nullptr;
static void*             s_rectUser = nullptr;

static std::string              s_lua_workspace;
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

@interface AsteroidsMenuHandler : NSObject
@end

@implementation AsteroidsMenuHandler
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

static AsteroidsMenuHandler* s_menu_handler = nil;

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
            s_menu_handler = [AsteroidsMenuHandler new];

        // Replace stub SDL menu so ⌘Q / standard shortcuts reach the responder chain.
        NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];

        NSString* appName =
            [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
        if (!appName.length)
            appName = [[NSProcessInfo processInfo] processName];

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

static BOOL script_rel_path_ok(NSString* name) {
    if (![name isKindOfClass:[NSString class]] || !name.length)
        return NO;
    if ([name containsString:@".."])
        return NO;
    if ([name rangeOfString:@"\\"].location != NSNotFound)
        return NO;
    BOOL isLua  = [name hasSuffix:@".lua"];
    BOOL isJson = [name hasSuffix:@".json"];
    if (!isLua && !isJson)
        return NO;
    NSArray<NSString*>* parts = [name componentsSeparatedByString:@"/"];
    if (parts.count == 1 && isLua)
        return YES;
    if (parts.count == 2 && [parts[1] length] > 0) {
        if (isLua && ([parts[0] isEqualToString:@"game"] || [parts[0] isEqualToString:@"behaviors"]))
            return [parts[1] rangeOfString:@"/"].location == NSNotFound;
        if (isJson && [parts[0] isEqualToString:@"scenes"])
            return [parts[1] rangeOfString:@"/"].location == NSNotFound;
    }
    return NO;
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

    if ([op isEqualToString:@"listLua"]) {
        if (s_lua_workspace.empty()) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Lua workspace not configured"});
            return;
        }
        NSString* dir = [NSString stringWithUTF8String:s_lua_workspace.c_str()];
        if (!dir.length) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Lua workspace path invalid"});
            return;
        }
        NSError*             err  = nil;
        NSFileManager*       fm   = [NSFileManager defaultManager];
        NSArray<NSString*>*  names = [fm contentsOfDirectoryAtPath:dir error:&err];
        if (!names) {
            script_bridge_send(@{
                @"requestId" : rid,
                @"ok" : @NO,
                @"error" : err.localizedDescription ?: @"list failed"
            });
            return;
        }
        NSMutableArray* files = [NSMutableArray array];
        for (NSString* name in names) {
            if (![name hasSuffix:@".lua"])
                continue;
            if (!script_rel_path_ok(name))
                continue;
            NSString* fullPath = [dir stringByAppendingPathComponent:name];
            NSString* text =
                [NSString stringWithContentsOfFile:fullPath
                                            encoding:NSUTF8StringEncoding
                                               error:&err];
            if (!text) {
                script_bridge_send(@{
                    @"requestId" : rid,
                    @"ok" : @NO,
                    @"error" : err.localizedDescription ?: @"read failed"
                });
                return;
            }
            [files addObject:@{@"path" : name, @"content" : text}];
        }
        NSString*            gameDir = [dir stringByAppendingPathComponent:@"game"];
        BOOL                 isSubdir = NO;
        if ([fm fileExistsAtPath:gameDir isDirectory:&isSubdir] && isSubdir) {
            NSArray<NSString*>* gnames = [fm contentsOfDirectoryAtPath:gameDir error:&err];
            if (!gnames) {
                script_bridge_send(@{
                    @"requestId" : rid,
                    @"ok" : @NO,
                    @"error" : err.localizedDescription ?: @"list game/ failed"
                });
                return;
            }
            for (NSString* gname in gnames) {
                if (![gname hasSuffix:@".lua"])
                    continue;
                NSString* rel = [NSString stringWithFormat:@"game/%@", gname];
                if (!script_rel_path_ok(rel))
                    continue;
                NSString* gFull = [gameDir stringByAppendingPathComponent:gname];
                NSString* gText =
                    [NSString stringWithContentsOfFile:gFull
                                              encoding:NSUTF8StringEncoding
                                                 error:&err];
                if (!gText) {
                    script_bridge_send(@{
                        @"requestId" : rid,
                        @"ok" : @NO,
                        @"error" : err.localizedDescription ?: @"read failed"
                    });
                    return;
                }
                [files addObject:@{@"path" : rel, @"content" : gText}];
            }
        }
        NSString*            behDir = [dir stringByAppendingPathComponent:@"behaviors"];
        BOOL                 isBehDir = NO;
        if ([fm fileExistsAtPath:behDir isDirectory:&isBehDir] && isBehDir) {
            NSArray<NSString*>* bnames = [fm contentsOfDirectoryAtPath:behDir error:&err];
            if (!bnames) {
                script_bridge_send(@{
                    @"requestId" : rid,
                    @"ok" : @NO,
                    @"error" : err.localizedDescription ?: @"list behaviors/ failed"
                });
                return;
            }
            for (NSString* bname in bnames) {
                if (![bname hasSuffix:@".lua"])
                    continue;
                NSString* rel = [NSString stringWithFormat:@"behaviors/%@", bname];
                if (!script_rel_path_ok(rel))
                    continue;
                NSString* bFull = [behDir stringByAppendingPathComponent:bname];
                NSString* bText =
                    [NSString stringWithContentsOfFile:bFull
                                              encoding:NSUTF8StringEncoding
                                                 error:&err];
                if (!bText) {
                    script_bridge_send(@{
                        @"requestId" : rid,
                        @"ok" : @NO,
                        @"error" : err.localizedDescription ?: @"read failed"
                    });
                    return;
                }
                [files addObject:@{@"path" : rel, @"content" : bText}];
            }
        }
        NSString*            scnDir = [dir stringByAppendingPathComponent:@"scenes"];
        BOOL                 isScnDir = NO;
        if ([fm fileExistsAtPath:scnDir isDirectory:&isScnDir] && isScnDir) {
            NSArray<NSString*>* snames = [fm contentsOfDirectoryAtPath:scnDir error:&err];
            if (!snames) {
                script_bridge_send(@{
                    @"requestId" : rid,
                    @"ok" : @NO,
                    @"error" : err.localizedDescription ?: @"list scenes/ failed"
                });
                return;
            }
            for (NSString* sname in snames) {
                if (![sname hasSuffix:@".json"])
                    continue;
                NSString* rel = [NSString stringWithFormat:@"scenes/%@", sname];
                if (!script_rel_path_ok(rel))
                    continue;
                NSString* sFull = [scnDir stringByAppendingPathComponent:sname];
                NSString* sText =
                    [NSString stringWithContentsOfFile:sFull
                                              encoding:NSUTF8StringEncoding
                                                 error:&err];
                if (!sText) {
                    script_bridge_send(@{
                        @"requestId" : rid,
                        @"ok" : @NO,
                        @"error" : err.localizedDescription ?: @"read failed"
                    });
                    return;
                }
                [files addObject:@{@"path" : rel, @"content" : sText}];
            }
        }
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES, @"files" : files});
        return;
    }

    if ([op isEqualToString:@"writeLua"]) {
        NSString* path    = d[@"path"];
        NSString* content = d[@"content"];
        if (![content isKindOfClass:[NSString class]])
            content = @"";
        if (!script_rel_path_ok(path)) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Invalid path"});
            return;
        }
        if (s_lua_workspace.empty()) {
            script_bridge_send(@{@"requestId" : rid, @"ok" : @NO, @"error" : @"Lua workspace not configured"});
            return;
        }
        NSString*            dir      = [NSString stringWithUTF8String:s_lua_workspace.c_str()];
        NSString*            fullPath = [dir stringByAppendingPathComponent:path];
        NSString*            parent   = [fullPath stringByDeletingLastPathComponent];
        NSFileManager*       fmW      = [NSFileManager defaultManager];
        NSError*             werr     = nil;
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
        NSMutableArray* names = [NSMutableArray arrayWithCapacity:g_registered_behaviors.size()];
        for (const auto& b : g_registered_behaviors)
            [names addObject:[NSString stringWithUTF8String:b.c_str()]];
        script_bridge_send(@{@"requestId" : rid, @"ok" : @YES, @"behaviors" : names});
        return;
    }

    // ── Scene mutation ops (two-way bridge) ──────────────────────────────
    NSArray* args = d[@"args"];

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
    if (!s_rectCb)
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
    if (w > 0 && h > 0)
        s_rectCb(x, y, w, h, uiw, uih, s_rectUser);
}
@end

void webview_host_set_game_rect_callback(WebViewGameRectFn fn, void* user) {
    s_rectCb = fn;
    s_rectUser = user;
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
        if (!s_webView || !s_rectCb)
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
                if (w > 0 && h > 0)
                    s_rectCb(x, y, w, h, uiw, uih, s_rectUser);
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

        WKWebView* wv =
            [[WKWebView alloc] initWithFrame:contentView.bounds configuration:cfg];
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

void webview_host_shutdown() {
    @autoreleasepool {
        if (s_key_cmd_q_monitor) {
            [NSEvent removeMonitor:s_key_cmd_q_monitor];
            s_key_cmd_q_monitor = nil;
        }
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
    }
}
