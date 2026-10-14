/**************************************************************************/
/*  snippet_json_highlighter.cpp                                          */
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

#include "snippet_json_highlighter.h"

#include "../gdscript.h"

#include "core/object/class_db.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/text_edit.h"

static bool is_snippet_word_char(char32_t p_char) {
	return (p_char >= '0' && p_char <= '9') || (p_char >= 'A' && p_char <= 'Z') ||
			(p_char >= 'a' && p_char <= 'z') || p_char == '_';
}

static bool is_snippet_digit(char32_t p_char) {
	return p_char >= '0' && p_char <= '9';
}

void SnippetJSONHighlighter::_load_colors() {
	// Note: the base font color is registered as "text_color", not "font_color".
	font_color = EDITOR_GET("text_editor/theme/highlighting/text_color");
	symbol_color = EDITOR_GET("text_editor/theme/highlighting/symbol_color");
	number_color = EDITOR_GET("text_editor/theme/highlighting/number_color");
	string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	member_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
	placeholder_color = EDITOR_GET("text_editor/theme/highlighting/string_placeholder_color");
	function_color = EDITOR_GET("text_editor/theme/highlighting/function_color");

	reserved_words.clear();
	if (GDScriptLanguage::get_singleton()) {
		Vector<String> words = GDScriptLanguage::get_singleton()->get_reserved_words();
		for (const String &word : words) {
			reserved_words.insert(word);
		}
	}
}

void SnippetJSONHighlighter::_parse_text() {
	_load_colors();
	line_cache.clear();
	code_spans.clear();
	cache_built = true;

	String text = get_text_edit()->get_text();
	last_text = text;
	if (text.is_empty()) {
		return;
	}

	// JSON state.
	bool expect_key = true; // Next string is an object key?
	bool value_is_body = false; // The value after the last ':' is a body?
	String last_key;
	LocalVector<bool> stack; // true = object, false = array.
	LocalVector<int> body_frames; // Stack indexes of arrays holding body values.

	bool in_string = false;
	bool string_is_key = false;
	bool string_is_code = false;
	bool inner_string = false; // Inside \" ... \" within a code string.
	int string_line = 0;
	int string_start_col = 0;
	String key_builder;

	// Color run tracking: line_cache entries mark where a color starts.
	int run_line = 0;
	int run_col = 0;
	Color run_color = font_color;
	bool run_open = false;

	auto close_run = [&]() {
		if (run_open) {
			Dictionary colors = line_cache.has(run_line) ? line_cache[run_line] : Dictionary();
			// TextEdit expects each entry to be a Dictionary holding "color".
			Dictionary color_info;
			color_info["color"] = run_color;
			colors[run_col] = color_info;
			line_cache[run_line] = colors;
			run_open = false;
		}
	};
	auto ensure_run = [&](int p_line, int p_col, const Color &p_color) {
		if (!run_open || run_color != p_color || run_line != p_line) {
			close_run();
			run_line = p_line;
			run_col = p_col;
			run_color = p_color;
			run_open = true;
		}
	};

	PackedStringArray lines = text.split("\n");
	for (int line_idx = 0; line_idx < lines.size(); line_idx++) {
		const String &line = lines[line_idx];
		close_run(); // Color runs never cross lines.

		const int line_len = line.length();
		int col = 0;
		while (col < line_len) {
			char32_t ch = line[col];

			if (in_string) {
				if (ch == '\\' && col + 1 < line_len) {
					if (string_is_key) {
						key_builder += line[col + 1];
					}
					if (line[col + 1] == '"' && string_is_code) {
						// `\"` delimits a GDScript string literal inside the code.
						inner_string = !inner_string;
						ensure_run(line_idx, col, string_color);
					}
					col += 2;
					continue;
				}
				if (ch == '"') {
					if (string_is_code) {
						CodeSpan span;
						span.line = string_line;
						span.from = string_start_col;
						span.to = col;
						code_spans.push_back(span);
					}
					if (string_is_key) {
						last_key = key_builder;
					}
					in_string = false;
					inner_string = false;
					ensure_run(line_idx, col, string_is_key ? member_color : (string_is_code ? font_color : string_color));
					col++;
					continue;
				}

				if (string_is_code && !inner_string) {
					if (ch == '#') {
						// GDScript comment: color the rest of the line, but stop
						// at the string's own closing quote.
						ensure_run(line_idx, col, comment_color);
						int j = col + 1;
						while (j < line_len) {
							if (line[j] == '\\') {
								j += 2;
								continue;
							}
							if (line[j] == '"') {
								break;
							}
							j++;
						}
						col = j;
						continue;
					}
					if (ch == '$') {
						// Tabstop ($0, $1...) or variable (${name}, $name).
						int j = col + 1;
						if (j < line_len && line[j] == '{') {
							while (j < line_len && line[j] != '}') {
								j++;
							}
							if (j < line_len) {
								j++;
							}
						} else {
							while (j < line_len && is_snippet_word_char(line[j])) {
								j++;
							}
						}
						ensure_run(line_idx, col, placeholder_color);
						col = j;
						continue;
					}
					if (is_snippet_word_char(ch)) {
						int j = col;
						while (j < line_len && is_snippet_word_char(line[j])) {
							j++;
						}
						String word = line.substr(col, j - col);

						Color word_color = font_color;
						bool all_digits = true;
						for (int k = 0; k < word.length(); k++) {
							if (!is_snippet_digit(word[k])) {
								all_digits = false;
								break;
							}
						}
						if (reserved_words.has(word)) {
							word_color = keyword_color;
						} else if (all_digits) {
							word_color = number_color;
						} else {
							int k = j;
							while (k < line_len && line[k] == ' ') {
								k++;
							}
							if (k < line_len && line[k] == '(') {
								word_color = function_color;
							}
						}
						ensure_run(line_idx, col, word_color);
						col = j;
						continue;
					}
					// Plain code content.
					ensure_run(line_idx, col, font_color);
					col++;
					continue;
				}

				// Key content, plain value content and inner GDScript string
				// content keep the color their string opened with.
				if (string_is_key) {
					key_builder += ch;
				} else if (string_is_code && inner_string) {
					ensure_run(line_idx, col, string_color);
				}
				col++;
				continue;
			}

			switch (ch) {
				case '"': {
					in_string = true;
					inner_string = false;
					string_is_key = expect_key;
					string_is_code = !string_is_key && (value_is_body || !body_frames.is_empty());
					string_line = line_idx;
					string_start_col = col;
					key_builder = String();
					ensure_run(line_idx, col, string_is_key ? member_color : (string_is_code ? font_color : string_color));
					col++;
				} break;
				case ':': {
					expect_key = false;
					value_is_body = (last_key == String("body"));
					ensure_run(line_idx, col, symbol_color);
					col++;
				} break;
				case ',': {
					expect_key = (!stack.is_empty() && stack[stack.size() - 1]);
					ensure_run(line_idx, col, symbol_color);
					col++;
				} break;
				case '{': {
					stack.push_back(true);
					expect_key = true;
					ensure_run(line_idx, col, symbol_color);
					col++;
				} break;
				case '}': {
					if (!stack.is_empty()) {
						stack.remove_at(stack.size() - 1);
					}
					expect_key = true;
					ensure_run(line_idx, col, symbol_color);
					col++;
				} break;
				case '[': {
					stack.push_back(false);
					if (value_is_body || !body_frames.is_empty()) {
						body_frames.push_back(stack.size() - 1);
					}
					expect_key = false;
					ensure_run(line_idx, col, symbol_color);
					col++;
				} break;
				case ']': {
					if (!stack.is_empty()) {
						stack.remove_at(stack.size() - 1);
					}
					while (!body_frames.is_empty() && body_frames[body_frames.size() - 1] >= (int)stack.size()) {
						body_frames.remove_at(body_frames.size() - 1);
					}
					expect_key = false;
					ensure_run(line_idx, col, symbol_color);
					col++;
				} break;
				default: {
					if (is_snippet_digit(ch)) {
						int j = col;
						while (j < line_len && (is_snippet_digit(line[j]) || line[j] == '.')) {
							j++;
						}
						ensure_run(line_idx, col, number_color);
						col = j;
					} else if (ch == 't' || ch == 'f' || ch == 'n') {
						int j = col;
						while (j < line_len && is_snippet_word_char(line[j])) {
							j++;
						}
						String word = line.substr(col, j - col);
						Color word_color = (word == "true" || word == "false" || word == "null") ? keyword_color : font_color;
						ensure_run(line_idx, col, word_color);
						col = j;
					} else if (ch == ' ' || ch == '\t' || ch == '\r') {
						col++;
					} else {
						ensure_run(line_idx, col, symbol_color);
						col++;
					}
				} break;
			}
		}
		close_run();
	}
}

Dictionary SnippetJSONHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	ensure_parsed();
	if (line_cache.has(p_line)) {
		return line_cache[p_line];
	}
	return Dictionary();
}

void SnippetJSONHighlighter::_clear_highlighting_cache() {
	// Rebuilt lazily on the next painted line, so typing stays cheap.
	cache_built = false;
}

void SnippetJSONHighlighter::ensure_parsed() {
	if (!cache_built || get_text_edit()->get_text() != last_text) {
		_parse_text();
	}
}

bool SnippetJSONHighlighter::is_position_in_code(int p_line, int p_column) const {
	for (const CodeSpan &span : code_spans) {
		if (span.line == p_line && p_column > span.from && p_column <= span.to) {
			return true;
		}
	}
	return false;
}

SnippetJSONHighlighter::SnippetJSONHighlighter() {
}
