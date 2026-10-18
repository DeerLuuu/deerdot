/**************************************************************************/
/*  snippet_parser.cpp                                                    */
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

#include "snippet_parser.h"

namespace {

struct OutputBuilder {
	String text;
	int line = 0;
	int col = 0;

	void append(char32_t p_char) {
		text += p_char;
		if (p_char == '\n') {
			line++;
			col = 0;
		} else {
			col++;
		}
	}

	void append(const String &p_string) {
		for (int i = 0; i < p_string.length(); i++) {
			append(p_string[i]);
		}
	}
};

bool is_word_char(char32_t p_char) {
	return (p_char >= '0' && p_char <= '9') || (p_char >= 'A' && p_char <= 'Z') ||
			(p_char >= 'a' && p_char <= 'z') || p_char == '_';
}

bool is_digit_char(char32_t p_char) {
	return p_char >= '0' && p_char <= '9';
}

} // namespace

namespace SnippetParser {

String parse(const String &p_body, const HashMap<String, String> &p_variables, List<SnippetTabStop> &r_tabstops) {
	OutputBuilder out;

	const int len = p_body.length();
	int i = 0;
	while (i < len) {
		char32_t ch = p_body[i];

		if (ch == '\\' && i + 1 < len && (p_body[i + 1] == '$' || p_body[i + 1] == '\\' || p_body[i + 1] == '{' || p_body[i + 1] == '}')) {
			out.append(p_body[i + 1]);
			i += 2;
			continue;
		}

		if (ch != '$') {
			out.append(ch);
			i++;
			continue;
		}

		// ch == '$'.
		if (i + 1 >= len) {
			out.append('$');
			i++;
			continue;
		}

		char32_t next = p_body[i + 1];
		if (next == '$') {
			out.append('$');
			i += 2;
			continue;
		}

		if (next == '{') {
			// Find the matching closing brace.
			int depth = 1;
			int j = i + 2;
			while (j < len && depth > 0) {
				if (p_body[j] == '{' && p_body[j - 1] != '\\') {
					depth++;
				} else if (p_body[j] == '}' && p_body[j - 1] != '\\') {
					depth--;
					if (depth == 0) {
						break;
					}
				}
				j++;
			}
			if (j >= len) {
				// Unbalanced: keep the rest literally.
				out.append(p_body.substr(i));
				break;
			}

			const String content = p_body.substr(i + 2, j - i - 2);
			i = j + 1;

			// Split into key and remainder at the first ':' or '|'.
			const int sep = content.find(":");
			const int choice_sep = content.find("|");
			String key = content;
			String remainder;
			bool has_choices = false;
			int split_at = -1;
			if (choice_sep != -1 && (sep == -1 || choice_sep < sep)) {
				has_choices = true;
				split_at = choice_sep;
			} else if (sep != -1) {
				split_at = sep;
			}
			if (split_at != -1) {
				key = content.substr(0, split_at);
				remainder = content.substr(split_at + 1);
			}

			bool is_number = !key.is_empty();
			for (int k = 0; k < key.length(); k++) {
				if (!is_digit_char(key[k])) {
					is_number = false;
					break;
				}
			}

			if (is_number) {
				SnippetTabStop stop;
				stop.number = key.to_int();
				stop.start_line = out.line;
				stop.start_col = out.col;

				String placeholder;
				if (has_choices) {
					// ${n|a,b,c|}: the remainder normally ends with a trailing '|'.
					const int trailing = remainder.rfind("|");
					const String choices = trailing == -1 ? remainder : remainder.substr(0, trailing);
					const int first_choice = choices.find(",");
					placeholder = first_choice == -1 ? choices : choices.substr(0, first_choice);
				} else {
					placeholder = remainder;
				}
				out.append(placeholder);

				stop.end_line = out.line;
				stop.end_col = out.col;
				r_tabstops.push_back(stop);
			} else {
				// Variable: ${name} or ${name:default}. Unknown variables are
				// kept literally: GDScript snippet bodies contain `$Node` syntax.
				if (p_variables.has(key)) {
					out.append(p_variables[key]);
				} else {
					out.append("${");
					out.append(content);
					out.append("}");
				}
			}
			continue;
		}

		if (is_digit_char(next)) {
			int j = i + 1;
			while (j < len && is_digit_char(p_body[j])) {
				j++;
			}
			SnippetTabStop stop;
			stop.number = p_body.substr(i + 1, j - i - 1).to_int();
			stop.start_line = out.line;
			stop.start_col = out.col;
			stop.end_line = out.line;
			stop.end_col = out.col;
			r_tabstops.push_back(stop);
			i = j;
			continue;
		}

		if (is_word_char(next)) {
			int j = i + 1;
			while (j < len && is_word_char(p_body[j])) {
				j++;
			}
			const String name = p_body.substr(i + 1, j - i - 1);
			if (p_variables.has(name)) {
				out.append(p_variables[name]);
			} else {
				// Preserve unknown names (notably `$Sprite2D`/`$node/path`) instead
				// of dropping them. Use `\$NAME` for a literal dollar sign.
				out.append("$");
				out.append(name);
			}
			i = j;
			continue;
		}

		// Lone '$'.
		out.append('$');
		i++;
	}

	return out.text;
}

} // namespace SnippetParser
