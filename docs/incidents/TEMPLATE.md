# Incident: [short title]

**Date:** YYYY-MM-DD  
**Severity / impact:** [user-visible, editor-only, dev velocity, …]  
**Status:** resolved | mitigated | monitoring  
**Related:** [PRs, commits, issues — optional]

## Summary

[2–5 sentences: symptom, root cause, fix.]

## Timeline (optional)

- …

## What happened

[Factual narrative: reproduction, environment, relevant components.]

## Why it was hard to solve

- [Wrong lead / assumption]
- [Missing signal: logs, types, docs]
- [Cross-layer or implicit contract confusion]

## Root cause

[Precise cause—not only the symptom.]

## Resolution

[What changed; where in the codebase.]

## Mitigations for the future

- [ ] [Test, log, assertion, documentation, tooling — actionable]

## Agent / contributor documentation

[Only durable lessons for this repo. Prefer promoting 1–3 bullets to `docs/agent-pitfalls.md` with a link back here; avoid duplicating long text.]

## Refactor / architecture follow-up

_ Omit this section if no structural change is justified._

Do **not** paste the full plan here. Add a dedicated file under `docs/plans/` (`YYYYMMDD-short-slug.md` or match the incident slug), then link it:

- **Related:** … — link plan in front matter if useful.
- This section body: one short paragraph + link, e.g. “See [Plan title](../plans/YYYY-MM-DD-slug.md).”
