/**************************************************************************/
/*  gdscript_snippet_editor.cpp                                           */
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

#include "gdscript_snippet_editor.h"

#include "../gdscript.h"
#include "gdscript_highlighter.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_paths.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/code_edit.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"

GDScriptSnippetEditor *GDScriptSnippetEditor::singleton = nullptr;

void GDScriptSnippetEditor::open_snippets_editor() {
	if (!singleton) {
		singleton = memnew(GDScriptSnippetEditor);
		EditorNode::get_singleton()->add_child(singleton);
	}

	singleton->_refresh_tab(TAB_GLOBAL);
	singleton->_refresh_tab(TAB_PROJECT);
	singleton->popup_centered(Size2i(1000, 640));
}

void GDScriptSnippetEditor::_build_tab(int p_index, const String &p_title, const String &p_dir) {
	SnippetDirTab &tab = get_tab(p_index);
	tab.dir_path = p_dir;

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_custom_minimum_size(Size2(0, 240));

	HBoxContainer *toolbar = memnew(HBoxContainer);
	vbox->add_child(toolbar);

	Button *refresh_button = memnew(Button);
	refresh_button->set_text(TTRC("Refresh"));
	refresh_button->set_focus_mode(Control::FOCUS_NONE);
	refresh_button->connect("pressed", callable_mp(this, &GDScriptSnippetEditor::_on_refresh_pressed).bind(p_index));
	toolbar->add_child(refresh_button);

	Button *open_dir_button = memnew(Button);
	open_dir_button->set_text(TTRC("Open Directory"));
	open_dir_button->set_focus_mode(Control::FOCUS_NONE);
	open_dir_button->connect("pressed", callable_mp(this, &GDScriptSnippetEditor::_on_open_dir_pressed).bind(p_index));
	toolbar->add_child(open_dir_button);

	Label *spacer = memnew(Label);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->add_child(spacer);

	Button *save_button = memnew(Button);
	save_button->set_text(TTRC("Save"));
	save_button->set_disabled(true);
	save_button->connect("pressed", callable_mp(this, &GDScriptSnippetEditor::_on_save_pressed).bind(p_index));
	toolbar->add_child(save_button);
	tab.save_button = save_button;

	HSplitContainer *split = memnew(HSplitContainer);
	split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	split->set_split_offset(240);
	vbox->add_child(split);

	tab.file_list = memnew(ItemList);
	tab.file_list->set_custom_minimum_size(Size2(220, 0));
	tab.file_list->connect("item_selected", callable_mp(this, &GDScriptSnippetEditor::_on_file_selected).bind(p_index));
	split->add_child(tab.file_list);

	tab.code_edit = memnew(CodeEdit);
	tab.code_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tab.code_edit->set_draw_line_numbers(true);
	tab.code_edit->set_highlight_current_line(true);
	tab.code_edit->set_draw_tabs(true);

	// Snippet bodies are GDScript fragments: reuse the regular script editor
	// highlighter for them.
	Ref<GDScriptSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	tab.code_edit->set_syntax_highlighter(highlighter);

	tab.code_edit->connect("text_changed", callable_mp(this, &GDScriptSnippetEditor::_on_text_changed).bind(p_index));
	split->add_child(tab.code_edit);

	tab.status = memnew(Label);
	tab.status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	tab.status->hide();
	vbox->add_child(tab.status);

	tab_container->add_child(vbox);
	tab_container->set_tab_title(p_index, p_title);
}

void GDScriptSnippetEditor::_refresh_tab(int p_index) {
	SnippetDirTab &tab = get_tab(p_index);
	tab.file_list->clear();

	PackedStringArray names;
	if (DirAccess::exists(tab.dir_path)) {
		Ref<DirAccess> dir = DirAccess::open(tab.dir_path);
		if (dir.is_valid()) {
			dir->list_dir_begin();
			String file_name = dir->get_next();
			while (!file_name.is_empty()) {
				if (!dir->current_is_dir() && (file_name.ends_with(".json") || file_name.ends_with(".code-snippets"))) {
					names.push_back(file_name);
				}
				file_name = dir->get_next();
			}
			dir->list_dir_end();
		}
	}
	names.sort();

	for (const String &name : names) {
		tab.file_list->add_item(name);
		if (name == tab.current_path.get_file()) {
			tab.file_list->select(tab.file_list->get_item_count() - 1);
		}
	}

	if (tab.current_path.is_empty() || !names.has(tab.current_path.get_file())) {
		// Nothing loaded yet, or the loaded file was removed on disk.
		tab.current_path = String();
		tab.dirty = false;
		tab.code_edit->set_text(String());
		tab.save_button->set_disabled(true);
	}

	if (!DirAccess::exists(tab.dir_path)) {
		tab.status->set_text(TTRC("This directory does not exist yet. Use \"Open Directory\" to create it, then add .json snippet files."));
		tab.status->show();
	} else if (tab.file_list->get_item_count() == 0) {
		tab.status->set_text(TTRC("No snippet files (.json, .code-snippets) in this directory."));
		tab.status->show();
	} else {
		tab.status->hide();
	}
}

void GDScriptSnippetEditor::_load_file(int p_index, const String &p_path) {
	SnippetDirTab &tab = get_tab(p_index);

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		error_dialog->set_text(vformat(TTRC("Could not open \"%s\"."), p_path));
		error_dialog->popup_centered();
		return;
	}

	tab.current_path = p_path;
	tab.code_edit->set_text(f->get_as_text());
	tab.dirty = false;
	tab.save_button->set_disabled(true);
	tab.status->hide();
}

bool GDScriptSnippetEditor::_save_current(int p_index) {
	SnippetDirTab &tab = get_tab(p_index);
	if (tab.current_path.is_empty()) {
		return false;
	}

	String text = tab.code_edit->get_text();

	JSON json;
	if (json.parse(text) != OK) {
		error_dialog->set_text(vformat(TTRC("Invalid JSON in \"%s\" (line %d): %s\nFix the error to save."),
				tab.current_path.get_file(), json.get_error_line(), json.get_error_message()));
		error_dialog->popup_centered();
		return false;
	}

	Ref<FileAccess> f = FileAccess::open(tab.current_path, FileAccess::WRITE);
	if (f.is_null()) {
		error_dialog->set_text(vformat(TTRC("Could not write \"%s\"."), tab.current_path));
		error_dialog->popup_centered();
		return false;
	}
	f->store_string(text);
	f->flush();

	// Apply the changes right away instead of waiting for the poll.
	if (GDScriptLanguage::get_singleton()) {
		GDScriptLanguage::get_singleton()->load_snippets();
	}

	tab.dirty = false;
	tab.save_button->set_disabled(true);
	return true;
}

void GDScriptSnippetEditor::_on_file_selected(int p_item, int p_tab) {
	SnippetDirTab &tab = get_tab(p_tab);
	String name = tab.file_list->get_item_text(p_item);
	String path = tab.dir_path.path_join(name);
	if (path == tab.current_path) {
		return;
	}

	if (tab.dirty) {
		pending_tab = p_tab;
		pending_path = path;
		switch_confirm->set_text(vformat(TTRC("Save changes to \"%s\"?"), tab.current_path.get_file()));
		switch_confirm->popup_centered();
		return;
	}

	_load_file(p_tab, path);
}

void GDScriptSnippetEditor::_on_text_changed(int p_tab) {
	SnippetDirTab &tab = get_tab(p_tab);
	if (!tab.dirty) {
		tab.dirty = true;
		tab.save_button->set_disabled(false);
	}
}

void GDScriptSnippetEditor::_on_save_pressed(int p_tab) {
	_save_current(p_tab);
}

void GDScriptSnippetEditor::_on_refresh_pressed(int p_tab) {
	_refresh_tab(p_tab);
}

void GDScriptSnippetEditor::_on_open_dir_pressed(int p_tab) {
	SnippetDirTab &tab = get_tab(p_tab);

	String os_path = ProjectSettings::get_singleton()->globalize_path(tab.dir_path);
	if (!DirAccess::exists(tab.dir_path)) {
		Error err = DirAccess::make_dir_recursive_absolute(os_path);
		if (err != OK) {
			error_dialog->set_text(vformat(TTRC("Could not create directory \"%s\" (error %d)."), os_path, err));
			error_dialog->popup_centered();
			return;
		}
		_refresh_tab(p_tab);
	}
	OS::get_singleton()->shell_open(os_path);
}

void GDScriptSnippetEditor::_revert_selection(int p_tab) {
	// Revert the file list selection to the file that is still loaded.
	SnippetDirTab &tab = get_tab(p_tab);
	for (int i = 0; i < tab.file_list->get_item_count(); i++) {
		if (tab.file_list->get_item_text(i) == tab.current_path.get_file()) {
			tab.file_list->select(i);
			break;
		}
	}
}

void GDScriptSnippetEditor::_on_switch_confirm_confirmed() {
	if (pending_tab < 0) {
		return;
	}
	if (_save_current(pending_tab)) {
		_load_file(pending_tab, pending_path);
	} else {
		// Save failed (e.g. invalid JSON): stay on the loaded file.
		_revert_selection(pending_tab);
	}
	pending_tab = -1;
	pending_path = String();
}

void GDScriptSnippetEditor::_on_switch_confirm_discard(const String &p_action) {
	if (p_action != "discard" || pending_tab < 0) {
		return;
	}
	_load_file(pending_tab, pending_path);
	pending_tab = -1;
	pending_path = String();
}

void GDScriptSnippetEditor::_on_switch_confirm_cancelled() {
	if (pending_tab < 0) {
		return;
	}
	_revert_selection(pending_tab);
	pending_tab = -1;
	pending_path = String();
}

GDScriptSnippetEditor::GDScriptSnippetEditor() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

	set_title(TTRC("Code Snippets"));
	set_min_size(Size2i(860, 520));

	tab_container = memnew(TabContainer);
	tab_container->set_use_hidden_tabs_for_min_size(true);
	add_child(tab_container);

	String global_dir;
	String project_dir = "res://snippets";
	if (EditorPaths::get_singleton()) {
		global_dir = EditorPaths::get_singleton()->get_snippets_dir();
		project_dir = EditorPaths::get_singleton()->get_project_snippets_dir();
	}

	_build_tab(TAB_GLOBAL, TTRC("Global Snippets"), global_dir);
	_build_tab(TAB_PROJECT, TTRC("Project Snippets"), project_dir);

	switch_confirm = memnew(SnippetSwitchConfirm);
	switch_confirm->set_ok_button_text(TTRC("Save"));
	switch_confirm->add_button(TTRC("Discard"), false, "discard");
	switch_confirm->connect("confirmed", callable_mp(this, &GDScriptSnippetEditor::_on_switch_confirm_confirmed));
	switch_confirm->connect("custom_action", callable_mp(this, &GDScriptSnippetEditor::_on_switch_confirm_discard));
	switch_confirm->cancel_callable = callable_mp(this, &GDScriptSnippetEditor::_on_switch_confirm_cancelled);
	add_child(switch_confirm);

	error_dialog = memnew(AcceptDialog);
	error_dialog->set_title(TTRC("Error"));
	error_dialog->set_ok_button_text(TTRC("Close"));
	add_child(error_dialog);
}
