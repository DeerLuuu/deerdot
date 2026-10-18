/**************************************************************************/
/*  test_layout_preset_structure.cpp                                      */
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

#include "editor/script/script_editor_plugin.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_layout_preset_structure)

namespace TestLayoutPresetStructure {

static Dictionary make_leaf(const String &p_script) {
	Dictionary leaf;
	leaf["scripts"] = Array{ p_script };
	leaf["help"] = Array{ "Node" };
	leaf["selected"] = p_script;
	return leaf;
}

TEST_CASE("[Editor] Layout preset structure filter keeps the tree and clears leaves") {
	Dictionary lower;
	lower["direction"] = "vertical";
	lower["offset"] = 7;
	lower["first"] = make_leaf("res://a.gd");
	lower["second"] = make_leaf("res://b.gd");

	Dictionary split;
	split["direction"] = "horizontal";
	split["offset"] = 123;
	split["first"] = lower;
	split["second"] = make_leaf("res://c.gd");

	const Dictionary filtered = DocumentEditorContainer::_filter_layout_to_structure(split);
	CHECK(filtered["direction"] == "horizontal");
	CHECK(int(filtered["offset"]) == 123);
	const Dictionary filtered_lower = filtered["first"];
	CHECK(filtered_lower["direction"] == "vertical");
	CHECK(int(filtered_lower["offset"]) == 7);
	const Dictionary leaf = filtered_lower["first"];
	CHECK(Array(leaf["scripts"]).is_empty());
	CHECK(Array(leaf["help"]).is_empty());
	CHECK(String(leaf["selected"]).is_empty());
	CHECK(Array(Dictionary(filtered["second"])["scripts"]).is_empty());
}

TEST_CASE("[Editor] Layout preset structure filter handles empty and legacy nodes") {
	const Dictionary empty = DocumentEditorContainer::_filter_layout_to_structure(Dictionary());
	CHECK(Array(empty["scripts"]).is_empty());
	CHECK(Array(empty["help"]).is_empty());
	CHECK(String(empty["selected"]).is_empty());

	Dictionary legacy_leaf;
	legacy_leaf["scripts"] = Array{ "res://legacy.gd" };
	const Dictionary legacy = DocumentEditorContainer::_filter_layout_to_structure(legacy_leaf);
	CHECK(Array(legacy["scripts"]).is_empty());
	CHECK(String(legacy["selected"]).is_empty());

	Dictionary split_without_children;
	split_without_children["direction"] = "vertical";
	const Dictionary filtered = DocumentEditorContainer::_filter_layout_to_structure(split_without_children);
	CHECK(Array(filtered["scripts"]).is_empty());
}

} // namespace TestLayoutPresetStructure
