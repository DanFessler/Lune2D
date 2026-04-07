import { useEffect, useRef } from "react";

const PASSTHROUGH_BLOCKING_SELECTOR = [
  '[role="dialog"]',
  '[role="menu"]',
  '[role="listbox"]',
  "[data-radix-popper-content-wrapper]",
].join(", ");

function rectsIntersect(a: DOMRect, b: DOMRect): boolean {
  return (
    a.left < b.right &&
    a.right > b.left &&
    a.top < b.bottom &&
    a.bottom > b.top
  );
}

function shouldBlockGamePassthrough(surfaceRect: DOMRect): boolean {
  const overlays = document.querySelectorAll<HTMLElement>(
    PASSTHROUGH_BLOCKING_SELECTOR,
  );
  for (const overlay of overlays) {
    const style = window.getComputedStyle(overlay);
    if (
      style.display === "none" ||
      style.visibility === "hidden" ||
      style.pointerEvents === "none"
    ) {
      continue;
    }
    const rect = overlay.getBoundingClientRect();
    if (rect.width <= 0 || rect.height <= 0) continue;
    if (rectsIntersect(surfaceRect, rect)) return true;
  }
  return false;
}

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

      if (shouldBlockGamePassthrough(r)) {
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
    const mo = new MutationObserver(schedule);
    mo.observe(document.body, {
      subtree: true,
      childList: true,
      attributes: true,
      attributeFilter: ["class", "style", "hidden", "aria-hidden", "data-state"],
    });
    schedule();

    return () => {
      window.removeEventListener("resize", schedule);
      ro.disconnect();
      mo.disconnect();
      if (rafRef.current != null) {
        cancelAnimationFrame(rafRef.current);
        rafRef.current = null;
      }
      postGameRectClear();
    };
  }, [surface]);
}
