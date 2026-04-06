# Incident reports

Use this folder for **non-obvious bugs** that were expensive to debug: wrong leads, cross-layer confusion, misleading errors, or high regression risk.

## How to add one

1. Copy `TEMPLATE.md` into a new file: `YYYY-MM-DD-short-slug.md` (example: `2026-04-06-bridge-message-race.md`).
2. Fill every section that applies. Keep **Summary** and **Root cause** accurate for someone reading in six months.
3. If future agents should avoid a specific mistake, add **1–3 short bullets** to `docs/agent-pitfalls.md` linking to this file—do not paste the full report there.

## Skill

Cursor agents can follow `.cursor/skills/incident-report/SKILL.md` when wrapping up a hard bug or when you ask for a postmortem.

## Refactor plans

If the incident implies a **structural** improvement, add a file under **`docs/plans/`** (`YYYY-MM-DD-short-slug.md`) with goals, steps, risks, and rollout. In the incident, **Refactor / architecture follow-up** should only **link** to that plan. Implementation is a separate task unless explicitly requested.
