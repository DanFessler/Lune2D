import { useEffect, useRef } from "react";

/**
 * Mirrors web/index.html postRect: pushes #game-surface geometry to native via WKWebView.
 */
export function useGameRectBridge(surface: HTMLDivElement | null) {
  const rafRef = useRef<number | null>(null);

  useEffect(() => {
    if (!surface) return;

    const postRect = () => {
      const bridge = window.webkit?.messageHandlers?.gameRect;
      if (!bridge) return;

      const r = surface.getBoundingClientRect();
      const root = document.documentElement.getBoundingClientRect();
      const b = window.__sdlUiBasis;
      const docW = document.documentElement.clientWidth;
      const docH = document.documentElement.clientHeight;
      const uiw = Math.round(
        b ? Math.min(b.w, docW || b.w) : docW || root.width,
      );
      const uih = Math.round(
        b ? Math.min(b.h, docH || b.h) : docH || root.height,
      );

      bridge.postMessage({
        x: Math.round(r.left - root.left),
        y: Math.round(r.top - root.top),
        w: Math.round(r.width),
        h: Math.round(r.height),
        uiw,
        uih,
      });
    };

    const schedule = () => {
      if (rafRef.current != null) return;
      rafRef.current = requestAnimationFrame(() => {
        rafRef.current = null;
        postRect();
      });
    };

    window.addEventListener("resize", schedule);
    const ro = new ResizeObserver(schedule);
    ro.observe(surface);
    schedule();

    return () => {
      window.removeEventListener("resize", schedule);
      ro.disconnect();
      if (rafRef.current != null) {
        cancelAnimationFrame(rafRef.current);
        rafRef.current = null;
      }
    };
  }, [surface]);
}
