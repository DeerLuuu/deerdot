/**************************************************************************/
/*  snippet_json_highlighter.h                                            */
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

#include "scene/resources/syntax_highlighter.h"

// A JSON syntax highlighter aware of the code snippets file layout: string
// values of "body" keys (and items of such arrays) contain GDScript code, so
// their content is highlighted with a small GDScript rule set, while the rest
// of the file gets regular JSON colors.
//
// It also records where those code strings are, so the snippets editor can
// limit code completion to positions inside them.
class SnippetJSONHighlighter : public SyntaxHighlighter {
	GDCLASS(SnippetJSONHighlighter, SyntaxHighlighter);

	struct CodeSpan {
		int line = 0;
		int from = 0; // Column of the opening quote.
		int to = 0; // Column of the closing quote.
	};

	HashMap<int, Dictionary> line_cache;
	LocalVector<CodeSpan> code_spans;
	bool cache_built = false;
	String last_text;

	Color font_color;
	Color symbol_color;
	Color number_color;
	Color string_color;
	Color comment_color;
	Color keyword_color;
	Color member_color;
	Color placeholder_color;
	Color function_color;

	// Cycling palette used to color bracket pairs by nesting depth.
	enum { BRACKET_PALETTE_SIZE = 4 };
	Color bracket_palette[BRACKET_PALETTE_SIZE];

	HashSet<StringName> reserved_words;
	HashSet<StringName> control_flow_keywords;
	HashMap<StringName, Color> type_colors; // Engine, user, autoload and Variant type names.
	Color control_flow_keyword_color;

	void _load_colors();
	void _parse_text();

protected:
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;
	virtual void _clear_highlighting_cache() override;

public:
	// Re-parses if the TextEdit text changed. The base class only invalidates
	// through lines_edited_from, which early-returns while its own cache is
	// empty (e.g. right after the window opened with an empty editor), so the
	// text has to be validated manually.
	void ensure_parsed();

	bool is_position_in_code(int p_line, int p_column) const;

	SnippetJSONHighlighter();
};
