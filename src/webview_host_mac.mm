#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <WebKit/WebKit.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "webview_host.hpp"

// SDL3 property for NSWindow* (see SDL_PROP_WINDOW_COCOA_WINDOW_POINTER).
#ifndef SDL_PROP_WINDOW_COCOA_WINDOW_POINTER
#define SDL_PROP_WINDOW_COCOA_WINDOW_POINTER "SDL.window.cocoa.window"
#endif

static WKWebView* s_webView = nil;
static NSWindow* s_nsWindow = nil;
static WebViewGameRectFn s_rectCb = nullptr;
static void* s_rectUser = nullptr;

@interface GameRectBridge : NSObject <WKScriptMessageHandler>
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
        NSView* contentView = s_nsWindow.contentView;
        if (!contentView)
            return false;

        WKWebViewConfiguration* cfg = [[WKWebViewConfiguration alloc] init];
        [cfg setValue:@NO forKey:@"drawsBackground"];
        WKUserContentController* uc = cfg.userContentController;
        GameRectBridge* bridge = [GameRectBridge new];
        [uc addScriptMessageHandler:bridge name:@"gameRect"];

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
        NSURL* base = [NSURL fileURLWithPath:root isDirectory:YES];
        NSURL* html = [base URLByAppendingPathComponent:@"index.html"];
        [wv loadFileURL:html allowingReadAccessToURL:base];

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

void webview_host_shutdown() {
    @autoreleasepool {
        WKWebView* wv = s_webView;
        s_webView = nil;
        if (wv) {
            wv.navigationDelegate = nil;
            [wv.configuration.userContentController removeScriptMessageHandlerForName:@"gameRect"];
            [wv removeFromSuperview];
        }
        s_nav = nil;
        s_nsWindow = nil;
    }
}
