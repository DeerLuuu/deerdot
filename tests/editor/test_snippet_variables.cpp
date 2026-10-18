/**************************************************************************/
/*  test_snippet_variables.cpp                                            */
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

#include "core/os/time.h"
#include "editor/gui/snippet_parser.h"
#include "tests/test_macros.h"

#include "modules/gdscript/gdscript.h"

TEST_FORCE_LINK(test_snippet_variables)

namespace TestSnippetVariables {

TEST_CASE("[Editor] Snippet variables expand and preserve node paths") {
	HashMap<String, String> variables;
	variables["TM_FILENAME"] = "player.gd";
	variables["TM_FILENAME_BASE"] = "player";
	variables["TM_DIRECTORY"] = "res://scripts";
	variables["CURRENT_YEAR"] = "2026";
	variables["CURRENT_MONTH"] = "10";
	variables["CURRENT_DATE"] = "2026-10-18";

	List<SnippetTabStop> stops;
	CHECK(SnippetParser::parse("$TM_FILENAME ${CURRENT_YEAR}", variables, stops) == "player.gd 2026");
	CHECK(SnippetParser::parse("${TM_FILENAME_BASE}:$CURRENT_DATE", variables, stops) == "player:2026-10-18");

	// Godot's node path syntax must never be consumed as a variable.
	CHECK(SnippetParser::parse("$Sprite2D", variables, stops) == "$Sprite2D");
	CHECK(SnippetParser::parse("$node/path", variables, stops) == "$node/path");
	CHECK(SnippetParser::parse("$\"Node\"", variables, stops) == "$\"Node\"");
	CHECK(SnippetParser::parse("$TM_FILENAME:$Sprite2D", variables, stops) == "player.gd:$Sprite2D");
	CHECK(SnippetParser::parse("$TM_WRONG_NAME", variables, stops) == "$TM_WRONG_NAME");
	CHECK(SnippetParser::parse("${UNKNOWN:fallback}", variables, stops) == "${UNKNOWN:fallback}");

	// Escapes and tab stops keep working.
	CHECK(SnippetParser::parse("\\$TM_FILENAME", variables, stops) == "$TM_FILENAME");
	CHECK(SnippetParser::parse("$$", variables, stops) == "$");
	CHECK(SnippetParser::parse("$1 ${1:x} ${2|a,b|}", variables, stops) == " x a");
	CHECK(stops.size() == 3);
}

TEST_CASE("[Editor] Snippet variables include padded date fields") {
	const HashMap<String, String> variables = GDScriptLanguage::make_snippet_variables("res://scripts/player.gd");
	CHECK(variables["TM_FILENAME"] == "player.gd");
	CHECK(variables["TM_FILENAME_BASE"] == "player");
	CHECK(variables["TM_DIRECTORY"] == "res://scripts");

	const Dictionary date = Time::get_singleton()->get_date_dict_from_system();
	CHECK(variables["CURRENT_YEAR"] == itos(int(date["year"])));
	CHECK(variables["CURRENT_MONTH"] == vformat("%02d", int(date["month"])));
	CHECK(variables["CURRENT_DATE"] == vformat("%04d-%02d-%02d", int(date["year"]), int(date["month"]), int(date["day"])));
}

TEST_CASE("[Editor] Snippet scope filtering") {
	CHECK(GDScriptLanguage::snippet_scope_matches("", "gdscript"));
	CHECK(GDScriptLanguage::snippet_scope_matches("gdscript", "gdscript"));
	CHECK(GDScriptLanguage::snippet_scope_matches("gdscript, gdshader", "gdscript"));
	CHECK(GDScriptLanguage::snippet_scope_matches("GDScript", "gdscript"));
	CHECK(!GDScriptLanguage::snippet_scope_matches("gdshader", "gdscript"));
	CHECK(!GDScriptLanguage::snippet_scope_matches("json, gdshader", "gdscript"));

	GDScriptLanguage::SnippetConfig config("name", "prefix", "body", "description", "gdscript, gdshader");
	CHECK(config.scope == "gdscript, gdshader");
}

} // namespace TestSnippetVariables
