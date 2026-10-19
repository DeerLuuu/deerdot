/**************************************************************************/
/*  test_script_reveal.cpp                                                */
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

#include "editor/gui/code_editor.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_script_reveal)

namespace TestScriptReveal {

static CodeTextEditor *make_editor() {
	CodeTextEditor *editor = memnew(CodeTextEditor);
	SceneTree::get_singleton()->get_root()->add_child(editor);
	editor->get_text_editor()->set_text("var a = 1;\nvar b = 2;\nvar c = 3;\n");
	return editor;
}

static void finish_animation() {
	for (int i = 0; i < 10; i++) {
		SceneTree::get_singleton()->process(0.1);
	}
}

TEST_CASE("[Editor][CodeEdit] Ghost reveal material lifecycle") {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	const Variant old_animations = settings->get_setting("interface/theme/ui_animations");
	const Variant old_style = settings->get_setting("interface/theme/ui_animations_style");
	settings->set_setting("interface/theme/ui_animations", true);
	settings->set_setting("interface/theme/ui_animations_style", "Expressive");

	CodeTextEditor *editor = make_editor();
	Ref<ShaderMaterial> original;
	original.instantiate();
	editor->get_text_editor()->set_material(original);

	editor->play_intro_reveal();
	Ref<Material> reveal = editor->get_text_editor()->get_material();
	CHECK(reveal.is_valid());
	Ref<ShaderMaterial> reveal_material = reveal;
	CHECK(reveal_material.is_valid());
	CHECK(reveal_material.ptr() != original.ptr());
	CHECK(editor->get_text_editor()->has_meta(SNAME("_intro_prev_material")));

	// The shader must keep both phases and the continuous alpha handoff.
	const String shader_code = reveal_material->get_shader()->get_code();
	CHECK(shader_code.contains("ghost_color"));
	CHECK(shader_code.contains("t < 0.62"));
	CHECK(shader_code.contains("mix(ghost_color.rgb, COLOR.rgb, cross)"));
	CHECK(shader_code.contains("mix(0.9, 1.0, blend)"));

	// A second call while the reveal is running must not replace the material.
	editor->play_intro_reveal();
	CHECK(editor->get_text_editor()->get_material().ptr() == reveal_material.ptr());

	finish_animation();
	CHECK(editor->get_text_editor()->get_material().ptr() == original.ptr());
	CHECK_FALSE(editor->get_text_editor()->has_meta(SNAME("_intro_prev_material")));

	editor->queue_free();
	MessageQueue::get_singleton()->flush();
	settings->set_setting("interface/theme/ui_animations", old_animations);
	settings->set_setting("interface/theme/ui_animations_style", old_style);
}

TEST_CASE("[Editor][CodeEdit] Ghost reveal respects disabled UI animations") {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	const Variant old_animations = settings->get_setting("interface/theme/ui_animations");
	const Variant old_style = settings->get_setting("interface/theme/ui_animations_style");
	settings->set_setting("interface/theme/ui_animations", false);
	settings->set_setting("interface/theme/ui_animations_style", "Expressive");

	CodeTextEditor *editor = make_editor();
	editor->play_intro_reveal();
	CHECK(editor->get_text_editor()->get_material().is_null());
	CHECK_FALSE(editor->get_text_editor()->has_meta(SNAME("_intro_prev_material")));

	editor->queue_free();
	MessageQueue::get_singleton()->flush();
	settings->set_setting("interface/theme/ui_animations", old_animations);
	settings->set_setting("interface/theme/ui_animations_style", old_style);
}

} // namespace TestScriptReveal
