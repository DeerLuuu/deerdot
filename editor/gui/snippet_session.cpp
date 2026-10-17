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
	p_edit->connect("text_set", callable_mp(session.ptr(), &SnippetSession::_on_text_set));
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
	last_version = code_edit->get_version();
	_apply_stop(0);
}

void SnippetSession::arm(const String &p_expanded_text, const Vector<SnippetTabStop> &p_stops) {
	if (p_stops.is_empty()) {
		return;
	}
	PendingSnippet candidate;
	candidate.text = p_expanded_text;
	candidate.stops = p_stops;
	if (pending.size() >= 8) {
		pending.remove_at(0);
	}
	pending.push_back(candidate);
}

Variant SnippetSession::encode_stops(const Vector<SnippetTabStop> &p_stops) {
	Array encoded;
	for (const SnippetTabStop &stop : p_stops) {
		Array item;
		item.push_back(stop.number);
		item.push_back(stop.start_line);
		item.push_back(stop.start_col);
		item.push_back(stop.end_line);
		item.push_back(stop.end_col);
		encoded.push_back(item);
	}
	return encoded;
}

Vector<SnippetTabStop> SnippetSession::decode_stops(const Variant &p_value) {
	Vector<SnippetTabStop> decoded;
	if (p_value.get_type() != Variant::ARRAY) {
		return decoded;
	}
	const Array encoded = p_value;
	for (int i = 0; i < encoded.size(); i++) {
		if (encoded[i].get_type() != Variant::ARRAY) {
			continue;
		}
		const Array item = encoded[i];
		if (item.size() != 5) {
			continue;
		}
		SnippetTabStop stop;
		stop.number = item[0];
		stop.start_line = item[1];
		stop.start_col = item[2];
		stop.end_line = item[3];
		stop.end_col = item[4];
		decoded.push_back(stop);
	}
	return decoded;
}

bool SnippetSession::_try_begin_inserted(const PendingSnippet &p_candidate) {
	if (_try_begin_inserted_at(p_candidate, p_candidate.text.length())) {
		return true;
	}

	// A completion can merge the text it inserts with the text already in the
	// line, and folding the merged brace pair removes up to two characters of
	// what was inserted before the caret (all within one deferred text change).
	// The text then left before the caret is a prefix of the armed text, so try
	// those prefixes too before giving the session up. Only a prefix long
	// enough to be unambiguous is accepted, so an unrelated insertion cannot
	// start a session by matching a couple of characters.
	static constexpr int MAX_MERGED_TAIL_LOST = 2;
	static constexpr int MIN_MATCHED_PREFIX = 4;
	for (int lost = 1; lost <= MAX_MERGED_TAIL_LOST; lost++) {
		const int length = (int)p_candidate.text.length() - lost;
		if (length < MIN_MATCHED_PREFIX) {
			break;
		}
		if (_try_begin_inserted_at(p_candidate, length)) {
			return true;
		}
	}
	return false;
}

bool SnippetSession::_try_begin_inserted_at(const PendingSnippet &p_candidate, int p_length) {
	const int caret_line = code_edit->get_caret_line();
	const int caret_col = code_edit->get_caret_column();

	const String expected = p_candidate.text.left(p_length);
	const PackedStringArray lines = expected.split("\n");
	int origin_line = 0;
	int origin_col = 0;
	if (lines.size() == 1) {
		origin_line = caret_line;
		origin_col = caret_col - (int)expected.length();
	} else {
		origin_line = caret_line - (lines.size() - 1);
		if (origin_line < 0) {
			return false;
		}
		origin_col = code_edit->get_line(origin_line).length() - lines[0].length();
	}
	if (origin_col < 0) {
		return false;
	}

	// TextEdit has no ranged get_text(): slice it line by line.
	String inserted;
	if (origin_line == caret_line) {
		inserted = code_edit->get_line(origin_line).substr(origin_col, caret_col - origin_col);
	} else {
		inserted = code_edit->get_line(origin_line).substr(origin_col);
		for (int l = origin_line + 1; l < caret_line; l++) {
			inserted += "\n" + code_edit->get_line(l);
		}
		inserted += "\n" + code_edit->get_line(caret_line).substr(0, caret_col);
	}
	if (inserted != expected) {
		return false;
	}

	Vector<SnippetTabStop> shifted = p_candidate.stops;
	for (SnippetTabStop &stop : shifted) {
		stop.start_line += origin_line;
		stop.end_line += origin_line;
		if (stop.start_line == origin_line) {
			stop.start_col += origin_col;
		}
		if (stop.end_line == origin_line) {
			stop.end_col += origin_col;
		}
	}

	if (p_length < (int)p_candidate.text.length()) {
		// The characters after the matched prefix were merged away, so a stop
		// that pointed into them has no place in the document anymore. Put it
		// at the caret instead of leaving it past the text, where Tab would
		// jump to a line that has nothing to do with the snippet.
		for (SnippetTabStop &stop : shifted) {
			if (stop.start_line > caret_line || (stop.start_line == caret_line && stop.start_col > caret_col)) {
				stop.start_line = caret_line;
				stop.start_col = caret_col;
			}
			if (stop.end_line > caret_line || (stop.end_line == caret_line && stop.end_col > caret_col)) {
				stop.end_line = caret_line;
				stop.end_col = caret_col;
			}
			if (stop.start_line > stop.end_line || (stop.start_line == stop.end_line && stop.start_col > stop.end_col)) {
				stop.end_line = stop.start_line;
				stop.end_col = stop.start_col;
			}
		}
	}

	begin(shifted);
	return true;
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

	// Undo and redo rewrite the text the recorded stops point into, and the
	// session cannot follow that: end it here, before the event reaches the
	// editor, so the key that follows cannot navigate into stale positions.
	// The event is not consumed, so the editor still undoes or redoes.
	if (key->is_action("ui_undo", true)) {
		if (code_edit->has_undo()) {
			end();
		}
		return;
	}
	if (key->is_action("ui_redo", true)) {
		if (code_edit->has_redo()) {
			end();
		}
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
	if (active) {
		const uint32_t version = code_edit->get_version();
		if (version < last_version) {
			// The text went back to an earlier version: an undo (from a menu, a
			// plugin, or a key the gui_input handler did not see) removed text
			// the stops and the snapshot describe. Anything navigated from here
			// on would be an unrelated place in the document.
			end();
		} else {
			last_version = version;
		}
	}

	if (!pending.is_empty()) {
		// A completion option carrying tab stops may just have been inserted:
		// match the text ending at the caret against the armed candidates.
		for (int i = 0; i < pending.size(); i++) {
			if (_try_begin_inserted(pending[i])) {
				pending.clear();
				return; // begin() applied the first stop and took a snapshot.
			}
		}
		pending.clear(); // Something else was inserted; the offer expired.
	}

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

	// The shifts above only follow a local edit. If they left a stop outside the
	// text (the snippet, or part of it, was deleted or replaced), navigating to
	// the closest place in range would land somewhere unrelated to the snippet,
	// so the session ends instead of guessing.
	for (const SnippetTabStop &stop : stops) {
		if (stop.start_line < 0 || stop.start_line >= current_lines.size() ||
				stop.end_line < 0 || stop.end_line >= current_lines.size() ||
				stop.start_col < 0 || stop.start_col > current_lines[stop.start_line].length() ||
				stop.end_col < 0 || stop.end_col > current_lines[stop.end_line].length() ||
				stop.start_line > stop.end_line ||
				(stop.start_line == stop.end_line && stop.start_col > stop.end_col)) {
			end();
			return;
		}
	}

	_take_snapshot();
}

void SnippetSession::_on_text_set() {
	// The whole text was replaced (document reloaded, buffer cleared): neither
	// the stops nor the armed candidates refer to the text in the editor
	// anymore. set_text() does not emit text_changed, so this is the only
	// notification that the session gets.
	end();
	pending.clear();
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
