/** Scene assets load via `runtime.loadScene`; other paths open in Monaco. */
export function isSceneAssetPath(relPath: string): boolean {
  const base = relPath.split("/").pop() ?? "";
  return base.endsWith(".scene.json") || base.endsWith(".scene");
}
