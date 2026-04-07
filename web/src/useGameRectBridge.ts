import { useEffect, useRef } from "react";

function uiSpaceDimensions(): { uiw: number; uih: number } {
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
  return { uiw, uih };
}

/** Tell native there is no game viewport hole (passthrough / SDL inset). */
function postGameRectClear() {
  const bridge = window.webkit?.messageHandlers?.gameRect;
  if (!bridge) return;
  const { uiw, uih } = uiSpaceDimensions();
  bridge.postMessage({ x: 0, y: 0, w: 0, h: 0, uiw, uih });
}

/**
 * Mirrors web/index.html postRect: pushes #game-surface geometry to native via WKWebView.
 * When the surface unmounts or is null, posts a zero rect so hit-testing does not keep the old hole.
 */
export function useGameRectBridge(surface: HTMLDivElement | null) {
  const rafRef = useRef<number | null>(null);

  useEffect(() => {
    if (!surface) {
      postGameRectClear();
      return;
    }

    const postRect = () => {
      const bridge = window.webkit?.messageHandlers?.gameRect;
      if (!bridge) return;

      const r = surface.getBoundingClientRect();
      const w = Math.round(r.width);
      const h = Math.round(r.height);
      const { uiw, uih } = uiSpaceDimensions();

      if (w <= 0 || h <= 0) {
        bridge.postMessage({ x: 0, y: 0, w: 0, h: 0, uiw, uih });
        return;
      }

      const root = document.documentElement.getBoundingClientRect();
      bridge.postMessage({
        x: Math.round(r.left - root.left),
        y: Math.round(r.top - root.top),
        w,
        h,
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
      postGameRectClear();
    };
  }, [surface]);
}
