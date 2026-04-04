/** Valid Luau behavior module basename (matches `behaviors/<name>.lua` on disk). */
export function sanitizeBehaviorBaseName(raw: string): string | null {
  const s = raw.trim();
  if (!s) return null;
  if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(s)) return null;
  return s;
}

/** Default strict Luau behavior table returned to the engine. */
export function behaviorLifecycleTemplate(_behaviorName: string): string {
  return `--!strict

return {
	start = function(_id: number)
	end,

	update = function(_entityId: number, _dt: number)
	end,

	draw = function(_entityId: number, _totalTime: number)
	end,

	keydown = function(_entityId: number, _key: string)
	end,

	onHudPlay = function(_entityId: number)
	end,
}
`;
}

export function behaviorRelativePath(baseName: string): string {
  return `behaviors/${baseName}.lua`;
}
