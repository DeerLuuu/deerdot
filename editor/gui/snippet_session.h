/**************************************************************************/
/*  snippet_session.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "editor/gui/snippet_parser.h"

class CodeEdit;
class InputEvent;

// A Tab-navigation session over the tab stops of an inserted snippet,
// attached to any CodeEdit by composition (no subclassing). While active:
//
//   - Tab / Shift+Tab move between the tab stops in order of appearance;
//     moving past the last stop ends the session where the cursor is.
//   - Arriving at a stop selects its placeholder text, so typing replaces
//     it; zero-width stops just place the cursor.
//   - Editing inside the current stop keeps the remaining stop positions
//     in sync (stops after the edit are shifted).
//   - Escape, moving the caret out of the current stop, undoing or redoing
//     text, or ending the text cancels the session.
//
// Sessions are obtained through get_for(), which stores them as metadata
// on the CodeEdit, so any consumer (snippets, script templates, quick
// fixes, other language modules) can drive one without owning it.
class SnippetSession : public RefCounted {
	GDCLASS(SnippetSession, RefCounted);

	struct PendingSnippet {
		String text;
		Vector<SnippetTabStop> stops;
	};

	CodeEdit *code_edit = nullptr;
	bool active = false;
	int current = -1;
	Vector<SnippetTabStop> stops;
	Vector<String> snapshot; // Line snapshot used to track edit offsets.
	Vector<PendingSnippet> pending; // Armed by completion, consumed on insert.
	// Text version the stops were computed for. Text operations move it
	// forward, undo moves it back: a version below this one means the text was
	// undone past the session, so the stops no longer describe the document.
	uint32_t last_version = 0;
	bool navigating = false;

	void _on_gui_input(const Ref<InputEvent> &p_event);
	void _on_text_changed();
	void _on_text_set();
	void _on_caret_changed();

	void _apply_stop(int p_index);
	void _take_snapshot();
	bool _try_begin_inserted(const PendingSnippet &p_candidate);

public:
	static Ref<SnippetSession> get_for(CodeEdit *p_edit);

	// Starts navigating p_stops (positions must refer to the text currently
	// in the editor). Jumps to the first stop.
	void begin(const Vector<SnippetTabStop> &p_stops);

	// Arms a candidate snippet: when the text right before the caret becomes
	// p_expanded_text (i.e. the completion option carrying it was accepted),
	// a session starts automatically. Used by completion flows; several
	// candidates can be armed at once.
	void arm(const String &p_expanded_text, const Vector<SnippetTabStop> &p_stops);

	// Tab stops travel through CompletionOption.default_value.
	static Variant encode_stops(const Vector<SnippetTabStop> &p_stops);
	static Vector<SnippetTabStop> decode_stops(const Variant &p_value);

	// Ends the session, leaving the caret where it is.
	void end();

	bool is_active() const;

	SnippetSession() {}
};
