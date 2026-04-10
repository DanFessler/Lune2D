#pragma once

/// Milestone-based full-scene undo/redo for editor state.
///
/// Model:
/// - `eng_editor_undo_reset()` clears all history.
/// - `eng_editor_undo_save_state()` appends the current scene snapshot when called,
///   deduping against the current snapshot and truncating redo tail if needed.
/// - `eng_editor_undo_try_undo()` / `redo()` move across saved milestones.
///
/// Recording is disabled while simulation is Playing.

void eng_editor_undo_reset();

/// Save current scene as a new undo milestone. Returns true when a new snapshot was appended.
bool eng_editor_undo_save_state();

bool eng_editor_undo_try_undo();
bool eng_editor_undo_try_redo();

bool eng_editor_undo_can_undo();
bool eng_editor_undo_can_redo();
