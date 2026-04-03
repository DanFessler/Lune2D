import react from "@vitejs/plugin-react";
import type { Plugin } from "vite";
import { defineConfig } from "vite";

/** IIFE bundles must use a classic script tag; Vite still injects type="module" by default. */
function classicEntryScript(): Plugin {
  return {
    name: "classic-entry-script",
    apply: "build",
    transformIndexHtml: {
      order: "post",
      handler(html) {
        let out = html
          .replace(/\s+type="module"/g, "")
          .replace(/\s+crossorigin/g, "");
        // Blocking scripts in <head> run before <body>; #root does not exist yet. ES modules are
        // deferred by default; mirror that for this classic IIFE bundle.
        out = out.replace(/<script src=/g, "<script defer src=");
        return out;
      },
    },
  };
}

// base: relative URLs for WKWebView file:// and local opens.
// IIFE + single chunk: ES modules often do not run from file:// in desktop browsers.
export default defineConfig({
  plugins: [react(), classicEntryScript()],
  base: "./",
  build: {
    outDir: "dist",
    emptyOutDir: true,
    rollupOptions: {
      output: {
        format: "iife",
        name: "hud",
        inlineDynamicImports: true,
        entryFileNames: "assets/hud.js",
        chunkFileNames: "assets/hud.js",
        assetFileNames: "assets/[name][extname]",
      },
    },
  },
});
