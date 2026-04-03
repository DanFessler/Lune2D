/// <reference types="vite/client" />

export {};

declare global {
  interface Window {
    __sdlUiBasis?: { w: number; h: number };
    webkit?: {
      messageHandlers?: {
        gameRect?: { postMessage: (msg: Record<string, number>) => void };
      };
    };
  }
}
