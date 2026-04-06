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
	-- Optional (see game/behavior_props): properties = defineProperties { speed = 5 },

	start = function(_self)
	end,

	update = function(_self, _dt: number)
	end,

	draw = function(_self, _totalTime: number)
	end,

	keydown = function(_self, _key: string)
	end,

	onHudPlay = function(_self)
	end,
}
`;
}

export function behaviorRelativePath(baseName: string): string {
  return `behaviors/${baseName}.lua`;
}

export function editorBehaviorRelativePath(baseName: string): string {
  return `editor/${baseName}.lua`;
}

/** Default strict Luau editor behavior paired with a runtime behavior. */
export function editorBehaviorLifecycleTemplate(behaviorName: string): string {
  return `--!strict
-- Editor behavior paired with behaviors/${behaviorName}.lua
-- Runs only during edit mode; stripped for distribution builds.

return {
\tupdateWorld = function(_dt: number)
\tend,

\tdrawWorld = function(_totalTime: number)
\tend,
}
`;
}
