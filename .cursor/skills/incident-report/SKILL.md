---
name: incident-report
description: >-
  Use when wrapping up a non-obvious or hard-to-debug bug, when the user asks
  for a postmortem or incident report, or when documenting debugging lessons for
  future agents. Guides creation of docs/incidents files, optional
  docs/agent-pitfalls updates, and a separate docs/plans refactor file (not
  implementation) when architecture should change.
---

# Incident report (hard bugs)

## When to use this skill

Apply when **at least one** of these is true:

- Multiple wrong leads or layers (native / web / Luau / IPC) before root cause
- Misleading errors, flakiness, or “works until it doesn’t”
- Fix required non-local reasoning or risked regressions
- User explicitly asks for postmortem, incident doc, or “document this bug”

Skip for trivial typos or obvious one-file mistakes unless the user wants a record.

## Outputs (in order)

1. **New incident file** under `docs/incidents/` using `TEMPLATE.md` (copy structure; fill every section that applies; use `YYYY-MM-DD-short-slug.md` for the filename).
2. **Optional: `docs/agent-pitfalls.md`** — Add **at most a few** durable bullets that prevent the next wrong turn. Each bullet should **link to the incident file**. Do not paste long narratives into `agent-pitfalls.md`.
3. **Refactor (if warranted)** — If the incident exposes a structural problem, create a **separate plan file** under `docs/plans/` (`YYYY-MM-DD-short-slug-planned-title.md`). In the incident, add **“Refactor / architecture follow-up”** with only a **link** to that plan (and optionally list the plan in **Related** in the incident header). Do **not** implement the plan as part of this skill unless the user separately asks to execute it.

## Incident file contents

Follow `docs/incidents/TEMPLATE.md` closely. Priorities:

- **Why it was hard** — Wrong hypotheses, missing signals, confusing boundaries.
- **Mitigations** — Tests, logging, assertions, docs, codegen, CI—concrete and scoped.
- **Agent documentation** — Only facts that should change how future agents work here; redundant with long narrative should be avoided.

## Refactor plan (deliverable format)

When a refactor is **necessary or strongly advisable**, put the full executable **plan** (not code) in **`docs/plans/<filename>.plan.md`**. Link back to the triggering incident at the top of the plan (relative path to `docs/incidents/…`). The plan file should include:

| Block                    | Content                                                            |
| ------------------------ | ------------------------------------------------------------------ |
| **Goal**                 | What improves (coupling, clarity, failure modes, testability).     |
| **Non-goals**            | What this plan explicitly will not do.                             |
| **Proposed shape**       | Target modules/boundaries or data flow in plain language.          |
| **Plan (ordered steps)** | Incremental steps (each verifiable—build/tests).                   |
| **Risks / regressions**  | What could break; how to detect it.                                |
| **Rollout**              | Feature flags, phased migration, or “big bang” with justification. |

If the change is policy-level or cross-cutting, note in the plan whether **`docs/adr/`** (or repo ADR convention) should gain an entry and **one sentence** on the decision being recorded.

In the **incident** file only: a short **“Refactor / architecture follow-up”** section that links to `../plans/…` (no duplicate long plan body in the incident).

Do **not** conflate “nice cleanup” with “architecture opportunity”—only add a plan file when the incident supports it.

## After writing

- Ensure the incident and plan filenames are unique and dated; the plan links to the incident and vice versa.
- If `docs/agent-pitfalls.md` was updated, keep bullets **short** and **linked**.
