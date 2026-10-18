/**************************************************************************/
/*  test_bracket_colors.cpp                                               */
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

#include "editor/script/syntax_highlighters.h"
#include "scene/gui/text_edit.h"
#include "tests/test_macros.h"

#include "modules/gdscript/editor/bracket_palette.h"

TEST_FORCE_LINK(test_bracket_colors)

namespace TestBracketColors {

static Color color_at(const Dictionary &p_map, int p_position) {
	int best = -1;
	Color color = Color(0, 0, 0, 0);
	for (const KeyValue<Variant, Variant> &E : p_map) {
		const int key = E.key;
		if (key <= p_position && key > best) {
			best = key;
			const Dictionary info = E.value;
			color = info["color"];
		}
	}
	return color;
}

TEST_CASE("[Editor] JSON brackets are colored by depth") {
	TextEdit *text_edit = memnew(TextEdit);
	text_edit->set_text("{\n[\n{\n{\n}\n}\n]\n}\n");
	Ref<EditorJSONSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	highlighter->set_text_edit(text_edit);
	highlighter->update_cache();

	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 0) == bracket_palette_color(0));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(1), 0) == bracket_palette_color(1));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(2), 0) == bracket_palette_color(2));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(3), 0) == bracket_palette_color(3));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(4), 0) == bracket_palette_color(3));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(7), 0) == bracket_palette_color(0));

	memdelete(text_edit);
}

TEST_CASE("[Editor] JSON brackets inside strings keep the string color") {
	TextEdit *text_edit = memnew(TextEdit);
	text_edit->set_text("{\"key\": \"value {[]}\"}\n");
	Ref<EditorJSONSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	highlighter->set_text_edit(text_edit);
	highlighter->update_cache();

	const Dictionary map = highlighter->get_line_syntax_highlighting(0);
	CHECK(color_at(map, 0) == bracket_palette_color(0));
	const int string_start = text_edit->get_line(0).find("\"value");
	CHECK(string_start >= 0);
	CHECK(color_at(map, string_start + 8) != bracket_palette_color(0));
	CHECK(color_at(map, string_start + 9) != bracket_palette_color(0));
	CHECK(color_at(map, string_start + 10) != bracket_palette_color(0));
	CHECK(color_at(map, string_start + 11) != bracket_palette_color(0));

	memdelete(text_edit);
}

TEST_CASE("[Editor] Shader comments keep their color and depth carries across lines") {
	TextEdit *text_edit = memnew(TextEdit);
	text_edit->set_text("void f() { // {\n    /* [ */\n    vec3 v = vec3(1.0);\n}\n");
	Ref<GDShaderSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	highlighter->set_text_edit(text_edit);
	highlighter->update_cache();

	const Dictionary line0 = highlighter->get_line_syntax_highlighting(0);
	CHECK(color_at(line0, 9) == bracket_palette_color(0));
	CHECK(color_at(line0, 15).a < 0.9); // `{` inside `//` stays comment-colored.
	const Dictionary line1 = highlighter->get_line_syntax_highlighting(1);
	CHECK(color_at(line1, 7).a < 0.9); // `[` inside `/* */` stays comment-colored.
	CHECK(color_at(highlighter->get_line_syntax_highlighting(3), 0) == bracket_palette_color(0));

	memdelete(text_edit);
}

TEST_CASE("[Editor] Disabled shader regions override bracket colors") {
	TextEdit *text_edit = memnew(TextEdit);
	text_edit->set_text("{\n}\n");
	Ref<GDShaderSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	highlighter->set_text_edit(text_edit);
	highlighter->update_cache();

	const Color disabled_color = Color(1, 0, 1);
	highlighter->add_disabled_branch_region(Point2i(0, 0));
	highlighter->set_disabled_branch_color(disabled_color);
	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 0) == disabled_color);
	CHECK(color_at(highlighter->get_line_syntax_highlighting(1), 0) == bracket_palette_color(0));

	highlighter->clear_disabled_branch_regions();
	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 0) == bracket_palette_color(0));

	memdelete(text_edit);
}

TEST_CASE("[Editor] Bracket depth cache is rebuilt after cache invalidation") {
	TextEdit *text_edit = memnew(TextEdit);
	text_edit->set_text("{{}}\n");
	Ref<GDShaderSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	highlighter->set_text_edit(text_edit);
	highlighter->update_cache();

	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 0) == bracket_palette_color(0));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 1) == bracket_palette_color(1));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 2) == bracket_palette_color(1));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 3) == bracket_palette_color(0));

	highlighter->clear_highlighting_cache();
	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 0) == bracket_palette_color(0));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 1) == bracket_palette_color(1));
	CHECK(color_at(highlighter->get_line_syntax_highlighting(0), 3) == bracket_palette_color(0));

	memdelete(text_edit);
}

} // namespace TestBracketColors
