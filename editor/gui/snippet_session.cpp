/**************************************************************************/
/*  snippet_session.cpp                                                   */
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

#include "snippet_session.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "scene/gui/code_edit.h"

static constexpr char META_NAME[] = "_snippet_session";

Ref<SnippetSession> SnippetSession::get_for(CodeEdit *p_edit) {
	ERR_FAIL_NULL_V(p_edit, Ref<SnippetSession>());

	if (p_edit->has_meta(META_NAME)) {
		Ref<SnippetSession> existing = p_edit->get_meta(META_NAME);
		if (existing.is_valid()) {
			return existing;
		}
	}

	Ref<SnippetSession> session;
	session.instantiate();
	session->code_edit = p_edit;

	// The gui_input signal is emitted before the built-in handling, so the
	// session can consume Tab while active (see Control::_call_gui_input).
	p_edit->connect("gui_input", callable_mp(session.ptr(), &SnippetSession::_on_gui_input));
	p_edit->connect("text_changed", callable_mp(session.ptr(), &SnippetSession::_on_text_changed));
	p_edit->connect("caret_changed", callable_mp(session.ptr(), &SnippetSession::_on_caret_changed));

	p_edit->set_meta(META_NAME, session);
	return session;
}

void SnippetSession::begin(const Vector<SnippetTabStop> &p_stops) {
	if (code_edit == nullptr) {
		return;
	}
	stops = p_stops;
	current = -1;
	active = true;
	_apply_stop(0);
}

void SnippetSession::end() {
	active = false;
	current = -1;
	stops.clear();
	snapshot.clear();
}

bool SnippetSession::is_active() const {
	return active;
}

void SnippetSession::_apply_stop(int p_index) {
	if (p_index < 0 || p_index >= stops.size()) {
		return;
	}
	current = p_index;
	const SnippetTabStop &stop = stops[p_index];

	navigating = true;
	code_edit->deselect();
	code_edit->set_caret_line(stop.start_line);
	code_edit->set_caret_column(stop.start_col);
	if (stop.start_line != stop.end_line || stop.start_col != stop.end_col) {
		// Select the placeholder text so typing replaces it.
		code_edit->select(stop.start_line, stop.start_col, stop.end_line, stop.end_col);
	}
	navigating = false;

	_take_snapshot();
}

void SnippetSession::_take_snapshot() {
	snapshot.clear();
	const String text = code_edit->get_text();
	const PackedStringArray lines = text.split("\n");
	for (const String &line : lines) {
		snapshot.push_back(line);
	}
}

void SnippetSession::_on_gui_input(const Ref<InputEvent> &p_event) {
	if (!active) {
		return;
	}

	const Ref<InputEventKey> key = p_event;
	if (key.is_null() || !key->is_pressed()) {
		return;
	}

	if (key->get_keycode() == Key::ESCAPE) {
		code_edit->accept_event();
		end();
		return;
	}

	if (key->get_keycode() != Key::TAB) {
		return;
	}
	if (key->is_ctrl_pressed() || key->is_alt_pressed() || key->is_meta_pressed()) {
		return;
	}

	code_edit->accept_event();

	if (!key->is_shift_pressed() && current + 1 < stops.size()) {
		_apply_stop(current + 1);
	} else if (key->is_shift_pressed() && current - 1 >= 0) {
		_apply_stop(current - 1);
	} else if (!key->is_shift_pressed()) {
		// Past the last stop: finish the session where the cursor is.
		end();
	}
}

void SnippetSession::_on_text_changed() {
	if (!active || navigating) {
		return;
	}

	const String text = code_edit->get_text();
	const PackedStringArray current_lines = text.split("\n");

	// Find the first line that changed to compute shift amounts for the
	// stops after the edit.
	int first = 0;
	while (first < snapshot.size() && first < current_lines.size() && snapshot[first] == current_lines[first]) {
		first++;
	}
	if (first >= snapshot.size() && first >= current_lines.size()) {
		return;
	}

	const int line_delta = current_lines.size() - snapshot.size();
	const int col_delta = (first < snapshot.size() && first < current_lines.size())
			? current_lines[first].length() - snapshot[first].length()
			: 0;

	for (SnippetTabStop &stop : stops) {
		if (stop.start_line > first) {
			stop.start_line += line_delta;
			stop.end_line += line_delta;
		} else if (stop.start_line == first && line_delta == 0) {
			stop.start_col += col_delta;
			stop.end_col += col_delta;
		}
	}

	// The current stop grows or shrinks with the edit: follow the caret.
	if (current >= 0 && current < stops.size()) {
		stops.write[current].end_line = code_edit->get_caret_line();
		stops.write[current].end_col = code_edit->get_caret_column();
	}

	_take_snapshot();
}

void SnippetSession::_on_caret_changed() {
	if (!active || navigating) {
		return;
	}
	if (current < 0 || current >= stops.size()) {
		return;
	}

	// Leaving the current stop (mouse click, arrow keys) ends the session.
	const SnippetTabStop &stop = stops[current];
	const int line = code_edit->get_caret_line();
	const int col = code_edit->get_caret_column();
	const bool inside = (line > stop.start_line || (line == stop.start_line && col >= stop.start_col)) &&
			(line < stop.end_line || (line == stop.end_line && col <= stop.end_col));
	if (!inside) {
		end();
	}
}
