export type EngineEntity = {
  id: string;
  name: string;
  x: number;
  y: number;
  angle: number;
  vx: number;
  vy: number;
};

declare global {
  interface Window {
    __engineOnEntities?: (payload: EngineEntity[]) => void;
  }
}

/** Installs `window.__engineOnEntities` and returns an unsubscribe. */
export function installEngineEntityBridge(
  onEntities: (list: EngineEntity[]) => void,
): () => void {
  const handler = (payload: EngineEntity[]) => {
    if (Array.isArray(payload)) onEntities(payload);
  };
  window.__engineOnEntities = handler;
  return () => {
    if (window.__engineOnEntities === handler) {
      delete window.__engineOnEntities;
    }
  };
}
