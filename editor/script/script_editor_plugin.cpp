/**************************************************************************/
/*  script_editor_plugin.cpp                                              */
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

#include "script_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/editor_language.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/string/fuzzy_search.h"
#include "core/variant/dictionary.h"
#include "core/version.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/doc/editor_help.h"
#include "editor/doc/editor_help_search.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/docks/inspector_dock.h"
#include "editor/editor_interface.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/file_system/editor_paths.h"
#include "editor/gui/code_editor.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/gui/filter_line_edit.h"
#include "editor/gui/window_wrapper.h"
#include "editor/inspector/editor_context_menu_plugin.h"
#include "editor/run/editor_run_bar.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/script/find_in_files.h"
#include "editor/script/script_editor_base.h"
#include "editor/script/script_editor_navigation_marker.h"
#include "editor/script/script_text_editor.h"
#include "editor/script/syntax_highlighters.h"
#include "editor/script/text_editor.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "editor/shader/shader_create_dialog.h"
#include "editor/shader/shader_editor_plugin.h"
#include "editor/shader/shader_text_editor.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/separator.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/style_box_flat.h"
#include "servers/display/display_server.h"

void ScriptEditorQuickOpen::popup_dialog(const Vector<String> &p_functions, bool p_dontclear) {
	popup_centered_ratio(0.6);
	if (p_dontclear) {
		search_box->select_all();
	} else {
		search_box->clear();
	}
	search_box->grab_focus();
	functions = p_functions;
	_update_search();
}

void ScriptEditorQuickOpen::_text_changed(const String &p_newtext) {
	_update_search();
}

void ScriptEditorQuickOpen::_update_search() {
	search_options->clear();
	TreeItem *root = search_options->create_item();

	for (int i = 0; i < functions.size(); i++) {
		const String entry = functions[i];
		// Entries are "name:line:kind"; only the name belongs in the list.
		const String name = entry.get_slicec(':', 0);
		if ((search_box->get_text().is_empty() || name.containsn(search_box->get_text()))) {
			TreeItem *ti = search_options->create_item(root);
			ti->set_text(0, name);
			ti->set_metadata(0, entry);
			if (root->get_first_child() == ti) {
				ti->select(0);
			}
		}
	}

	get_ok_button()->set_disabled(root->get_first_child() == nullptr);
}

void ScriptEditorQuickOpen::_confirmed() {
	TreeItem *ti = search_options->get_selected();
	if (!ti) {
		return;
	}
	int line = ti->get_metadata(0).operator String().get_slicec(':', 1).to_int();
	if (line < 1) {
		return;
	}

	emit_signal(SNAME("goto_line"), line - 1);
	hide();
}

void ScriptEditorQuickOpen::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			connect(SceneStringName(confirmed), callable_mp(this, &ScriptEditorQuickOpen::_confirmed));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			disconnect(SceneStringName(confirmed), callable_mp(this, &ScriptEditorQuickOpen::_confirmed));
		} break;
	}
}

void ScriptEditorQuickOpen::_bind_methods() {
	ADD_SIGNAL(MethodInfo("goto_line", PropertyInfo(Variant::INT, "line")));
}

ScriptEditorQuickOpen::ScriptEditorQuickOpen() {
	set_ok_button_text(TTRC("Open"));
	get_ok_button()->set_disabled(true);
	set_hide_on_ok(false);

	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);

	search_box = memnew(FilterLineEdit);
	vbc->add_margin_child(TTRC("Search:"), search_box);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &ScriptEditorQuickOpen::_text_changed));
	register_text_enter(search_box);

	search_options = memnew(Tree);
	search_box->set_forward_control(search_options);
	search_options->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	search_options->set_hide_root(true);
	search_options->set_hide_folding(true);
	search_options->add_theme_constant_override("draw_guides", 1);
	vbc->add_margin_child(TTRC("Matches:"), search_options, true);
	search_options->connect("item_activated", callable_mp(this, &ScriptEditorQuickOpen::_confirmed));
}

/////////////////////////////////

void DocumentList::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			filter->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			item_list->set_fixed_icon_size(Vector2i(1, 1) * get_theme_constant("class_icon_size", EditorStringName(Editor)));

			const Ref<Texture2D> &help_icon = get_editor_theme_icon(SNAME("Help"));
			for (ItemData &item : items) {
				Control *editor = item.editor;
				ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
				EditorHelp *eh = Object::cast_to<EditorHelp>(editor);
				if (seb) {
					item.icon = seb->get_theme_icon();
				} else if (eh) {
					item.icon = help_icon;
				}
			}
			_update_item_list();
		} break;
	}
}

Variant DocumentList::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	Node *cur_node = document_editor_container->tab_container->get_current_tab_control();
	if (cur_node == nullptr) {
		return Variant();
	}

	HBoxContainer *drag_preview = memnew(HBoxContainer);
	String preview_name;
	Ref<Texture2D> preview_icon;

	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(cur_node);
	if (seb) {
		preview_name = seb->get_document_name();
		preview_icon = seb->get_theme_icon();
	}
	EditorHelp *eh = Object::cast_to<EditorHelp>(cur_node);
	if (eh) {
		preview_name = eh->get_class();
		preview_icon = get_editor_theme_icon(SNAME("Help"));
	}

	if (preview_icon.is_valid()) {
		TextureRect *tf = memnew(TextureRect);
		tf->set_texture(preview_icon);
		tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
		drag_preview->add_child(tf);
	}
	Label *label = memnew(Label(preview_name));
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // Don't translate script names and class names.
	drag_preview->add_child(label);
	set_drag_preview(drag_preview);

	Dictionary drag_data;
	drag_data["type"] = "script_list_element";
	drag_data["script_list_element"] = cur_node;
	drag_data["editor"] = document_editor_container->config_section;

	return drag_data;
}

bool DocumentList::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary d = p_data;
	const String type = d.get("type", "");

	if (type == "script_list_element") {
		if (d["editor"] != document_editor_container->config_section) {
			return false;
		}
		Node *node = Object::cast_to<Node>(d["script_list_element"]);

		if (Object::cast_to<ScriptEditorBase>(node) || Object::cast_to<EditorHelp>(node)) {
			return true;
		}
	}

	if (type == "files") {
		Vector<String> files = d["files"];

		if (files.is_empty()) {
			return false; // Weird.
		}

		for (const String &file : files) {
			if (document_editor_container->can_open_file(file)) {
				return true;
			}
		}
		return false;
	}

	return false;
}

void DocumentList::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	Dictionary d = p_data;
	const String type = d.get("type", "");

	if (type == "script_list_element") {
		Node *node = Object::cast_to<Node>(d["script_list_element"]);

		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(node);
		EditorHelp *eh = Object::cast_to<EditorHelp>(node);
		if (seb || eh) {
			int new_index = 0;
			if (item_list->get_item_count() > 0) {
				int pos = 0;
				if (p_point == Vector2(Math::INF, Math::INF)) {
					if (item_list->is_anything_selected()) {
						pos = item_list->get_selected_items()[0];
					}
				} else {
					pos = item_list->get_item_at_position(p_point);
				}
				new_index = item_list->get_item_metadata(pos);
			}
			TabContainer *tab_container = document_editor_container->tab_container;
			tab_container->move_child(node, new_index);
			tab_container->set_current_tab(new_index);
			update_list();
		}
	}

	if (type == "files") {
		Vector<String> files = d["files"];

		int new_index = 0;
		if (item_list->get_item_count() > 0) {
			int pos = 0;
			if (p_point == Vector2(Math::INF, Math::INF)) {
				if (item_list->is_anything_selected()) {
					pos = item_list->get_selected_items()[0];
				}
			} else {
				pos = item_list->get_item_at_position(p_point);
			}
			new_index = item_list->get_item_metadata(pos);
		}
		TabContainer *tab_container = document_editor_container->tab_container;
		int num_tabs_before = tab_container->get_tab_count();
		for (const String &file : files) {
			if (!document_editor_container->can_open_file(file)) {
				continue;
			}

			Ref<Resource> res = document_editor_container->open_file(file);
			if (res.is_valid()) {
				const int num_tabs = tab_container->get_tab_count();
				if (num_tabs > num_tabs_before) {
					tab_container->move_child(tab_container->get_tab_control(tab_container->get_tab_count() - 1), new_index);
					num_tabs_before = num_tabs;
				} else if (num_tabs > 0) { // Maybe script was already open.
					tab_container->move_child(tab_container->get_current_tab_control(), new_index);
				}
			}
		}
		if (tab_container->get_tab_count() > 0) {
			tab_container->set_current_tab(new_index);
		}
		update_list();
	}
}

void DocumentList::_document_selected(int p_index) {
	Control *editor = Object::cast_to<Control>(item_list->get_item_metadata(p_index));
	if (editor) {
		document_editor_container->_activate_document(editor, true);
	}
}

void DocumentList::_document_clicked(int p_index, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index == MouseButton::MIDDLE) {
		_document_selected(p_index);
		document_editor_container->_menu_option(DocumentEditorContainer::FILE_MENU_CLOSE);
	}

	if (p_mouse_button_index == MouseButton::RIGHT) {
		_show_context_menu();
	}
}

void DocumentList::_show_context_menu() {
	context_menu->clear();

	// The menu acts on the current document, which lives in the active pane.
	TabContainer *pane = document_editor_container->_active_pane_container();
	if (pane == nullptr) {
		return;
	}

	int selected = pane->get_current_tab();
	if (selected < 0) {
		return;
	}

	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane->get_tab_control(selected));
	const Ref<Resource> res = seb ? seb->get_edited_resource() : Ref<Resource>();
	const Ref<Script> script = res;
	{
		if (seb) {
			context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/save"), DocumentEditorContainer::FILE_MENU_SAVE);
			context_menu->add_separator();
		}
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_file"), DocumentEditorContainer::FILE_MENU_CLOSE);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_other_tabs"), DocumentEditorContainer::FILE_MENU_CLOSE_OTHER_TABS);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_tabs_below"), DocumentEditorContainer::FILE_MENU_CLOSE_TABS_BELOW);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_all"), DocumentEditorContainer::FILE_MENU_CLOSE_ALL);
		if (document_editor_container->is_main_editor) {
			context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_docs"), DocumentEditorContainer::FILE_MENU_CLOSE_DOCS);
		}
		context_menu->add_separator();

		if (document_editor_container->is_main_editor && script.is_valid() && script->is_tool()) {
			context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/reload_script_soft"), DocumentEditorContainer::FILE_MENU_SOFT_RELOAD_TOOL);
			context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/run_file"), DocumentEditorContainer::FILE_MENU_RUN);
			context_menu->add_separator();
		}

		if (seb) {
			context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/copy_path"), DocumentEditorContainer::FILE_MENU_COPY_PATH);
			context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/copy_uid"), DocumentEditorContainer::FILE_MENU_COPY_UID);
			context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/show_in_file_system"), DocumentEditorContainer::FILE_MENU_SHOW_IN_FILE_SYSTEM);
			if (!document_editor_container->is_main_editor) {
				context_menu->add_shortcut(ED_SHORTCUT("script_editor/open_in_inspector", TTRC("Open File in Inspector")), DocumentEditorContainer::FILE_MENU_INSPECT);
				context_menu->add_shortcut(ED_SHORTCUT("script_editor/inspect_native_code", TTRC("Inspect Native Shader Code...")), DocumentEditorContainer::FILE_MENU_INSPECT_NATIVE_SHADER_CODE);
			}
			context_menu->add_separator();
		}

		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/move_document_up"), DocumentEditorContainer::FILE_MENU_MOVE_UP);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/move_document_down"), DocumentEditorContainer::FILE_MENU_MOVE_DOWN);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/sort_documents"), DocumentEditorContainer::FILE_MENU_SORT);
	}

	{
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_SAVE), !seb);
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_CLOSE), pane->get_tab_count() < 1);
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_CLOSE_OTHER_TABS), pane->get_tab_count() <= 1);
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_CLOSE_TABS_BELOW), pane->get_current_tab() >= pane->get_tab_count() - 1);
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_CLOSE_ALL), pane->get_tab_count() < 1);
		if (document_editor_container->is_main_editor) {
			context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_CLOSE_DOCS), !document_editor_container->_has_docs_tab());
			context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_SOFT_RELOAD_TOOL), script.is_null() || !script->is_tool());
			context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_RUN), script.is_null());
		}
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_COPY_PATH), res.is_null() || res->get_path().is_empty());
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_COPY_UID), res.is_null() || ResourceLoader::get_resource_uid(res->get_path()) == ResourceUID::INVALID_ID);
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_SHOW_IN_FILE_SYSTEM), res.is_null() || res->get_path().is_empty());
		if (!document_editor_container->is_main_editor) {
			context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_INSPECT), !seb);
			context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_INSPECT_NATIVE_SHADER_CODE), !seb);
		}
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_MOVE_UP), pane->get_current_tab() <= 0);
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_MOVE_DOWN), pane->get_current_tab() >= pane->get_tab_count() - 1);
		context_menu->set_item_disabled(context_menu->get_item_index(DocumentEditorContainer::FILE_MENU_SORT), pane->get_tab_count() <= 1);
	}

	// Context menu plugin.
	if (EditorContextMenuPluginManager::get_singleton()->has_plugins_for_slot(EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR)) {
		EditorContextMenuPlugin::OptionsData context_data = DocumentEditorContainer::get_context_data(pane->get_tab_control(selected));
		EditorContextMenuPluginManager::get_singleton()->add_options_from_plugins(context_menu, EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR, context_data);

#ifndef DISABLE_DEPRECATED
		Vector<String> selected_paths;
		Ref<Resource> current_resource = context_data["selected_resource"];
		if (current_resource.is_valid()) {
			selected_paths.push_back(current_resource->get_path());
		}
		EditorContextMenuPluginManager::get_singleton()->add_options_from_plugins(context_menu, EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR, selected_paths, current_resource, 500);
#endif
	}

	context_menu->set_position(get_screen_position() + get_local_mouse_position());
	context_menu->reset_size();
	context_menu->popup();
}

static void _find_scripts(Node *p_base, Node *p_current, HashSet<Ref<Script>> &r_used_scripts) {
	if (p_current != p_base && p_current->get_owner() != p_base) {
		return;
	}

	if (p_current->get_script_instance()) {
		Ref<Script> scr = p_current->get_script();
		if (scr.is_valid()) {
			r_used_scripts.insert(scr);
		}
	}

	for (Node *child : p_current->iterate_children()) {
		_find_scripts(p_base, child, r_used_scripts);
	}
}

void DocumentList::update_list() {
	TabContainer *tab_container = document_editor_container->tab_container;
	int num_open_editors = 0;
	for (TabContainer *pane : document_editor_container->panes) {
		num_open_editors += pane->get_tab_count();
	}
	const Ref<Texture2D> &help_icon = get_editor_theme_icon(SNAME("Help"));

	HashSet<Ref<Script>> scripts_in_scene;
	if (document_editor_container->highlight_scene_scripts && num_open_editors > 0) {
		Node *edited = EditorNode::get_singleton()->get_edited_scene();
		if (edited) {
			_find_scripts(edited, edited, scripts_in_scene);
		}
	}

	items.clear();
	items.reserve(num_open_editors);

	for (TabContainer *pane : document_editor_container->panes) {
		for (int i = 0; i < pane->get_tab_count(); i++) {
			ItemData item;
			item.editor = pane->get_tab_control(i);
			item.display_name = pane->get_tab_title(i);

			Control *editor = item.editor;
			ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
			EditorHelp *eh = Object::cast_to<EditorHelp>(editor);
			if (seb) {
				const String &path = seb->get_edited_resource()->get_path();

				item.tooltip = path.is_empty() ? TTR("Unsaved file.") : path;
				item.icon = seb->get_theme_icon();

				TextEditorBase *teb = Object::cast_to<TextEditorBase>(seb);
				if (teb) {
					item.has_error = !teb->get_validation_success();
				}

				const Ref<Script> &scr = seb->get_edited_resource();
				if (scr.is_valid()) {
					item.used_in_scene = scripts_in_scene.has(scr);
					item.tool = scr->is_tool();
				}
			}
			if (eh) {
				const String name = eh->get_class().is_empty() ? TTR("Unknown Help Class") : eh->get_class().unquote();
				const String tooltip = vformat(TTR("%s Class Reference"), name);

				item.tooltip = tooltip;
				item.icon = help_icon;
			}

			items.push_back(item);
		}
	}

	const String &filter_text = filter->get_text();
	if (!filter_text.is_empty()) {
		PackedStringArray search_names;
		for (ItemData &item : items) {
			item.hidden = true;
			search_names.append(item.display_name);
		}

		FuzzySearch fuzzy;
		fuzzy.set_case_sensitive(false);
		Vector<Ref<FuzzySearchMatch>> results = fuzzy.search_all(filter_text, search_names);

		for (const Ref<FuzzySearchMatch> &res : results) {
			items[res->get_original_index()].hidden = false;
		}
	}

	_update_item_list();
}

void DocumentList::_update_item_list() {
	item_list->clear();
	if (items.is_empty()) {
		return;
	}

	TabContainer *tab_container = document_editor_container->tab_container;
	Control *current_editor = document_editor_container->_active_pane_container()->get_current_tab_control();

	Color tool_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	tool_color.set_s(tool_color.get_s() * 1.5);
	Color used_item_bg_color = Color(0.5, 0.5, 0.5, 0.125);
	const Ref<Texture2D> &error_icon = get_editor_theme_icon(SNAME("Error"));

	for (const ItemData &item : items) {
		if (item.hidden) {
			continue;
		}
		item_list->add_item(item.display_name, item.icon);
		if (item.tool) {
			item_list->set_item_icon_modulate(-1, tool_color);
		}

		item_list->set_item_tooltip(-1, item.tooltip);
		item_list->set_item_metadata(-1, item.editor); // Keep the document editor itself, not a pane index.
		if (item.used_in_scene) {
			item_list->set_item_custom_bg_color(-1, used_item_bg_color);
		}
		if (item.has_error) {
			item_list->set_item_tag_icon(-1, error_icon);
		}
		if (item.editor == current_editor) {
			item_list->select(item_list->get_item_count() - 1);
		}
	}
	item_list->ensure_current_is_visible();

	// Update colors.
	if (!script_temperature_enabled) {
		return;
	}

	Color hot_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	hot_color.set_s(hot_color.get_s() * 0.9);
	Color cold_color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor));

	int edit_pass = document_editor_container->edit_pass;

	for (int i = 0; i < item_list->get_item_count(); i++) {
		int index = item_list->get_item_metadata(i);
		Node *open_editor = tab_container->get_tab_control(index);
		if (!open_editor) {
			continue;
		}

		int pass = open_editor->get_meta("__editor_pass", -1);
		if (pass < 0) {
			continue;
		}

		int h = edit_pass - pass;
		if (h > temperature_history_size) {
			continue;
		}
		int non_zero_hist_size = (temperature_history_size == 0) ? 1 : temperature_history_size;
		float v = Math::ease(h / float(non_zero_hist_size), 0.4);

		item_list->set_item_custom_fg_color(i, hot_color.lerp(cold_color, v));
	}
}

void DocumentList::ensure_current_is_visible() {
	item_list->ensure_current_is_visible();
}

void DocumentList::goto_next_document(bool p_previous) {
	if (item_list->get_item_count() == 0) {
		return;
	}
	int next_tab = item_list->get_current() + (p_previous ? -1 : 1);
	next_tab = Math::wrapi(next_tab, 0, item_list->get_item_count());
	Control *editor = Object::cast_to<Control>(item_list->get_item_metadata(next_tab));
	if (editor) {
		document_editor_container->_activate_document(editor, true);
	}
}

void DocumentList::update_editor_settings() {
	script_temperature_enabled = EDITOR_GET("text_editor/script_list/script_temperature_enabled");
	temperature_history_size = EDITOR_GET("text_editor/script_list/script_temperature_history_size");

	update_list();
}

DocumentList::DocumentList(DocumentEditorContainer *p_document_editor_container) {
	document_editor_container = p_document_editor_container;
	set_v_size_flags(SIZE_EXPAND_FILL);

	filter = memnew(LineEdit);
	filter->set_clear_button_enabled(true);
	filter->set_placeholder(TTRC("Filter Documents"));
	filter->set_accessibility_name(TTRC("Filter Documents"));
	filter->connect(SceneStringName(text_changed), callable_mp(this, &DocumentList::update_list).unbind(1));
	add_child(filter);

	item_list = memnew(ItemList);
	item_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	item_list->set_custom_minimum_size(Size2(60, 40) * EDSCALE);
	item_list->set_v_size_flags(SIZE_EXPAND_FILL);
	item_list->set_theme_type_variation("ItemListSecondary");
	item_list->set_allow_rmb_select(true);
	item_list->connect("item_clicked", callable_mp(this, &DocumentList::_document_clicked));
	item_list->connect(SceneStringName(item_selected), callable_mp(this, &DocumentList::_document_selected));
	add_child(item_list);
	SET_DRAG_FORWARDING_GCD(item_list, DocumentList);

	context_menu = memnew(PopupMenu);
	add_child(context_menu);
	context_menu->connect(SceneStringName(id_pressed), callable_mp(document_editor_container, &DocumentEditorContainer::_menu_option));
}

/////////////////////////////////

void DocumentOutline::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			sort_button->set_button_icon(get_editor_theme_icon(SNAME("Sort")));

			for (const KeyValue<String, Button *> &E : kind_buttons) {
				const StringName icon = E.value->get_meta("kind_icon", StringName());
				if (!icon.is_empty()) {
					E.value->set_button_icon(get_editor_theme_icon(icon));
				}
			}

			update_visibility();
		} break;
	}
}

void DocumentOutline::_tree_selected() {
	if (updating_outline) {
		return;
	}
	// The selection can be gone by the time the signal is delivered: rebuilding
	// the tree (on a filter change, or a new member list) clears it, and the
	// tree still reports the selection change. Reading it unconditionally
	// dereferenced a null item.
	TreeItem *selected = tree->get_selected();
	if (selected == nullptr) {
		return;
	}

	Control *active_editor = document_editor_container->get_active_editor();
	int line = selected->get_metadata(0);
	if (line < 0) {
		return;
	}
	if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(active_editor)) {
		// An entry's line belongs to the document it is listed for: a member
		// copied from a trait points at the `uses` statement that brought it in,
		// not at the line it is declared on in the trait's own file. An entry
		// whose line this document does not have is not worth jumping to: the
		// caret would be clamped to the last line and land somewhere unrelated.
		CodeTextEditor *code_editor = teb->get_code_editor();
		if (code_editor == nullptr || line >= code_editor->get_text_editor()->get_line_count()) {
			return;
		}
		teb->goto_line_centered(line);
	} else if (EditorHelp *eh = Object::cast_to<EditorHelp>(active_editor)) {
		eh->scroll_to_section(line);
	}
}

void DocumentOutline::_toggle_sort(bool p_alphabetic_sort) {
	EditorSettings::get_singleton()->set("text_editor/script_list/sort_members_outline_alphabetically", p_alphabetic_sort);
	update_outline();
}

void DocumentOutline::update_editor_settings() {
	members_overview_enabled = EDITOR_GET("text_editor/script_list/show_members_overview");
	help_overview_enabled = EDITOR_GET("text_editor/help/show_help_index");
	update_visibility();
}

// Icon matching the kind of member the language reported, so methods,
// properties, signals and the rest can be told apart at a glance.
void DocumentOutline::_add_kind_filter() {
	kind_filter_hbox = memnew(HBoxContainer);
	kind_filter_hbox->add_theme_constant_override("separation", 2);
	add_child(kind_filter_hbox);

	_add_kind_button("method", TTRC("Methods"), "text_editor/script_list/outline_show_methods", SNAME("MemberMethod"));
	_add_kind_button("property", TTRC("Properties"), "text_editor/script_list/outline_show_properties", SNAME("MemberProperty"));
	_add_kind_button("signal", TTRC("Signals"), "text_editor/script_list/outline_show_signals", SNAME("MemberSignal"));
	_add_kind_button("constant", TTRC("Constants"), "text_editor/script_list/outline_show_constants", SNAME("MemberConstant"));
	_add_kind_button("enum", TTRC("Enums"), "text_editor/script_list/outline_show_enums", SNAME("Enum"));
}

Button *DocumentOutline::_add_kind_button(const String &p_kind, const String &p_label, const String &p_setting, const StringName &p_icon) {
	Button *button = memnew(Button);
	button->set_theme_type_variation(SceneStringName(FlatButton));
	button->set_toggle_mode(true);
	// The theme is not ready yet while the outline is built; the icon is set
	// when the theme arrives.
	button->set_meta("kind_icon", p_icon);
	button->set_tooltip_text(vformat(TTRC("Show %s"), p_label));
	button->set_accessibility_name(p_label);
	button->set_pressed(EDITOR_GET(p_setting));
	button->connect(SceneStringName(toggled), callable_mp(this, &DocumentOutline::_toggle_kind).bind(p_kind));

	kind_filter_hbox->add_child(button);
	kind_buttons[p_kind] = button;
	kind_settings[p_kind] = p_setting;

	return button;
}

void DocumentOutline::_toggle_kind(bool p_pressed, const String &p_kind) {
	const String *setting = kind_settings.getptr(p_kind);
	if (setting == nullptr) {
		return;
	}

	EditorSettings::get_singleton()->set(*setting, p_pressed);
	update_outline();
}

bool DocumentOutline::_is_kind_visible(const String &p_kind) const {
	// Entries without a kind are always shown.
	if (p_kind.is_empty()) {
		return true;
	}

	const String *setting = kind_settings.getptr(p_kind);
	if (setting == nullptr) {
		return true;
	}
	return EDITOR_GET(*setting);
}

Ref<Texture2D> DocumentOutline::_get_member_icon(const String &p_kind) const {
	if (p_kind == "method") {
		return get_editor_theme_icon(SNAME("MemberMethod"));
	}
	if (p_kind == "property") {
		return get_editor_theme_icon(SNAME("MemberProperty"));
	}
	if (p_kind == "signal") {
		return get_editor_theme_icon(SNAME("MemberSignal"));
	}
	if (p_kind == "constant") {
		return get_editor_theme_icon(SNAME("MemberConstant"));
	}
	if (p_kind == "enum") {
		return get_editor_theme_icon(SNAME("Enum"));
	}
	return Ref<Texture2D>();
}

// Creates (or reuses) the group items a member name written as a dotted path
// belongs to, and returns the item the member itself hangs under. The map is
// keyed by the whole group path: two inner classes (or two traits) can each
// hold a group with the same name, and a plain group name would merge them.
TreeItem *DocumentOutline::_get_group_item(const Vector<String> &p_groups, HashMap<String, TreeItem *> &r_parents, const Color &p_disabled_color) {
	TreeItem *parent = tree->get_root();
	String path;
	for (const String &group : p_groups) {
		path = path.is_empty() ? group : path + "." + group;
		TreeItem *group_item = nullptr;
		if (r_parents.has(path)) {
			group_item = r_parents[path];
		}
		if (group_item == nullptr) {
			group_item = tree->create_item(parent);
			group_item->set_text(0, group);
			group_item->set_selectable(0, false);
			group_item->set_custom_color(0, p_disabled_color);
			r_parents[path] = group_item;
		}
		parent = group_item;
	}
	return parent;
}

void DocumentOutline::update_outline() {
	String selected;
	if (tree->is_anything_selected()) {
		selected = tree->get_selected()->get_meta("function");
	}

	Control *active_editor = document_editor_container->get_active_editor();
	if (current_editor != active_editor) {
		selected = ""; // Script/doc changed, forget the last selection.
	}
	current_editor = active_editor;

	updating_outline = true;

	tree->clear();
	tree->create_item();

	if (CodeEditorBase *ceb = Object::cast_to<CodeEditorBase>(active_editor)) {
		PackedStringArray functions = ceb->get_functions();
		if (EDITOR_GET("text_editor/script_list/sort_members_outline_alphabetically")) {
			functions.sort();
		}

		const String &filter_text = filter->get_text();
		HashMap<String, TreeItem *> parents;
		Color disabled_color = get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));

		if (filter_text.is_empty()) {
			for (const String &function_name : functions) {
				if (!_is_kind_visible(function_name.get_slicec(':', 2))) {
					continue;
				}

				// Organize functions inside subclasses and traits: the groups are
				// every part of the name but the member itself.
				String name = function_name.get_slicec(':', 0);
				Vector<String> groups = name.split(".");
				const int last = groups.size() - 1;
				name = groups[last];
				groups.remove_at(last);
				TreeItem *parent = _get_group_item(groups, parents, disabled_color);

				TreeItem *ti = tree->create_item(parent);
				ti->set_text(0, name);
				ti->set_icon(0, _get_member_icon(function_name.get_slicec(':', 2)));
				int line = function_name.get_slicec(':', 1).to_int() - 1;
				ti->set_metadata(0, line);
				ti->set_meta("function", function_name);
				if (selected == function_name) {
					ti->select(0);
				}
			}
		} else {
			PackedStringArray search_names;
			PackedStringArray searched;
			for (const String &function_name : functions) {
				if (!_is_kind_visible(function_name.get_slicec(':', 2))) {
					continue;
				}
				searched.push_back(function_name);
				search_names.append(function_name.get_slicec(':', 0));
			}

			FuzzySearch fuzzy;
			fuzzy.set_case_sensitive(false);
			Vector<Ref<FuzzySearchMatch>> results = fuzzy.search_all(filter_text, search_names);

			for (const Ref<FuzzySearchMatch> &res : results) {
				String function_name = searched[res->get_original_index()];

				// Organize functions inside subclasses and traits: the groups are
				// every part of the name but the member itself.
				String name = function_name.get_slicec(':', 0);
				Vector<String> groups = name.split(".");
				const int last = groups.size() - 1;
				name = groups[last];
				groups.remove_at(last);
				TreeItem *parent = _get_group_item(groups, parents, disabled_color);

				TreeItem *ti = tree->create_item(parent);
				ti->set_text(0, name);
				ti->set_icon(0, _get_member_icon(function_name.get_slicec(':', 2)));
				int line = function_name.get_slicec(':', 1).to_int() - 1;
				ti->set_metadata(0, line);
				ti->set_meta("function", function_name);
				if (selected == function_name) {
					ti->select(0);
				}
			}
		}
	} else if (EditorHelp *eh = Object::cast_to<EditorHelp>(active_editor)) {
		const Vector<Pair<String, int>> sections = eh->get_sections();
		for (const Pair<String, int> &section : sections) {
			TreeItem *ti = tree->create_item();
			ti->set_text(0, section.first);
			ti->set_metadata(0, section.second);
			ti->set_meta("function", section.first);
			if (selected == section.first) {
				ti->select(0);
			}
		}
	}
	updating_outline = false;
}

void DocumentOutline::update_visibility() {
	Control *active_editor = document_editor_container->get_active_editor();
	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(active_editor);
	EditorHelp *eh = Object::cast_to<EditorHelp>(active_editor);

	bool members_overview_visible = members_overview_enabled && seb && seb->show_members_overview();
	bool help_overview_visible = help_overview_enabled && eh;

	buttons_hbox->set_visible(members_overview_visible);
	set_visible(members_overview_visible || help_overview_visible);

	bool use_monospace_font = members_overview_visible && EDITOR_GET("interface/theme/use_monospace_font_for_editor_symbols");
	if (use_monospace_font) {
		Ref<Font> monospace_font = get_theme_font(SNAME("source"), EditorStringName(EditorFonts));
		if (tree->get_theme_font(SceneStringName(font)) != monospace_font) {
			tree->add_theme_font_override(SceneStringName(font), monospace_font);
		}
	} else if (tree->has_theme_font_override(SceneStringName(font))) {
		tree->remove_theme_font_override(SceneStringName(font));
	}
}

DocumentOutline::DocumentOutline(DocumentEditorContainer *p_document_editor_container) {
	document_editor_container = p_document_editor_container;
	// Which kinds are listed, right above the filter box.
	_add_kind_filter();

	buttons_hbox = memnew(HBoxContainer);

	filter = memnew(FilterLineEdit);
	filter->set_placeholder(TTRC("Filter Methods"));
	filter->set_accessibility_name(TTRC("Filter Methods"));
	filter->set_clear_button_enabled(true);
	filter->set_h_size_flags(SIZE_EXPAND_FILL);
	filter->connect(SceneStringName(text_changed), callable_mp(this, &DocumentOutline::update_outline).unbind(1));
	buttons_hbox->add_child(filter);

	sort_button = memnew(Button);
	sort_button->set_theme_type_variation(SceneStringName(FlatButton));
	sort_button->set_tooltip_text(TTRC("Toggle alphabetical sorting of the method list."));
	sort_button->set_toggle_mode(true);
	sort_button->set_pressed(EDITOR_GET("text_editor/script_list/sort_members_outline_alphabetically"));
	sort_button->connect(SceneStringName(toggled), callable_mp(this, &DocumentOutline::_toggle_sort));
	buttons_hbox->add_child(sort_button);

	add_child(buttons_hbox);

	tree = memnew(Tree);
	filter->set_forward_control(tree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_theme_type_variation("TreeSecondary");
	tree->add_theme_constant_override("h_separation", 0);
	tree->set_allow_reselect(true);
	tree->set_allow_rmb_select(true);
	tree->set_hide_root(true);
	tree->set_hide_folding(true);
	tree->set_custom_minimum_size(Size2(0, 40 * EDSCALE));
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->connect(SceneStringName(item_selected), callable_mp(this, &DocumentOutline::_tree_selected));
	add_child(tree);
}

/*** SCRIPT EDITOR ******/

String DocumentEditorContainer::_get_debug_tooltip(const String &p_text, Node *p_se) {
	if (EDITOR_GET("text_editor/behavior/documentation/enable_tooltips")) {
		return String();
	}

	// NOTE: See also `ScriptTextEditor::_show_symbol_tooltip()` for documentation tooltips enabled.
	String debug_value = EditorDebuggerNode::get_singleton()->get_var_value(p_text);
	if (!debug_value.is_empty()) {
		constexpr int DISPLAY_LIMIT = 1024;
		if (debug_value.size() > DISPLAY_LIMIT) {
			debug_value = debug_value.left(DISPLAY_LIMIT) + "... " + TTR("(truncated)");
		}
		debug_value = TTR("Current value: ") + debug_value;
	}

	return debug_value;
}

void DocumentEditorContainer::_resource_created(Ref<Resource> p_res) {
	EditorNode::get_singleton()->push_item(p_res.ptr());
}

void DocumentEditorContainer::_goto_script_line(Ref<RefCounted> p_script, int p_line) {
	Ref<Script> scr = Object::cast_to<Script>(*p_script);
	if (scr.is_valid() && (scr->has_source_code() || scr->get_path().is_resource_file())) {
		if (edit(p_script, p_line, 0)) {
			EditorNode::get_singleton()->push_item(p_script.ptr());
		}
	}
}

void DocumentEditorContainer::_change_execution(Ref<RefCounted> p_script, int p_line, bool p_set) {
	Ref<Script> scr = Object::cast_to<Script>(*p_script);
	if (scr.is_valid() && (scr->has_source_code() || scr->get_path().is_resource_file())) {
		for (Control *editor : _get_all_pane_editors()) {
			if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(editor)) {
				if ((scr.is_valid() && teb->get_edited_resource() == p_script) || teb->get_edited_resource()->get_path() == scr->get_path()) {
					if (p_set) {
						teb->set_executing_line(p_line);
					} else {
						teb->clear_executing_line();
					}
				}
			}
		}
	}
}

void DocumentEditorContainer::_set_breakpoint(Ref<RefCounted> p_script, int p_line, bool p_enabled) {
	Ref<Script> scr = Object::cast_to<Script>(*p_script);
	if (scr.is_valid() && (scr->has_source_code() || scr->get_path().is_resource_file())) {
		// Update if open.
		for (Control *editor : _get_all_pane_editors()) {
			CodeEditorBase *ceb = Object::cast_to<CodeEditorBase>(editor);
			if (ceb && ceb->get_edited_resource()->get_path() == scr->get_path()) {
				ceb->set_breakpoint(p_line, p_enabled);
				return;
			}
		}

		// Handle closed.
		Dictionary state = script_editor_cache->get_value(scr->get_path(), "state");
		Array breakpoints;
		if (state.has("breakpoints")) {
			breakpoints = state["breakpoints"];
		}

		if (breakpoints.has(p_line)) {
			if (!p_enabled) {
				breakpoints.erase(p_line);
			}
		} else if (p_enabled) {
			breakpoints.push_back(p_line);
		}
		state["breakpoints"] = breakpoints;
		script_editor_cache->set_value(scr->get_path(), "state", state);
		EditorDebuggerNode::get_singleton()->set_breakpoint(scr->get_path(), p_line + 1, p_enabled);
	}
}

void DocumentEditorContainer::_clear_breakpoints() {
	for (Control *editor : _get_all_pane_editors()) {
		if (CodeEditorBase *ceb = Object::cast_to<CodeEditorBase>(editor)) {
			ceb->clear_breakpoints();
		}
	}

	// Clear from closed scripts.
	Vector<String> cached_editors = script_editor_cache->get_sections();
	for (const String &E : cached_editors) {
		Array breakpoints = _get_cached_breakpoints_for_script(E);
		for (int breakpoint : breakpoints) {
			EditorDebuggerNode::get_singleton()->set_breakpoint(E, (int)breakpoint + 1, false);
		}

		if (breakpoints.size() > 0) {
			Dictionary state = script_editor_cache->get_value(E, "state");
			state["breakpoints"] = Array();
			script_editor_cache->set_value(E, "state", state);
		}
	}
}

Array DocumentEditorContainer::_get_cached_breakpoints_for_script(const String &p_path) const {
	if (!ResourceLoader::exists(p_path, "Script") || p_path.begins_with("local://") || !script_editor_cache->has_section_key(p_path, "state")) {
		return Array();
	}

	Dictionary state = script_editor_cache->get_value(p_path, "state");
	if (!state.has("breakpoints")) {
		return Array();
	}
	return state["breakpoints"];
}

ScriptEditorBase *DocumentEditorContainer::_get_current_editor() const {
	return Object::cast_to<ScriptEditorBase>(_active_pane_container()->get_current_tab_control());
}

void DocumentEditorContainer::_go_to_tab(int p_idx, bool p_save_history) {
	if (restoring_layout) {
		return;
	}
	if (ScriptEditorBase *current = _get_current_editor()) {
		if (current->is_unsaved()) {
			current->apply_code();
		}
	}

	Control *c = tab_container->get_tab_control(p_idx);
	if (!c) {
		return;
	}

	tab_container->set_current_tab(p_idx);

	c = tab_container->get_current_tab_control();

	if (ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(c)) {
		if (is_visible_in_tree() && !grab_focus_block) {
			seb->ensure_focus();
		}

		TextEditorBase *teb = Object::cast_to<TextEditorBase>(c);
		if (p_save_history && teb) {
			ScriptEditorNavigationMarker::get_singleton()->locate_begin();
			teb->trigger_history_save_on_navigate();
			ScriptEditorNavigationMarker::get_singleton()->locate_end();
		}

		Ref<Script> scr = seb->get_edited_resource();
		if (scr.is_valid()) {
			ScriptEditor::get_singleton()->notify_script_changed(scr);
		}

		seb->validate_script();
	}

	if (EditorHelp *eh = Object::cast_to<EditorHelp>(c)) {
		if (is_visible_in_tree()) {
			eh->set_focused();
		}

		if (p_save_history) {
			ScriptEditorNavigationMarker::get_singleton()->locate_begin();
			eh->trigger_history_save_on_navigate();
			ScriptEditorNavigationMarker::get_singleton()->locate_end();
		}
	}

	c->set_meta("__editor_pass", ++edit_pass);
	_update_history_arrows();
}

// Configures a pane for side-by-side editing: visible tab bar, per-tab close
// buttons and tabs that can be dragged onto the other pane. Panes sharing the
// same rearrange group exchange tabs (with their contents) natively.
void DocumentEditorContainer::_setup_pane(TabContainer *p_pane) {
	ERR_FAIL_NULL(p_pane);

	p_pane->set_tabs_visible(true);
	p_pane->set_drag_to_rearrange_enabled(true);
	p_pane->set_tabs_rearrange_group(PANES_REARRANGE_GROUP);

	p_pane->connect("tab_selected", callable_mp(this, &DocumentEditorContainer::_pane_tab_selected).bind(p_pane));
	p_pane->connect("tab_changed", callable_mp(this, &DocumentEditorContainer::_pane_tab_changed).bind(p_pane));
	p_pane->connect("active_tab_rearranged", callable_mp(this, &DocumentEditorContainer::_pane_tab_rearranged).bind(p_pane));

	// Per-tab close buttons and the tab context menu belong to the tab bar. The close button policy, the
	// maximum tab width and the icon width follow the same settings as the scene tabs, so both tab strips
	// behave and look the same.
	if (TabBar *bar = p_pane->get_tab_bar()) {
		bar->set_tab_close_display_policy((TabBar::CloseButtonDisplayPolicy)EDITOR_GET("interface/scene_tabs/display_close_button").operator int());
		bar->set_max_tab_width(int(EDITOR_GET("interface/scene_tabs/maximum_width")) * EDSCALE);
		bar->connect("tab_close_pressed", callable_mp(this, &DocumentEditorContainer::_pane_tab_close_pressed).bind(p_pane));
		bar->connect("tab_rmb_clicked", callable_mp(this, &DocumentEditorContainer::_show_tab_context_menu).bind(p_pane));
	}
}

void DocumentEditorContainer::_update_tab_bar_settings() {
	const TabBar::CloseButtonDisplayPolicy close_policy = (TabBar::CloseButtonDisplayPolicy)EDITOR_GET("interface/scene_tabs/display_close_button").operator int();
	const int max_width = int(EDITOR_GET("interface/scene_tabs/maximum_width")) * EDSCALE;
	const int icon_max_width = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
	for (TabContainer *pane : panes) {
		if (TabBar *bar = pane->get_tab_bar()) {
			bar->set_tab_close_display_policy(close_policy);
			bar->set_max_tab_width(max_width);
			bar->add_theme_constant_override("icon_max_width", icon_max_width);
		}
	}
}

void DocumentEditorContainer::_update_pane_top_spacing() {
	if (pane_top_spacer == nullptr) {
		return;
	}
	// The Expressive tab lift draws five pixels above the tab, so leave the same room above the panes.
	// Subtle and disabled animations keep a zero-height spacer (pixel-identical layout).
	const int spacing = EditorThemeManager::is_expressive_animation_style() ? 5 * EDSCALE : 0;
	pane_top_spacer->set_custom_minimum_size(Size2(0, spacing));
}

void DocumentEditorContainer::_pane_tab_selected(int p_tab, TabContainer *p_pane) {
	_set_active_pane(p_pane);
	ensure_select_current();
}

void DocumentEditorContainer::_pane_tab_changed(int p_tab, TabContainer *p_pane) {
	// Tabs may have been dragged in or out of this pane: refresh what depends
	// on the panes' contents, and drop a split pane that became empty. The
	// emptiness check is deferred so tabs can finish moving during a drop.
	callable_mp(this, &DocumentEditorContainer::_check_split_pane_empty).call_deferred();
	_update_filenames();
	document_list->update_list();
}

void DocumentEditorContainer::_pane_tab_rearranged(int p_tab, TabContainer *p_pane) {
	// A tab was reordered or moved to another pane: keep titles and the
	// document list in sync.
	callable_mp(this, &DocumentEditorContainer::_check_split_pane_empty).call_deferred();
	_update_filenames();
	document_list->update_list();
}

void DocumentEditorContainer::_pane_tab_close_pressed(int p_tab, TabContainer *p_pane) {
	Control *editor = p_pane->get_tab_control(p_tab);
	if (editor == nullptr) {
		return;
	}

	// Make the tab being closed the current document, so the shared close
	// flows (save prompt, discard, save-then-close) act on it.
	p_pane->set_current_tab(p_tab);
	_set_active_pane(p_pane);

	_request_close_tab(p_pane, p_tab);
}

// Right-clicking a tab offers the usual document actions for that tab, on
// either pane, and the way to move it between the panes.
void DocumentEditorContainer::_show_tab_context_menu(int p_tab, TabContainer *p_pane) {
	if (p_tab < 0 || p_tab >= p_pane->get_tab_count()) {
		return;
	}

	tab_context_pane_id = p_pane->get_instance_id();
	tab_context_index = p_tab;

	// Act on the tab that was clicked, like the rest of the editor would.
	p_pane->set_current_tab(p_tab);
	_set_active_pane(p_pane);

	const Control *editor = p_pane->get_tab_control(p_tab);
	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(const_cast<Control *>(editor));
	const Ref<Resource> res = (seb != nullptr) ? seb->get_edited_resource() : Ref<Resource>();

	tab_context_menu->clear();
	tab_context_menu->add_item(TTRC("Close"), TAB_MENU_CLOSE);
	tab_context_menu->add_item(TTRC("Close Others"), TAB_MENU_CLOSE_OTHERS);
	tab_context_menu->add_separator();
	tab_context_menu->add_item(TTRC("Copy Path"), TAB_MENU_COPY_PATH);
	tab_context_menu->add_separator();
	if (_can_add_pane()) {
		// The tab is moved into a new pane, on the right or below this one.
		tab_context_menu->add_item(TTRC("Split Right"), TAB_MENU_SPLIT_RIGHT);
		tab_context_menu->add_item(TTRC("Split Down"), TAB_MENU_SPLIT_DOWN);
	}
	if (panes.size() == 2) {
		// With exactly two panes, the tab goes to the other one.
		tab_context_menu->add_item(TTRC("Move to Other Pane"), TAB_MENU_MOVE_TO_OTHER_PANE);
	}
	tab_context_menu->add_separator();
	tab_context_menu->add_item(TTRC("Open in New Window"), TAB_MENU_OPEN_IN_WINDOW);

	tab_context_menu->set_item_disabled(tab_context_menu->get_item_index(TAB_MENU_CLOSE_OTHERS), p_pane->get_tab_count() <= 1);
	tab_context_menu->set_item_disabled(tab_context_menu->get_item_index(TAB_MENU_COPY_PATH), res.is_null() || res->get_path().is_empty());

	tab_menu_hovered_id = -1;
	set_process(true);
	tab_context_menu->set_position(DisplayServer::get_singleton()->mouse_get_position());
	tab_context_menu->popup();
}

// Takes the document of a tab out of its pane and gives it a window of its
// own. The pane it came from is dropped if it is left empty.
void DocumentEditorContainer::_popout_editor(TabContainer *p_pane, int p_index) {
	if (p_pane == nullptr || p_index < 0 || p_index >= p_pane->get_tab_count()) {
		return;
	}

	Control *editor = p_pane->get_tab_control(p_index);
	if (editor == nullptr) {
		return;
	}

	const String title = p_pane->get_tab_title(p_index);

	Window *window = memnew(Window);
	window->set_title(title);
	window->set_size(Size2i(900, 600));
	window->set_min_size(Size2i(200, 150));
	window->set_meta("script_editor_document", true);
	window->connect("close_requested", callable_mp(this, &DocumentEditorContainer::_on_popout_close_requested).bind(window));
	add_child(window);

	p_pane->remove_child(editor);
	window->add_child(editor);
	editor->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	popout_windows.push_back(window->get_instance_id());
	window->popup_centered();

	_check_split_pane_empty();
	_update_filenames();
	document_list->update_list();
}

void DocumentEditorContainer::_on_popout_close_requested(Window *p_window) {
	// The pane that is focused right now is where the document belongs. It is
	// taken before the window goes away, since closing it moves the focus back
	// into the editor and that can change the focused pane.
	TabContainer *target = _active_pane_container();
	const ObjectID target_id = (target != nullptr) ? target->get_instance_id() : ObjectID();

	// The window has to go away first, and the document returns on the next
	// frame: moving it around inside the close request leaves the window stuck.
	p_window->hide();
	callable_mp(this, &DocumentEditorContainer::_return_popout).call_deferred(p_window, target_id);
}

// Puts the document back into the given pane (the focused one when the window
// was closed) and drops the window.
void DocumentEditorContainer::_return_popout(Window *p_window, ObjectID p_target_pane) {
	if (p_window == nullptr) {
		return;
	}

	TabContainer *target = Object::cast_to<TabContainer>(ObjectDB::get_instance(p_target_pane));
	if (target == nullptr || _pane_index(target) < 0) {
		// The pane is gone: fall back to whatever is focused now.
		target = _active_pane_container();
	}

	Control *editor = nullptr;
	for (int i = 0; i < p_window->get_child_count(); i++) {
		editor = Object::cast_to<Control>(p_window->get_child(i));
		if (editor != nullptr) {
			break;
		}
	}

	if (editor != nullptr && target != nullptr) {
		p_window->remove_child(editor);

		target->add_child(editor);
		target->set_current_tab(target->get_tab_count() - 1);
		editor->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	}

	popout_windows.erase(p_window->get_instance_id());
	p_window->queue_free();

	_update_filenames();
	document_list->update_list();
}

void DocumentEditorContainer::_update_popout_windows() {
	// Window titles follow the documents while they are renamed or saved.
	for (const ObjectID &window_id : popout_windows) {
		Window *window = Object::cast_to<Window>(ObjectDB::get_instance(window_id));
		if (window == nullptr) {
			continue;
		}

		Control *editor = nullptr;
		for (int i = 0; i < window->get_child_count(); i++) {
			editor = Object::cast_to<Control>(window->get_child(i));
			if (editor != nullptr) {
				break;
			}
		}
		if (editor == nullptr) {
			continue;
		}

		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
		if (seb != nullptr) {
			window->set_title(seb->get_document_name());
		}
	}
}

void DocumentEditorContainer::_tab_menu_option(int p_option) {
	// The menu is asynchronous: the pane it was opened on, and the tab it was
	// opened for, may be gone by the time an entry is picked.
	TabContainer *pane = _tab_context_pane();
	if (pane == nullptr || _pane_index(pane) < 0 || tab_context_index < 0 || tab_context_index >= pane->get_tab_count()) {
		return;
	}

	switch (p_option) {
		case TAB_MENU_CLOSE: {
			_request_close_tab(pane, tab_context_index);
		} break;
		case TAB_MENU_CLOSE_OTHERS: {
			script_close_queue_pane = pane->get_instance_id();
			for (int i = pane->get_tab_count() - 1; i >= 0; i--) {
				if (i != tab_context_index) {
					script_close_queue.push_back(i);
				}
			}
			_queue_close_tabs();
		} break;
		case TAB_MENU_COPY_PATH: {
			Control *editor = pane->get_tab_control(tab_context_index);
			ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
			if (seb != nullptr) {
				const Ref<Resource> res = seb->get_edited_resource();
				if (res.is_valid() && !res->get_path().is_empty()) {
					DisplayServer::get_singleton()->clipboard_set(res->get_path());
				}
			}
		} break;
		case TAB_MENU_SPLIT_RIGHT:
		case TAB_MENU_SPLIT_DOWN: {
			const DropZone zone = (p_option == TAB_MENU_SPLIT_DOWN) ? DROP_ZONE_BOTTOM : DROP_ZONE_RIGHT;
			const int index = tab_context_index;
			// The menu was built when the split was possible, but the panes can
			// have changed while it was open: only a pane the split really made
			// takes the tab, never the last one around.
			TabContainer *new_pane = _split_pane(pane, zone);
			if (new_pane != nullptr) {
				_move_tab_to_pane(pane, index, new_pane);
			}
		} break;
		case TAB_MENU_OPEN_IN_WINDOW: {
			_popout_editor(pane, tab_context_index);
		} break;
		case TAB_MENU_MOVE_TO_OTHER_PANE: {
			if (TabContainer *to = _other_pane(pane)) {
				_move_tab_to_pane(pane, tab_context_index, to);
			}
		} break;
		default:
			break;
	}
}

// The tab menu reports keyboard navigation, not the pointer, so the hovered
// entry is polled while the menu is open.
void DocumentEditorContainer::_update_tab_menu_highlight() {
	if (tab_context_menu == nullptr || !tab_context_menu->is_visible()) {
		return;
	}

	// get_focused_item() reports the entry the pointer (or keyboard) is on.
	const int index = tab_context_menu->get_focused_item();
	const int id = (index >= 0) ? tab_context_menu->get_item_id(index) : -1;
	if (id == tab_menu_hovered_id) {
		return;
	}
	tab_menu_hovered_id = id;
	_tab_menu_option_focused(id);
}

// The menu closed: the entry the pointer was on is not hovered anymore, and the
// highlight it drew has to go with it.
void DocumentEditorContainer::_tab_context_menu_hidden() {
	tab_menu_hovered_id = -1;
	_hide_drop_overlay();
}

void DocumentEditorContainer::_tab_menu_option_focused(int p_option) {
	TabContainer *pane = _tab_context_pane();
	switch (p_option) {
		case TAB_MENU_SPLIT_RIGHT: {
			_show_highlight_overlay(_get_drop_overlay_rect(DROP_ZONE_RIGHT), TTRC("New pane"));
		} break;
		case TAB_MENU_SPLIT_DOWN: {
			_show_highlight_overlay(_get_drop_overlay_rect(DROP_ZONE_BOTTOM), TTRC("New pane"));
		} break;
		case TAB_MENU_MOVE_TO_OTHER_PANE: {
			TabContainer *to = _other_pane(pane);
			if (to != nullptr) {
				_show_highlight_overlay(to->get_global_rect(), TTRC("Other pane"));
			} else {
				// The other pane is not there (any more): nothing to point at.
				_hide_drop_overlay();
			}
		} break;
		default: {
			_hide_drop_overlay();
		} break;
	}
}

void DocumentEditorContainer::_move_tab_to_pane(TabContainer *p_from, int p_index, TabContainer *p_to) {
	if (p_from == nullptr || p_to == nullptr || p_from == p_to || p_index < 0 || p_index >= p_from->get_tab_count()) {
		return;
	}

	_hide_drop_overlay();
	p_to->move_tab_from_tab_container(p_from, p_index, p_to->get_tab_count());
	p_to->set_current_tab(p_to->get_tab_count() - 1);
	_set_active_pane(p_to);

	_update_filenames();
	document_list->update_list();

	// A pane emptied by the move is not needed anymore.
	callable_mp(this, &DocumentEditorContainer::_check_split_pane_empty).call_deferred();
}

// Closing a tab may need to ask about unsaved changes; both the tab close
// button and the tab menu go through here.
void DocumentEditorContainer::_request_close_tab(TabContainer *p_pane, int p_idx) {
	if (p_pane == nullptr || p_idx < 0 || p_idx >= p_pane->get_tab_count()) {
		return;
	}

	p_pane->set_current_tab(p_idx);
	_set_active_pane(p_pane);

	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(p_pane->get_tab_control(p_idx));
	if (seb != nullptr && seb->is_unsaved()) {
		_ask_close_current_unsaved_tab(seb);
	} else {
		_close_tab(p_pane, p_idx, false);
	}
}

void DocumentEditorContainer::_check_split_pane_empty() {
	if (restoring_layout) {
		// Panes are empty on purpose while a layout is being rebuilt.
		return;
	}

	// Panes holding nothing are dropped, so an action that moves the documents
	// out of a pane never leaves an empty area behind.
	LocalVector<TabContainer *> empty;
	for (TabContainer *pane : panes) {
		if (pane->get_tab_count() == 0) {
			empty.push_back(pane);
		}
	}
	for (TabContainer *pane : empty) {
		_remove_pane(pane);
	}
}

void DocumentEditorContainer::_update_history_arrows() {
	script_back->set_disabled(history_pos <= 0);
	script_forward->set_disabled(history_pos >= history.size() - 1);
}

void DocumentEditorContainer::_save_new_history(const Dictionary &p_state, Control *p_control) {
	if (restoring_layout) {
		return;
	}

	if (history_pos >= 0 && history_pos < history.size()) {
		if (history[history_pos].control == p_control) {
			if (history[history_pos].state == p_state) {
				return;
			}
			if (p_state["row"] == history[history_pos].state["row"]) {
				_save_previous_state(p_state, p_control);
				return;
			}
		}
	}

	ScriptHistory sh;
	sh.control = p_control;
	sh.state = p_state;

	history.resize(history_pos + 1);
	history.push_back(sh);
	_compress_history_patterns(true);

	for (int i = history.size() - 1; i >= 0; i--) {
		if (history[i].control == tab_container->get_current_tab_control()) {
			history_pos = i;
			break;
		}
	}

	_update_history_arrows();
}

void DocumentEditorContainer::_save_previous_state(const Dictionary &p_state, Control *p_control) {
	if (history_pos < 0 || history_pos >= history.size()) {
		return;
	}
	ScriptHistory sh = history[history_pos];
	if (sh.control != p_control) {
		return;
	}
	history.write[history_pos].state = p_state;
}

// Compress the history and remove duplicate patterns.
// Example 1: If the history is ...ABAB..., it will be compressed to ...AB....
// Example 2: If the history is ...ABCABC..., it will be compressed to ...ABC....
void DocumentEditorContainer::_compress_history_patterns(bool p_once) {
	bool stop = false;
	bool changed = true;
	int iterations = 0;

	while (!stop && changed && history.size() > 1 && iterations++ < 100) {
		changed = false;
		for (int end_idx = history.size() - 1; end_idx >= 1; end_idx--) {
			bool found_duplicate = false;
			int max_possible_len = (end_idx + 1) / 2;

			for (int len = 1; len <= max_possible_len; len++) {
				// Compare [first_start, first_start + len) and [second_start, second_start + len)
				int second_start = end_idx - len + 1;
				int first_start = second_start - len;

				if (first_start < 0) {
					continue;
				}

				bool is_match = true;

				for (int k = 0; k < len; k++) {
					const ScriptHistory &h1 = history[first_start + k];
					const ScriptHistory &h2 = history[second_start + k];

					if (h1.control != h2.control || h1.state["row"] != h2.state["row"]) {
						is_match = false;
						break;
					}
				}

				if (is_match) {
					for (int r = 0; r < len; r++) {
						history.remove_at(first_start);
					}

					if (history_pos >= second_start) {
						history_pos -= len;
					} else if (first_start <= history_pos && history_pos < second_start) {
						history_pos = first_start;
					}

					found_duplicate = true;
					changed = true;
					stop = p_once;
					break;
				}
			}

			if (found_duplicate) {
				break;
			}
		}
	}

	if (history.is_empty()) {
		history_pos = -1;
	} else {
		if (history_pos >= history.size()) {
			history_pos = history.size() - 1;
		}
		if (history_pos < -1) {
			history_pos = -1;
		}
	}
}

TabContainer *DocumentEditorContainer::_pane_at(int p_index) const {
	if (p_index < 0 || p_index >= (int)panes.size()) {
		return panes.is_empty() ? nullptr : panes[0];
	}
	return panes[p_index];
}

TabContainer *DocumentEditorContainer::_active_pane_container() const {
	return _pane_at(active_pane);
}

TabContainer *DocumentEditorContainer::_tab_context_pane() const {
	return Object::cast_to<TabContainer>(ObjectDB::get_instance(tab_context_pane_id));
}

// The pane a "move to the other pane" action targets: the other pane when there
// are exactly two and the given pane is one of them. The number of panes alone
// does not say which pane is the other one, nor that the given pane still is
// one of them.
TabContainer *DocumentEditorContainer::_other_pane(TabContainer *p_pane) const {
	if (p_pane == nullptr || panes.size() != 2) {
		return nullptr;
	}
	if (p_pane == panes[0]) {
		return panes[1];
	}
	if (p_pane == panes[1]) {
		return panes[0];
	}
	return nullptr;
}

int DocumentEditorContainer::_pane_index(const TabContainer *p_pane) const {
	for (uint32_t i = 0; i < panes.size(); i++) {
		if (panes[i] == p_pane) {
			return (int)i;
		}
	}
	return -1;
}

void DocumentEditorContainer::_set_active_pane(TabContainer *p_pane) {
	const int index = _pane_index(p_pane);
	if (index >= 0) {
		active_pane = index;
	}
}

bool DocumentEditorContainer::_can_add_pane() const {
	return panes.size() < (uint32_t)MAX_PANES;
}

TabContainer *DocumentEditorContainer::_pane_under_mouse() const {
	Viewport *viewport = get_viewport();
	if (viewport == nullptr) {
		return nullptr;
	}

	// Panes are the leaves of the layout tree and never overlap, so the first
	// one holding the pointer is the one the drop belongs to.
	const Vector2 mouse = viewport->get_mouse_position();
	for (TabContainer *pane : panes) {
		if (pane->is_visible_in_tree() && pane->get_global_rect().has_point(mouse)) {
			return pane;
		}
	}
	return nullptr;
}

TabContainer *DocumentEditorContainer::_pane_of(Control *p_editor) const {
	// The editor may sit in a nested pane, so walk up to the pane holding it.
	for (Node *node = p_editor; node != nullptr; node = node->get_parent()) {
		TabContainer *pane = Object::cast_to<TabContainer>(node);
		if (pane != nullptr && _pane_index(pane) >= 0) {
			return pane;
		}
	}
	return nullptr;
}

LocalVector<Control *> DocumentEditorContainer::_get_all_pane_editors() const {
	LocalVector<Control *> editors;
	for (TabContainer *pane : panes) {
		for (int i = 0; i < pane->get_tab_count(); i++) {
			editors.push_back(pane->get_tab_control(i));
		}
	}
	return editors;
}

void DocumentEditorContainer::_activate_document(Control *p_editor, bool p_focus) {
	if (restoring_layout) {
		return;
	}
	TabContainer *pane = _pane_of(p_editor);
	if (pane == nullptr) {
		return;
	}
	if (ScriptEditorBase *current = _get_current_editor()) {
		if (current->is_unsaved()) {
			current->apply_code();
		}
	}

	int idx = pane->get_tab_idx_from_control(p_editor);
	if (idx < 0) {
		return;
	}

	pane->set_current_tab(idx);
	_set_active_pane(pane);

	Control *c = pane->get_current_tab_control();

	if (ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(c)) {
		if (is_visible_in_tree() && !grab_focus_block && p_focus) {
			seb->ensure_focus();
		}

		TextEditorBase *teb = Object::cast_to<TextEditorBase>(c);
		if (teb) {
			ScriptEditorNavigationMarker::get_singleton()->locate_begin();
			teb->trigger_history_save_on_navigate();
			ScriptEditorNavigationMarker::get_singleton()->locate_end();
		}

		Ref<Script> scr = seb->get_edited_resource();
		if (scr.is_valid()) {
			ScriptEditor::get_singleton()->notify_script_changed(scr);
		}

		seb->validate_script();
	}

	if (EditorHelp *eh = Object::cast_to<EditorHelp>(c)) {
		if (is_visible_in_tree()) {
			eh->set_focused();
		}

		ScriptEditorNavigationMarker::get_singleton()->locate_begin();
		eh->trigger_history_save_on_navigate();
		ScriptEditorNavigationMarker::get_singleton()->locate_end();
	}

	c->set_meta("__editor_pass", ++edit_pass);
	_update_history_arrows();
}

// Splits the given pane, putting a new empty pane on the requested side of it.
// The panes form a tree of split containers, so any pane can be split again
// until the maximum number of panes is reached. Returns the new pane, or null
// when no split was made.
TabContainer *DocumentEditorContainer::_split_pane(TabContainer *p_target, DropZone p_zone) {
	return Object::cast_to<TabContainer>(_split_control(p_target, p_zone));
}

// Wraps a control of the pane tree in a new split holding it and a new empty
// pane, and returns that pane. Returns nullptr when the split cannot be made.
Control *DocumentEditorContainer::_split_control(Control *p_target, DropZone p_zone) {
	if (p_target == nullptr || p_zone == DROP_ZONE_NONE || !_can_add_pane()) {
		return nullptr;
	}

	const bool below = (p_zone == DROP_ZONE_BOTTOM);
	TabContainer *new_pane = memnew(TabContainer);
	new_pane->set_custom_minimum_size(below ? Size2(0, 100) * EDSCALE : Size2(200, 0) * EDSCALE);
	new_pane->set_h_size_flags(SIZE_EXPAND_FILL);
	new_pane->set_v_size_flags(SIZE_EXPAND_FILL);

	// The split takes the place of the pane it splits, and holds both panes.
	SplitContainer *split = below ? (SplitContainer *)memnew(VSplitContainer) : (SplitContainer *)memnew(HSplitContainer);
	split->set_h_size_flags(SIZE_EXPAND_FILL);
	split->set_v_size_flags(SIZE_EXPAND_FILL);
	split->connect(SceneStringName(resized), callable_mp(this, &DocumentEditorContainer::_update_drop_overlay_rect));

	Node *parent = p_target->get_parent();
	parent->add_child(split);
	parent->move_child(split, p_target->get_index());

	p_target->reparent(split);
	split->add_child(new_pane);

	panes.push_back(new_pane);
	_setup_pane(new_pane);
	active_pane = (int)panes.size() - 1;

	_update_filenames();
	document_list->update_list();

	return new_pane;
}

// Drops a pane, collapsing the split that held it so the layout stays a tree
// of panes. The last pane always stays; the primary one is replaced by another
// pane when it goes, so no empty area is left behind.
void DocumentEditorContainer::_remove_pane(TabContainer *p_pane) {
	const int index = _pane_index(p_pane);
	if (index < 0 || panes.size() <= 1) {
		return;
	}

	SplitContainer *split = Object::cast_to<SplitContainer>(p_pane->get_parent());
	Control *remaining = nullptr;
	if (split != nullptr) {
		for (int i = 0; i < split->get_child_count(); i++) {
			Control *child = Object::cast_to<Control>(split->get_child(i));
			if (child != nullptr && child != p_pane) {
				remaining = child;
				break;
			}
		}
	}

	panes.remove_at(index);

	if (split != nullptr) {
		split->remove_child(p_pane);

		if (remaining != nullptr && split != editor_split) {
			// Left with a single child: the split is replaced by it.
			Node *grandparent = split->get_parent();
			const int split_index = split->get_index();
			split->remove_child(remaining);
			grandparent->add_child(remaining);
			grandparent->move_child(remaining, split_index);
			split->queue_free();
		}
	} else {
		p_pane->get_parent()->remove_child(p_pane);
	}
	p_pane->queue_free();

	if (index == 0) {
		// The primary pane is simply the first one, so another takes its place.
		tab_container = panes[0];
		active_pane = 0;
	} else if (active_pane >= index) {
		active_pane = MAX(0, active_pane - 1);
	}

	// The tab menu and the queued closes hold this pane or its tabs, which are
	// about to be freed: forget them instead of keeping a stale reference.
	const ObjectID removed_id = p_pane->get_instance_id();
	if (tab_context_pane_id == removed_id) {
		tab_context_pane_id = ObjectID();
		tab_context_index = -1;
	}
	if (script_close_queue_pane == removed_id) {
		script_close_queue_pane = ObjectID();
		script_close_queue.clear();
	}

	_update_filenames();
	document_list->update_list();
}

// Moves every document back into the primary pane and drops the extra panes.
void DocumentEditorContainer::_merge_all_panes() {
	if (panes.size() <= 1) {
		return;
	}

	TabContainer *target = panes[0];
	LocalVector<TabContainer *> extras;
	for (uint32_t i = 1; i < panes.size(); i++) {
		extras.push_back(panes[i]);
	}

	for (TabContainer *pane : extras) {
		while (pane->get_tab_count() > 0) {
			target->move_tab_from_tab_container(pane, 0, target->get_tab_count());
		}
	}
	for (TabContainer *pane : extras) {
		_remove_pane(pane);
	}

	active_pane = 0;
	_update_filenames();
	document_list->update_list();
}

// Dragging a document tab into the highlighted area on the right of the editor
// splits the view there, like dragging a tab to the side of a code editor.
// Only the overlay covering that area is a drop target, so drops on the tabs
// themselves keep using the built-in tab rearrangement.
bool DocumentEditorContainer::_can_drop_split_tab(const Vector2 &p_at_position, const Variant &p_data) const {
	if (!_can_add_pane() || p_data.get_type() != Variant::DICTIONARY) {
		return false;
	}

	const Dictionary data = p_data;
	return String(data.get("type", String())) == "tab" && String(data.get("tab_type", String())) == "tab_container_tab";
}

// Called every frame of a drag: it keeps the hint and the inline preview in
// step with where the drag currently is.
void DocumentEditorContainer::_update_drag_feedback() {
	Viewport *viewport = get_viewport();
	if (viewport == nullptr) {
		return;
	}

	if (!viewport->gui_is_dragging()) {
		// A tab released outside the panes becomes a window of its own. The pane
		// and the document are looked up by id: either of them may have gone
		// while the drag was in progress.
		if (pending_popout_pane.is_valid()) {
			TabContainer *pane = Object::cast_to<TabContainer>(ObjectDB::get_instance(pending_popout_pane));
			Control *editor = Object::cast_to<Control>(ObjectDB::get_instance(pending_popout_editor));
			pending_popout_pane = ObjectID();
			pending_popout_editor = ObjectID();

			if (pane != nullptr && editor != nullptr) {
				// The document may have been dropped somewhere else already.
				const int index = (editor->get_parent() == pane) ? pane->get_tab_idx_from_control(editor) : -1;
				if (index >= 0 && _pane_under_mouse() == nullptr) {
					_popout_editor(pane, index);
				}
			}
		}
		_hide_drop_overlay();
		return;
	}

	const Variant drag_data = viewport->gui_get_drag_data();
	if (drag_data.get_type() != Variant::DICTIONARY) {
		_hide_drop_overlay();
		return;
	}

	const Dictionary data = drag_data;
	const String type = data.get("type", String());

	// Dragging a document tab: offer to split the pane under the pointer, to
	// the right or below depending on which half it is over.
	if (type == "tab" && String(data.get("tab_type", String())) == "tab_container_tab") {
		_hide_code_preview();
		TabContainer *pane = _pane_under_mouse();
		if (pane == nullptr) {
			// Dragged out of the panes: releasing it opens the document in a
			// window of its own.
			TabBar *from_bar = Object::cast_to<TabBar>(get_node_or_null(NodePath(String(data.get("from_path", String())))));
			TabContainer *from = (from_bar != nullptr) ? TabContainer::get_tab_bar_container(from_bar) : nullptr;
			const int from_index = data.get("tab_index", -1);
			if (from != nullptr && from_index >= 0 && from_index < from->get_tab_count()) {
				pending_popout_pane = from->get_instance_id();
				pending_popout_editor = from->get_tab_control(from_index)->get_instance_id();
				_show_highlight_overlay(Rect2(get_global_rect().position, get_global_rect().size), TTRC("Release to open in a new window"));
			} else {
				pending_popout_pane = ObjectID();
				pending_popout_editor = ObjectID();
				_hide_split_overlay();
			}
			return;
		}

		pending_popout_pane = ObjectID();
		pending_popout_editor = ObjectID();

		if (!_can_add_pane()) {
			_hide_split_overlay();
			return;
		}

		const Vector2 mouse = viewport->get_mouse_position();
		const Rect2 pane_rect = pane->get_global_rect();
		const bool below = mouse.y > pane_rect.position.y + pane_rect.size.y * 0.5f;
		const Rect2 half = below
				? Rect2(pane_rect.position + Point2(0, pane_rect.size.y * 0.5f), Size2(pane_rect.size.x, pane_rect.size.y * 0.5f))
				: Rect2(pane_rect.position + Point2(pane_rect.size.x * 0.5f, 0), Size2(pane_rect.size.x * 0.5f, pane_rect.size.y));
		_show_drop_overlay_rect(half, below ? DROP_ZONE_BOTTOM : DROP_ZONE_RIGHT, TTRC("Drop here to split the view"));
		return;
	}

	// Drops the editor turns into code are previewed inline, in the editor the
	// drop would go to.
	_hide_split_overlay();
	_update_drop_preview();
}

bool DocumentEditorContainer::_can_drop_on_editor(const Vector2 &p_at_position, const Variant &p_data) const {
	switch (drop_overlay_zone) {
		case DROP_ZONE_RIGHT:
		case DROP_ZONE_BOTTOM:
			return _can_drop_split_tab(p_at_position, p_data);

		default:
			return false;
	}
}

void DocumentEditorContainer::_drop_on_editor(const Vector2 &p_at_position, const Variant &p_data) {
	switch (drop_overlay_zone) {
		case DROP_ZONE_RIGHT:
		case DROP_ZONE_BOTTOM:
			_drop_split_tab(p_at_position, p_data);
			break;

		default:
			break;
	}
}

// Shows the text the current drop would insert, as translucent text at the
// position the drop would use. It is asked for again every frame while dragging
// because holding Ctrl or Alt changes what the drop inserts, and it is only
// shown over the editor the drop would actually go to.
void DocumentEditorContainer::_update_drop_preview() {
	Viewport *viewport = get_viewport();
	if (viewport == nullptr || !viewport->gui_is_dragging()) {
		_hide_code_preview();
		return;
	}

	// The drop goes to the editor under the mouse, so preview that one; moving
	// the drag off the code (over the scene tree, the file list, ...) drops the
	// preview.
	const Vector2 mouse = viewport->get_mouse_position();
	ScriptEditorBase *target = nullptr;
	CodeEdit *code_edit = nullptr;
	for (Control *editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
		if (seb == nullptr) {
			continue;
		}
		CodeEdit *code = Object::cast_to<CodeEdit>(seb->get_base_editor());
		// Use the editor panel, not just its text area: the drop is accepted
		// over the whole panel, including its margins and line number bar.
		if (code != nullptr && seb->is_visible_in_tree() && seb->get_global_rect().has_point(mouse)) {
			target = seb;
			code_edit = code;
			break;
		}
	}

	if (target == nullptr) {
		_hide_code_preview();
		return;
	}

	const String preview = target->get_drop_preview_text(viewport->gui_get_drag_data());
	if (preview.is_empty()) {
		_hide_code_preview();
		return;
	}

	if (drop_code_preview == nullptr) {
		drop_code_preview = memnew(Label);
		drop_code_preview->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		drop_code_preview->set_autowrap_mode(TextServer::AUTOWRAP_OFF);
	}
	if (drop_code_preview->get_parent() != code_edit) {
		if (drop_code_preview->get_parent() != nullptr) {
			drop_code_preview->get_parent()->remove_child(drop_code_preview);
		}
		code_edit->add_child(drop_code_preview);
		// Match the editor's code font so the preview sits exactly in the text.
		drop_code_preview->add_theme_font_override(SceneStringName(font), code_edit->get_theme_font(SceneStringName(font)));
		drop_code_preview->add_theme_font_size_override(SceneStringName(font_size), code_edit->get_theme_font_size(SceneStringName(font_size)));
	}

	drop_code_preview->set_text(preview);
	drop_code_preview->show();

	// The preview belongs to a line of the editor: keep it aligned while the
	// editor scrolls, and hide it only when that line is fully out of view.
	// Partly visible lines still count, which matters at the bottom edge where
	// the last line usually is not fully visible.
	const Point2i drop_pos = code_edit->get_line_column_at_pos(code_edit->get_local_mouse_position());
	const Rect2i drop_rect = code_edit->get_rect_at_line_column(drop_pos.y, drop_pos.x);
	const int line_height = code_edit->get_line_height();
	if (drop_rect.position.y + line_height <= 0 || drop_rect.position.y >= code_edit->get_size().y) {
		drop_code_preview->hide();
		return;
	}

	const Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	drop_code_preview->set_modulate(Color(accent.r, accent.g, accent.b, 0.7f));
	drop_code_preview->set_position(drop_rect.position);
}

void DocumentEditorContainer::_hide_code_preview() {
	if (drop_code_preview != nullptr) {
		drop_code_preview->hide();
	}
}

Dictionary DocumentEditorContainer::_get_layout_presets() const {
	if (!EditorSettings::get_singleton()) {
		return Dictionary();
	}
	return EditorSettings::get_singleton()->get_project_metadata("script_editor_layouts", config_section, Dictionary());
}

void DocumentEditorContainer::_set_layout_presets(const Dictionary &p_presets) {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	EditorSettings::get_singleton()->set_project_metadata("script_editor_layouts", config_section, p_presets);
}

// Fills the layout submenu with the saved layouts, from scratch every time.
void DocumentEditorContainer::_update_layout_presets_menu() {
	if (layout_presets_menu == nullptr) {
		return;
	}

	layout_presets_menu->clear();
	layout_preset_ids.clear();

	Vector<String> names;
	const Dictionary presets = _get_layout_presets();
	for (const KeyValue<Variant, Variant> &E : presets) {
		names.push_back(E.key);
	}
	names.sort();

	if (names.is_empty()) {
		layout_presets_menu->add_item(TTRC("No saved layouts"), -1);
		layout_presets_menu->set_item_disabled(0, true);
		return;
	}

	const int base_id = 200000;
	for (int i = 0; i < names.size(); i++) {
		const int id = base_id + i;
		layout_preset_ids.insert(id, names[i]);
		layout_presets_menu->add_item(names[i], id);
	}
}

void DocumentEditorContainer::_update_layout_save_ok() {
	if (layout_save_dialog == nullptr || layout_name_edit == nullptr) {
		return;
	}
	layout_save_dialog->get_ok_button()->set_disabled(layout_name_edit->get_text().strip_edges().is_empty());
}

void DocumentEditorContainer::_update_layout_save_ok_unused(const String &p_text) {
	_update_layout_save_ok();
}

void DocumentEditorContainer::_show_layout_save_dialog() {
	if (layout_save_dialog == nullptr) {
		return;
	}
	layout_name_edit->clear();
	_update_layout_save_ok();
	layout_save_dialog->popup_centered();
	layout_name_edit->grab_focus();
}

void DocumentEditorContainer::_save_layout_preset(const String &p_name) {
	const String name = p_name.strip_edges();
	if (name.is_empty()) {
		return;
	}

	Dictionary presets = _get_layout_presets();
	presets[name] = _serialize_pane_layout(editor_split);
	_set_layout_presets(presets);

	EditorToaster::get_singleton()->popup_str(vformat(TTR("Saved layout \"%s\"."), name), EditorToaster::SEVERITY_INFO);
}

void DocumentEditorContainer::_apply_layout_preset(const String &p_name) {
	const Dictionary presets = _get_layout_presets();
	if (!presets.has(p_name)) {
		return;
	}

	// Start from a single pane, so applying a layout cannot pile panes up.
	_merge_all_panes();

	restoring_layout = true;
	_restore_pane_layout(presets[p_name], tab_container);
	restoring_layout = false;

	_update_filenames();
	document_list->update_list();
}

void DocumentEditorContainer::_show_layout_delete_dialog() {
	if (layout_delete_dialog == nullptr) {
		return;
	}

	layout_delete_list->clear();
	Vector<String> names;
	const Dictionary presets = _get_layout_presets();
	for (const KeyValue<Variant, Variant> &E : presets) {
		names.push_back(E.key);
	}
	names.sort();
	for (const String &name : names) {
		layout_delete_list->add_item(name);
	}

	_update_layout_delete_ok();
	layout_delete_dialog->popup_centered();
}

void DocumentEditorContainer::_delete_layout_preset(const String &p_name) {
	Dictionary presets = _get_layout_presets();
	if (!presets.has(p_name)) {
		return;
	}
	presets.erase(p_name);
	_set_layout_presets(presets);
	_update_layout_presets_menu();
}

void DocumentEditorContainer::_update_layout_delete_ok() {
	if (layout_delete_dialog == nullptr || layout_delete_list == nullptr) {
		return;
	}
	layout_delete_dialog->get_ok_button()->set_disabled(layout_delete_list->get_selected_items().is_empty());
}

void DocumentEditorContainer::_on_layout_delete_selected(int p_index) {
	_update_layout_delete_ok();
}

void DocumentEditorContainer::_layout_preset_selected(int p_id) {
	const String *name = layout_preset_ids.getptr(p_id);
	if (name != nullptr) {
		_apply_layout_preset(*name);
	}
}

void DocumentEditorContainer::_on_save_layout_confirmed() {
	if (layout_name_edit != nullptr) {
		_save_layout_preset(layout_name_edit->get_text());
	}
}

void DocumentEditorContainer::_on_delete_layout_confirmed() {
	if (layout_delete_list == nullptr) {
		return;
	}
	const PackedInt32Array selected = layout_delete_list->get_selected_items();
	if (!selected.is_empty()) {
		_delete_layout_preset(layout_delete_list->get_item_text(selected[0]));
	}
	_update_layout_delete_ok();
}

bool DocumentEditorContainer::_has_text_selection() const {
	TextEditorBase *teb = Object::cast_to<TextEditorBase>(_get_current_editor());
	CodeEdit *code_edit = (teb != nullptr) ? Object::cast_to<CodeEdit>(teb->get_base_editor()) : nullptr;
	return code_edit != nullptr && code_edit->has_selection();
}

void DocumentEditorContainer::_show_save_snippet_dialog() {
	if (snippet_save_dialog == nullptr || !_has_text_selection()) {
		return;
	}

	snippet_prefix_edit->clear();
	_update_snippet_dialog_ok();
	snippet_save_dialog->popup_centered();
	snippet_prefix_edit->grab_focus();
}

void DocumentEditorContainer::_update_snippet_dialog_ok(const String &p_text) {
	if (snippet_save_dialog == nullptr || snippet_prefix_edit == nullptr) {
		return;
	}

	// A snippet needs a prefix to be typed.
	const bool empty = snippet_prefix_edit->get_text().strip_edges().is_empty();
	snippet_save_dialog->get_ok_button()->set_disabled(empty);
}

void DocumentEditorContainer::_on_save_snippet_confirmed() {
	if (snippet_prefix_edit == nullptr) {
		return;
	}
	_save_selection_as_snippet(snippet_prefix_edit->get_text());
}

// Writes the selected code into the project's snippets as a new snippet, so it
// can be expanded by prefix like any other snippet.
void DocumentEditorContainer::_save_selection_as_snippet(const String &p_prefix) {
	const String prefix = p_prefix.strip_edges();
	if (prefix.is_empty() || !_has_text_selection()) {
		return;
	}

	TextEditorBase *teb = Object::cast_to<TextEditorBase>(_get_current_editor());
	CodeEdit *code_edit = (teb != nullptr) ? Object::cast_to<CodeEdit>(teb->get_base_editor()) : nullptr;
	if (code_edit == nullptr) {
		return;
	}
	const String body = code_edit->get_selected_text();

	const String dir = EditorPaths::get_singleton()->get_project_snippets_dir();
	DirAccess::make_dir_recursive_absolute(dir);
	const String path = dir.path_join("user_snippets.json");

	// Keep whatever the file already holds; it may have been written before.
	Dictionary data;
	if (FileAccess::exists(path)) {
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
		if (file.is_valid()) {
			JSON json;
			if (json.parse(file->get_as_text()) != OK || json.get_data().get_type() != Variant::DICTIONARY) {
				// Never overwrite a file that cannot be understood.
				error_dialog->set_text(vformat(TTR("Could not save the snippet: \"%s\" is not valid JSON."), path));
				error_dialog->popup_centered();
				return;
			}
			data = json.get_data();
		}
	}

	Dictionary snippet;
	snippet["prefix"] = prefix;
	snippet["body"] = body;
	data[prefix] = snippet;

	Ref<FileAccess> out = FileAccess::open(path, FileAccess::WRITE);
	if (out.is_null()) {
		error_dialog->set_text(vformat(TTR("Could not write \"%s\"."), path));
		error_dialog->popup_centered();
		return;
	}
	out->store_string(JSON::stringify(data, "\t"));
	out->flush();

	if (snippets_changed) {
		snippets_changed();
	}

	EditorToaster::get_singleton()->popup_str(vformat(TTR("Saved snippet \"%s\"."), prefix), EditorToaster::SEVERITY_INFO);
}

void DocumentEditorContainer::_show_drop_overlay(DropZone p_zone, const String &p_hint) {
	if (split_drop_overlay == nullptr || p_zone == DROP_ZONE_NONE) {
		return;
	}

	drop_overlay_zone = p_zone;
	_apply_drop_overlay(_get_drop_overlay_rect(p_zone), p_hint, true);
}

void DocumentEditorContainer::_show_drop_overlay_rect(const Rect2 &p_rect, DropZone p_zone, const String &p_hint) {
	if (split_drop_overlay == nullptr) {
		return;
	}

	drop_overlay_zone = p_zone;
	_apply_drop_overlay(p_rect, p_hint, true);
}

void DocumentEditorContainer::_show_highlight_overlay(const Rect2 &p_rect, const String &p_hint) {
	drop_overlay_zone = DROP_ZONE_NONE;
	_apply_drop_overlay(p_rect, p_hint, false);
}

namespace {
// Drop overlay transition parameters. Kept together so the values can be compared with the animation
// budget in one place instead of being scattered across the three transition helpers.
constexpr double SPLIT_OVERLAY_FADE_IN_SUBTLE = 0.12;
constexpr double SPLIT_OVERLAY_FADE_IN_EXPRESSIVE = 0.18;
constexpr double SPLIT_OVERLAY_FADE_OUT_SUBTLE = 0.10;
constexpr double SPLIT_OVERLAY_FADE_OUT_EXPRESSIVE = 0.12;
constexpr float SPLIT_OVERLAY_SCALE_EXPRESSIVE = 0.98f;
constexpr double SPLIT_OVERLAY_MORPH_SUBTLE = 0.14;
constexpr double SPLIT_OVERLAY_MORPH_EXPRESSIVE = 0.18;
constexpr float SPLIT_OVERLAY_MORPH_MAX_OVERSHOOT_PX = 4.0f; // Multiplied by EDSCALE at the use site.
constexpr float SPLIT_OVERLAY_BACK_PEAK_OVERSHOOT = 0.1003f; // Peak overshoot of BACK with EASE_OUT.
} // namespace

void DocumentEditorContainer::_apply_drop_overlay(const Rect2 &p_rect, const String &p_hint, bool p_accept_drops) {
	if (split_drop_overlay == nullptr) {
		return;
	}

	// Highlights that only hint at a target must not take drags over. This also restores the filter
	// immediately when the overlay is shown again while it is still fading out.
	split_drop_overlay->set_mouse_filter(p_accept_drops ? Control::MOUSE_FILTER_STOP : Control::MOUSE_FILTER_IGNORE);
	if (split_drop_hint != nullptr) {
		split_drop_hint->set_text(p_hint);
	}
	if (split_drop_box != nullptr) {
		split_drop_box->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	}

	// The animation master switch takes priority over every transition: when it is off, a morph that is
	// still running has to stop and snap to the target rectangle instead of finishing on its own.
	const bool animations_enabled = EDITOR_GET("interface/theme/ui_animations");
	if (!animations_enabled) {
		_stop_split_overlay_morph_tween();
		split_drop_overlay->set_position(p_rect.position);
		split_drop_overlay->set_size(p_rect.size);
		_update_drop_overlay_pivot();
		split_overlay_target_rect = p_rect;
		split_overlay_target_zone = drop_overlay_zone;
		split_drop_overlay->show();
		_stop_split_overlay_tween();
		split_overlay_shown = true;
		split_drop_overlay->set_modulate(Color(1, 1, 1, 1));
		split_drop_overlay->set_scale(Vector2(1, 1));
		return;
	}

	// The drop zone itself is logical and updates immediately: releasing while the overlay is morphing
	// still lands in the zone under the pointer. Only the visual rectangle is tweened, and only when the
	// zone swaps between the right and bottom halves (a discrete change), never while the pointer moves
	// inside one zone.
	const DropZone previous_zone = split_overlay_target_zone;
	const bool zone_changed = split_overlay_shown &&
			((previous_zone == DROP_ZONE_RIGHT && drop_overlay_zone == DROP_ZONE_BOTTOM) ||
					(previous_zone == DROP_ZONE_BOTTOM && drop_overlay_zone == DROP_ZONE_RIGHT));
	const bool same_target = split_overlay_target_zone == drop_overlay_zone && split_overlay_target_rect.is_equal_approx(p_rect);

	const bool was_visible = split_drop_overlay->is_visible();
	if (split_overlay_morph_tween.is_valid() && same_target) {
		// Keep feeding the running morph; restarting it would stutter.
	} else if (zone_changed && split_overlay_target_rect.intersects(p_rect)) {
		// Right and bottom halves of the same pane overlap around the center, so the pointer stays inside
		// the moving rectangle. Rects from different panes do not overlap: snap there instead, so the drop
		// target always stays under the pointer.
		_start_split_overlay_morph_tween(p_rect);
	} else {
		_stop_split_overlay_morph_tween();
		split_drop_overlay->set_position(p_rect.position);
		split_drop_overlay->set_size(p_rect.size);
		_update_drop_overlay_pivot();
	}
	split_overlay_target_rect = p_rect;
	split_overlay_target_zone = drop_overlay_zone;
	split_drop_overlay->show();

	if (split_overlay_shown) {
		// Already appearing (or shown) during this drag: only the target rectangle changed, which was
		// applied above. Replaying the entrance here would make the overlay flicker while moving. The
		// entrance tween releases its own reference once it finishes.
		return;
	}
	split_overlay_shown = true;

	const bool expressive = EditorThemeManager::is_expressive_animation_style();
	// Reappearing in the middle of the fade-out continues from the current alpha instead of jumping.
	const float start_alpha = was_visible ? split_drop_overlay->get_modulate().a : 0.0f;
	split_drop_overlay->set_modulate(Color(1, 1, 1, start_alpha));
	if (!expressive) {
		split_drop_overlay->set_scale(Vector2(1, 1));
	} else if (!was_visible) {
		split_drop_overlay->set_scale(Vector2(SPLIT_OVERLAY_SCALE_EXPRESSIVE, SPLIT_OVERLAY_SCALE_EXPRESSIVE));
	}
	_start_split_overlay_tween(true);
}

void DocumentEditorContainer::_update_drop_overlay_pivot() {
	if (split_drop_overlay == nullptr) {
		return;
	}
	// Scaling happens around the center of the current rectangle.
	split_drop_overlay->set_pivot_offset(split_drop_overlay->get_size() * 0.5f);
}

void DocumentEditorContainer::_stop_split_overlay_tween() {
	if (split_overlay_tween.is_valid()) {
		split_overlay_tween->kill();
	}
	split_overlay_tween = Ref<Tween>();
}

void DocumentEditorContainer::_start_split_overlay_tween(bool p_show) {
	if (split_drop_overlay == nullptr) {
		return;
	}
	_stop_split_overlay_tween();

	const bool expressive = EditorThemeManager::is_expressive_animation_style();
	split_overlay_tween = Ref<Tween>(create_tween());
	if (split_overlay_tween.is_null()) {
		// No tween available (not in a scene tree): fall back to showing/hiding instantly.
		split_drop_overlay->set_modulate(Color(1, 1, 1, 1));
		split_drop_overlay->set_scale(Vector2(1, 1));
		if (!p_show) {
			split_drop_overlay->hide();
		}
		return;
	}
	if (p_show) {
		// Subtle only fades; Expressive also springs from slightly smaller than the final size.
		split_overlay_tween->set_trans(expressive ? Tween::TRANS_BACK : Tween::TRANS_SINE);
		split_overlay_tween->set_ease(Tween::EASE_OUT);
		split_overlay_tween->set_parallel(true);
		split_overlay_tween->tween_property(split_drop_overlay, NodePath("modulate:a"), 1.0, expressive ? SPLIT_OVERLAY_FADE_IN_EXPRESSIVE : SPLIT_OVERLAY_FADE_IN_SUBTLE);
		if (expressive) {
			split_overlay_tween->tween_property(split_drop_overlay, NodePath("scale"), Vector2(1, 1), SPLIT_OVERLAY_FADE_IN_EXPRESSIVE);
		}
		// Release the reference when the entrance ends, so the member only points at a transition that is
		// actually in flight (a finished Tween is kept alive by the scene tree, not by this Ref).
		split_overlay_tween->set_parallel(false);
		split_overlay_tween->tween_callback(callable_mp(this, &DocumentEditorContainer::_split_overlay_entrance_finished));
	} else {
		// Leaving always stays brisk and linear-ish, whatever the style: no overshoot on the way out.
		split_overlay_tween->set_trans(Tween::TRANS_SINE);
		split_overlay_tween->set_ease(Tween::EASE_OUT);
		split_overlay_tween->tween_property(split_drop_overlay, NodePath("modulate:a"), 0.0, expressive ? SPLIT_OVERLAY_FADE_OUT_EXPRESSIVE : SPLIT_OVERLAY_FADE_OUT_SUBTLE);
		split_overlay_tween->tween_callback(callable_mp(this, &DocumentEditorContainer::_split_overlay_fade_out_finished));
	}
}

void DocumentEditorContainer::_split_overlay_entrance_finished() {
	// The entrance tween ends here; drop the reference so it does not look like a transition is still
	// running. The scene tree keeps the finished tween alive until its cleanup pass.
	split_overlay_tween = Ref<Tween>();
}

void DocumentEditorContainer::_stop_split_overlay_morph_tween() {
	if (split_overlay_morph_tween.is_valid()) {
		split_overlay_morph_tween->kill();
	}
	split_overlay_morph_tween = Ref<Tween>();
}

void DocumentEditorContainer::_start_split_overlay_morph_tween(const Rect2 &p_rect) {
	if (split_drop_overlay == nullptr) {
		return;
	}
	_stop_split_overlay_morph_tween();

	const bool expressive = EditorThemeManager::is_expressive_animation_style();
	split_overlay_morph_tween = Ref<Tween>(create_tween());
	if (split_overlay_morph_tween.is_null()) {
		// No tween available (not in a scene tree): apply the rectangle instantly.
		split_drop_overlay->set_position(p_rect.position);
		split_drop_overlay->set_size(p_rect.size);
		_update_drop_overlay_pivot();
		return;
	}

	// The new tween starts from the rectangle the overlay currently draws, so quickly swapping back and
	// forth between the zones continues from where it is instead of queueing animations.
	const Rect2 from(split_drop_overlay->get_position(), split_drop_overlay->get_size());
	real_t back_amplitude = 0.0f;
	if (expressive) {
		// BACK overshoots by about 10% of the travelled distance. On a large right/bottom swap that would
		// fling the highlight tens of pixels over the neighboring docks, so the overshoot is capped to a
		// few pixels while keeping the BACK curve shape.
		const real_t max_delta = MAX(MAX(Math::abs(p_rect.position.x - from.position.x), Math::abs(p_rect.position.y - from.position.y)),
				MAX(Math::abs(p_rect.size.x - from.size.x), Math::abs(p_rect.size.y - from.size.y)));
		const real_t max_overshoot = SPLIT_OVERLAY_MORPH_MAX_OVERSHOOT_PX * EDSCALE;
		back_amplitude = CLAMP(max_overshoot / (SPLIT_OVERLAY_BACK_PEAK_OVERSHOOT * MAX(max_delta, 1.0f)), 0.0f, 1.0f);
	}

	// Both the position and the size (and the fitting pivot) are driven by the same progress value, so the
	// overlay changes shape and place as one motion.
	split_overlay_morph_tween->tween_method(callable_mp(this, &DocumentEditorContainer::_set_split_overlay_morph_progress).bind(from, p_rect, back_amplitude), 0.0, 1.0, expressive ? SPLIT_OVERLAY_MORPH_EXPRESSIVE : SPLIT_OVERLAY_MORPH_SUBTLE);
}

void DocumentEditorContainer::_set_split_overlay_morph_progress(real_t p_progress, const Rect2 &p_from, const Rect2 &p_to, real_t p_back_amplitude) {
	if (split_drop_overlay == nullptr) {
		return;
	}

	// SINE for the Subtle style; a capped BACK for Expressive. Amplitude 0 means BACK is not used at all.
	real_t eased = p_progress;
	if (p_back_amplitude > 0.0f) {
		const real_t raw = Tween::run_equation(Tween::TRANS_BACK, Tween::EASE_OUT, p_progress, 0.0, 1.0, 1.0);
		eased = 1.0f + (raw - 1.0f) * p_back_amplitude;
	} else {
		eased = Tween::run_equation(Tween::TRANS_SINE, Tween::EASE_OUT, p_progress, 0.0, 1.0, 1.0);
	}

	const Rect2 rect(p_from.position.lerp(p_to.position, eased), p_from.size.lerp(p_to.size, eased));
	split_drop_overlay->set_position(rect.position);
	split_drop_overlay->set_size(rect.size);
	_update_drop_overlay_pivot();
}

void DocumentEditorContainer::_split_overlay_fade_out_finished() {
	if (split_overlay_shown) {
		// The overlay was shown again while fading out; the entrance tween owns it now.
		return;
	}
	if (split_drop_overlay != nullptr) {
		split_drop_overlay->set_modulate(Color(1, 1, 1, 1));
		split_drop_overlay->set_scale(Vector2(1, 1));
		// Keep the hidden overlay out of the way; the next _apply_drop_overlay() sets the target filter.
		split_drop_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		split_drop_overlay->hide();
	}
	// The fade is over: release the reference instead of keeping a finished Tween around. The scene tree
	// still owns it until its cleanup pass.
	split_overlay_tween = Ref<Tween>();
}

void DocumentEditorContainer::_hide_split_overlay() {
	drop_overlay_zone = DROP_ZONE_NONE;
	split_overlay_target_zone = DROP_ZONE_NONE;
	if (split_drop_overlay == nullptr) {
		return;
	}
	_stop_split_overlay_morph_tween();
	// A fading-out overlay is no longer a drop target: release the mouse right away instead of eating
	// clicks and hover until hide() runs. _apply_drop_overlay() restores the real filter when shown again.
	split_drop_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);

	if (!split_overlay_shown) {
		// Already hidden, or already fading out (the running tween finishes the hide).
		if (!split_overlay_tween.is_valid() && split_drop_overlay->is_visible()) {
			split_drop_overlay->hide();
		}
		return;
	}
	split_overlay_shown = false;

	if (!EDITOR_GET("interface/theme/ui_animations") || !split_drop_overlay->is_visible()) {
		// Animations disabled, or nothing was drawn yet: hide instantly.
		_stop_split_overlay_tween();
		split_drop_overlay->set_modulate(Color(1, 1, 1, 1));
		split_drop_overlay->set_scale(Vector2(1, 1));
		split_drop_overlay->hide();
		return;
	}
	_start_split_overlay_tween(false);
}

void DocumentEditorContainer::_hide_drop_overlay() {
	_hide_split_overlay();
	_hide_code_preview();
}

// Applies the editor theme to the drop highlight. Theme items cannot be read
// before the editor is in the tree, so this runs on theme changes instead of
// from the constructor.
void DocumentEditorContainer::_update_drop_overlay_theme() {
	if (split_drop_overlay == nullptr) {
		return;
	}

	const Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));

	Ref<StyleBoxFlat> overlay_style = memnew(StyleBoxFlat);
	overlay_style->set_bg_color(Color(accent.r, accent.g, accent.b, 0.12f));
	overlay_style->set_border_color(Color(accent.r, accent.g, accent.b, 0.7f));
	overlay_style->set_border_width_all(2);
	overlay_style->set_corner_radius_all(4);
	split_drop_overlay->add_theme_style_override(SceneStringName(panel), overlay_style);

	if (split_drop_hint != nullptr) {
		split_drop_hint->add_theme_color_override(SceneStringName(font_color), accent);
	}
}

Rect2 DocumentEditorContainer::_get_drop_overlay_rect(DropZone p_zone) const {
	// The area available to the panes, excluding the tab bar so dragging along
	// the tabs keeps reordering them.
	const Rect2 area = editor_split->get_global_rect();
	float bar_height = 0.0f;
	if (TabBar *bar = tab_container->get_tab_bar()) {
		if (bar->is_visible()) {
			bar_height = bar->get_size().y;
		}
	}

	const Rect2 content(area.position + Point2(0, bar_height), Size2(area.size.x, MAX(0.0f, area.size.y - bar_height)));
	switch (p_zone) {
		case DROP_ZONE_RIGHT: {
			const float half = content.size.x * 0.5f;
			return Rect2(content.position + Point2(content.size.x - half, 0), Size2(half, content.size.y));
		} break;
		case DROP_ZONE_BOTTOM: {
			const float half = content.size.y * 0.5f;
			return Rect2(content.position + Point2(0, content.size.y - half), Size2(content.size.x, half));
		} break;
		case DROP_ZONE_EDITOR:
			return content;
		default:
			return Rect2();
	}
}

void DocumentEditorContainer::_update_drop_overlay_rect() {
	if (split_drop_overlay == nullptr || drop_overlay_zone == DROP_ZONE_NONE) {
		return;
	}

	// Splitting resizes are layout events: snap the highlight to the split line instead of letting an
	// animation lag behind it.
	_stop_split_overlay_morph_tween();
	const Rect2 rect = _get_drop_overlay_rect(drop_overlay_zone);
	split_drop_overlay->set_position(rect.position);
	split_drop_overlay->set_size(rect.size);
	_update_drop_overlay_pivot();
	split_overlay_target_rect = rect;
}

void DocumentEditorContainer::_drop_split_tab(const Vector2 &p_at_position, const Variant &p_data) {
	if (!_can_drop_split_tab(p_at_position, p_data)) {
		return;
	}

	const Dictionary data = p_data;
	TabBar *from_bar = Object::cast_to<TabBar>(get_node_or_null(NodePath(String(data.get("from_path", String())))));
	TabContainer *from = (from_bar != nullptr) ? TabContainer::get_tab_bar_container(from_bar) : nullptr;
	if (from == nullptr) {
		return;
	}

	const int from_index = data.get("tab_index", -1);
	if (from_index < 0 || from_index >= from->get_tab_count()) {
		return;
	}

	// The pane the pointer is over is the one being split, and the highlighted
	// half says on which side the new pane goes.
	const DropZone zone = drop_overlay_zone;
	TabContainer *target = _pane_under_mouse();
	_hide_drop_overlay();
	if (target == nullptr) {
		return;
	}

	// The drop may be refused (the pane limit was reached in the meantime): only
	// a pane the split really made takes the tab.
	TabContainer *new_pane = _split_pane(target, zone);
	if (new_pane == nullptr) {
		return;
	}

	new_pane->move_tab_from_tab_container(from, from_index, 0);
	new_pane->set_current_tab(new_pane->get_tab_count() - 1);

	_update_filenames();
	document_list->update_list();
}

void DocumentEditorContainer::_toggle_split_editor_view() {
	// With a single pane this splits the pane being edited; with several panes
	// it puts every document back into one.
	if (panes.size() <= 1) {
		_split_pane(_active_pane_container(), DROP_ZONE_RIGHT);
		return;
	}

	_merge_all_panes();
}

void DocumentEditorContainer::_add_recent_script(const String &p_path) {
	if (p_path.is_empty()) {
		return;
	}

	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", config_section, Array());
	if (rc.has(p_path)) {
		rc.erase(p_path);
	}
	rc.push_front(p_path);
	if (rc.size() > 10) {
		rc.resize(10);
	}

	EditorSettings::get_singleton()->set_project_metadata("recent_files", config_section, rc);
	_update_recent_scripts();
}

void DocumentEditorContainer::_update_recent_scripts() {
	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", config_section, Array());
	recent_scripts->clear();

	String path;
	for (int i = 0; i < rc.size(); i++) {
		path = rc[i];
		recent_scripts->add_item(path.replace("res://", ""));
	}

	recent_scripts->add_separator();
	recent_scripts->add_shortcut(ED_GET_SHORTCUT("script_editor/clear_recent"));
	recent_scripts->set_item_auto_translate_mode(-1, AUTO_TRANSLATE_MODE_ALWAYS);
	recent_scripts->set_item_disabled(-1, rc.is_empty());

	recent_scripts->reset_size();
}

void DocumentEditorContainer::_open_recent_script(int p_idx) {
	// clear button
	if (p_idx == recent_scripts->get_item_count() - 1) {
		EditorSettings::get_singleton()->set_project_metadata("recent_files", config_section, Array());
		callable_mp(this, &DocumentEditorContainer::_update_recent_scripts).call_deferred();
		return;
	}

	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", config_section, Array());
	ERR_FAIL_INDEX(p_idx, rc.size());

	String path = rc[p_idx];
	// if its not on disk its a help file or deleted
	if (FileAccess::exists(path)) {
		if (_get_recognized_extensions().find(path.get_extension())) {
			Ref<Resource> scr = ResourceLoader::load(path);
			if (scr.is_valid()) {
				edit(scr, true);
				return;
			}
		}

		Error err;
		Ref<TextFile> text_file = _load_text_file(path, &err);
		if (text_file.is_valid()) {
			edit(text_file, true);
			return;
		}
		// if it's a path then it's most likely a deleted file not help
	} else if (path.contains("::")) {
		// built-in script
		String res_path = path.get_slice("::", 0);
		EditorNode::get_singleton()->load_scene_or_resource(res_path, false, false);

		Ref<Script> scr = ResourceLoader::load(path);
		if (scr.is_valid()) {
			edit(scr, true);
			return;
		}
	} else if (!path.is_resource_file()) {
		_help_class_open(path);
		return;
	}

	rc.remove_at(p_idx);
	EditorSettings::get_singleton()->set_project_metadata("recent_files", config_section, rc);
	_update_recent_scripts();
	_show_error_dialog(path);
}

void DocumentEditorContainer::_show_error_dialog(const String &p_path) {
	error_dialog->set_text(vformat(TTR("Can't open '%s'. The file could have been moved or deleted."), p_path));
	error_dialog->popup_centered();
}

void DocumentEditorContainer::_close_tab(TabContainer *p_pane, int p_idx, bool p_save) {
	int selected = p_idx;
	if (p_pane == nullptr || selected < 0 || selected >= p_pane->get_tab_count()) {
		return;
	}

	Node *tselected = p_pane->get_tab_control(selected);

	if (ScriptEditorBase *current = Object::cast_to<ScriptEditorBase>(tselected)) {
		Ref<Resource> file = current->get_edited_resource();
		if (p_save && file.is_valid()) {
			// Do not try to save internal scripts, but prompt to save in-memory
			// scripts which are not saved to disk yet (have empty path).
			if (!file->is_built_in()) {
				save_current_script();
			}
		}
		if (file.is_valid()) {
			if (!file->get_path().is_empty()) {
				// Only saved scripts can be restored.
				previous_scripts.push_back(file->get_path());
			}

			Ref<Script> scr = file;
			if (scr.is_valid()) {
				ScriptEditor::get_singleton()->notify_script_close(scr);
			}
		}
	}

	// Roll back to previous tab instead of just previous history state
	_roll_back_to_pre_tab();

	// Remove all history states related to the closed tab.
	for (int i = 0; i < history.size(); i++) {
		if (history[i].control == tselected) {
			history.remove_at(i);
			i--;
			history_pos--;
		}
	}

	_compress_history_patterns(false);

	int idx = p_pane->get_current_tab();
	if (TextEditorBase *current = Object::cast_to<TextEditorBase>(tselected)) {
		_save_editor_state(current);
	}
	memdelete(tselected);

	if (script_close_queue.is_empty()) {
		if (idx >= p_pane->get_tab_count()) {
			idx = p_pane->get_tab_count() - 1;
		}
		if (idx >= 0) {
			if (history_pos >= 0) {
				Control *history_control = Object::cast_to<Control>(history[history_pos].control);
				idx = (history_control != nullptr && history_control->get_parent() == p_pane) ? p_pane->get_tab_idx_from_control(history_control) : -1;
			}
			if (Control *c = p_pane->get_tab_control(idx)) {
				_activate_document(c, true);
			} else {
				_update_history_arrows();
			}
		} else {
			_update_history_arrows();
		}

		document_list->update_list();
		_save_layout();
		_update_find_replace_bar();
	}

	// An emptied split pane removes itself; its documents live in the primary.
	if (p_pane->get_tab_count() == 0) {
		_remove_pane(p_pane);
	}
}

void DocumentEditorContainer::_close_current_tab(bool p_save) {
	if (ScriptEditorBase *current = _get_current_editor()) {
		Control *editor = Object::cast_to<Control>(current);
		TabContainer *pane = _pane_of(editor);
		if (pane != nullptr && editor->get_parent() == pane) {
			_close_tab(pane, pane->get_tab_idx_from_control(editor), p_save);
		}
	}
}

void DocumentEditorContainer::_close_discard_current_tab(const String &p_str) {
	ScriptEditorBase *seb = _get_current_editor();
	if (seb) {
		Ref<Script> scr = seb->get_edited_resource();
		if (scr.is_valid()) {
			scr->reload_from_file();
		}
	}
	_close_tab(_active_pane_container(), _active_pane_container()->get_current_tab(), false);
	erase_tab_confirm->hide();
}

void DocumentEditorContainer::_close_docs_tab() {
	// Help pages can be open in any pane, so they are all considered.
	for (Control *editor : _get_all_pane_editors()) {
		EditorHelp *eh = Object::cast_to<EditorHelp>(editor);
		if (eh == nullptr) {
			continue;
		}
		TabContainer *pane = _pane_of(eh);
		if (pane == nullptr || eh->get_parent() != pane) {
			continue;
		}
		_close_tab(pane, pane->get_tab_idx_from_control(eh), true);
	}
}

void DocumentEditorContainer::_copy_script_path() {
	if (ScriptEditorBase *seb = _get_current_editor()) {
		Ref<Resource> scr = seb->get_edited_resource();
		DisplayServer::get_singleton()->clipboard_set(scr->get_path());
	}
}

void DocumentEditorContainer::_copy_script_uid() {
	if (ScriptEditorBase *seb = _get_current_editor()) {
		Ref<Resource> scr = seb->get_edited_resource();
		ResourceUID::ID uid = ResourceLoader::get_resource_uid(scr->get_path());
		DisplayServer::get_singleton()->clipboard_set(ResourceUID::get_singleton()->id_to_text(uid));
	}
}

// Closing several tabs at once acts on the documents of the active pane, like
// the other actions that work on "the current document".
void DocumentEditorContainer::_close_other_tabs() {
	TabContainer *pane = _active_pane_container();
	if (pane == nullptr) {
		return;
	}
	script_close_queue_pane = pane->get_instance_id();
	int current_idx = pane->get_current_tab();
	for (int i = pane->get_tab_count() - 1; i >= 0; i--) {
		if (i != current_idx) {
			script_close_queue.push_back(i);
		}
	}
	_queue_close_tabs();
}

void DocumentEditorContainer::_close_tabs_below() {
	TabContainer *pane = _active_pane_container();
	if (pane == nullptr) {
		return;
	}
	const int current_idx = pane->get_current_tab();
	if (current_idx < 0) {
		return;
	}
	for (int i = pane->get_tab_count() - 1; i > current_idx; i--) {
		script_close_queue.push_back(i);
	}
	script_close_queue_pane = pane->get_instance_id();
	if (pane == tab_container) {
		// Record the history state before the tabs after it go away. The path
		// only makes sense for the primary pane, whose tabs _go_to_tab indexes.
		_go_to_tab(current_idx, true);
	}
	_queue_close_tabs();
}

void DocumentEditorContainer::_close_all_tabs() {
	TabContainer *pane = _active_pane_container();
	if (pane == nullptr) {
		return;
	}
	script_close_queue_pane = pane->get_instance_id();
	for (int i = pane->get_tab_count() - 1; i >= 0; i--) {
		script_close_queue.push_back(i);
	}
	_queue_close_tabs();
}

void DocumentEditorContainer::_queue_close_tabs() {
	// The queue belongs to one pane; closing several tabs at once is offered
	// from the tab menu, which can target either pane. The pane is looked up by
	// id: it may have been dropped while the queue waited on the user.
	TabContainer *pane = tab_container;
	if (script_close_queue_pane.is_valid()) {
		pane = Object::cast_to<TabContainer>(ObjectDB::get_instance(script_close_queue_pane));
		if (pane == nullptr || _pane_index(pane) < 0) {
			// The queued indices point into a pane that is gone.
			script_close_queue.clear();
			script_close_queue_pane = ObjectID();
			pane = tab_container;
		}
	}

	while (!script_close_queue.is_empty()) {
		int idx = script_close_queue.front()->get();
		script_close_queue.pop_front();

		pane->set_current_tab(idx);
		_set_active_pane(pane);
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane->get_tab_control(idx));
		if (seb) {
			// Maybe there are unsaved changes.
			if (seb->is_unsaved()) {
				_ask_close_current_unsaved_tab(seb);
				erase_tab_confirm->connect(SceneStringName(visibility_changed), callable_mp(this, &DocumentEditorContainer::_queue_close_tabs), CONNECT_ONE_SHOT);
				break;
			}
		}

		_close_current_tab(false);
	}

	if (script_close_queue.is_empty()) {
		script_close_queue_pane = ObjectID();
	}
	_update_find_replace_bar();
}

void DocumentEditorContainer::_ask_close_current_unsaved_tab(ScriptEditorBase *p_seb) {
	erase_tab_confirm->set_text(TTR("Close and save changes?") + "\n\"" + p_seb->get_document_name() + "\"");
	erase_tab_confirm->popup_centered();
}

void DocumentEditorContainer::_resave_scripts(const String &p_str) {
	apply_scripts();

	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		Ref<Resource> scr = seb->get_edited_resource();

		if (scr->is_built_in()) {
			continue; // Internal script, who cares.
		}

		_auto_format_text(seb);

		Ref<TextFile> text_file = scr;
		if (text_file.is_valid()) {
			seb->apply_code();
			_save_text_file(text_file, text_file->get_path());
			break;
		} else {
			EditorNode::get_singleton()->save_resource(scr);
		}
		seb->tag_saved_version();
	}

	disk_changed->hide();
}

void DocumentEditorContainer::_res_saved_callback(const Ref<Resource> &p_res) {
	// The document may be open in any pane, and it is the same resource there.
	for (Control *editor : _get_all_pane_editors()) {
		if (ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor)) {
			if (seb->get_edited_resource() == p_res) {
				seb->tag_saved_version();
			}
		}
	}

	if (p_res.is_valid()) {
		// In case the Resource has built-in scripts.
		_mark_built_in_scripts_as_saved(p_res->get_path());
	}

	_update_filenames();
	Ref<Script> scr = p_res;
	if (scr.is_valid()) {
		trigger_live_script_reload(scr->get_path());
	}
}

void DocumentEditorContainer::_scene_saved_callback(const String &p_path) {
	// If scene was saved, mark all built-in scripts from that scene as saved.
	_mark_built_in_scripts_as_saved(p_path);
}

void DocumentEditorContainer::_mark_built_in_scripts_as_saved(const String &p_parent_path) {
	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		Ref<Resource> edited_res = seb->get_edited_resource();
		if (!edited_res->is_built_in()) {
			continue; // External script, who cares.
		}

		if (edited_res->get_path().get_slice("::", 0) != p_parent_path) {
			continue; // Wrong scene.
		}
		seb->tag_saved_version();

		Ref<Script> scr = edited_res;
		if (scr.is_valid()) {
			trigger_live_script_reload(scr->get_path());
			clear_docs_from_script(scr);
			if (scr->is_tool()) {
				scr->get_language()->reload_tool_script(edited_res);
			} else {
				scr->get_language()->reload_scripts({ scr });
			}
			update_docs_from_script(scr);
		}
	}
}

void DocumentEditorContainer::trigger_live_script_reload(const String &p_script_path) {
	if (!script_paths_to_reload.has(p_script_path)) {
		Ref<Script> reloaded_script = ResourceCache::get_ref(p_script_path);
		if (reloaded_script.is_null()) {
			reloaded_script = ResourceLoader::load(p_script_path);
		}
		if (reloaded_script.is_valid()) {
			if (!reloaded_script->get_language()->get_editor_language()->validate(reloaded_script->get_source_code(), p_script_path, nullptr, nullptr, nullptr, nullptr)) {
				// Script has errors, don't live reload.
				return;
			}
		}

		script_paths_to_reload.append(p_script_path);
	}
	if (!pending_auto_reload && auto_reload_running_scripts) {
		callable_mp(this, &DocumentEditorContainer::_live_auto_reload_running_scripts).call_deferred();
		pending_auto_reload = true;
	}
}

void DocumentEditorContainer::_live_auto_reload_running_scripts() {
	pending_auto_reload = false;
	EditorDebuggerNode::get_singleton()->reload_scripts(script_paths_to_reload);
	script_paths_to_reload.clear();
}

bool DocumentEditorContainer::_test_script_times_on_disk(Ref<Resource> p_for_script) {
	disk_changed_list->clear();
	TreeItem *r = disk_changed_list->create_item();

	bool need_ask = false;
	bool need_reload = false;
	bool use_autoreload = EDITOR_GET("text_editor/behavior/files/auto_reload_scripts_on_external_change");

	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		Ref<Resource> edited_res = seb->get_edited_resource();
		if (p_for_script.is_valid() && edited_res.is_valid() && p_for_script != edited_res) {
			continue;
		}

		if (edited_res->is_built_in()) {
			continue; // Internal script, who cares.
		}

		uint64_t last_date = seb->edited_file_data.last_modified_time;
		uint64_t date = FileAccess::get_modified_time(seb->edited_file_data.path);

		if (last_date != date) {
			TreeItem *ti = disk_changed_list->create_item(r);
			ti->set_text(0, seb->edited_file_data.path.get_file());

			if (!use_autoreload || seb->is_unsaved()) {
				need_ask = true;
			}
			need_reload = true;
		}
	}

	if (need_reload) {
		if (!need_ask) {
			reload_scripts();
			need_reload = false;
		} else {
			callable_mp((Window *)disk_changed, &Window::popup_centered_ratio).call_deferred(0.3);
		}
	}

	return need_reload;
}

static void _find_changed_scripts_for_external_editor(Node *p_base, Node *p_current, HashSet<Ref<Script>> &r_scripts) {
	if (p_current->get_owner() != p_base && p_base != p_current) {
		return;
	}
	Ref<Script> c = p_current->get_script();

	if (c.is_valid()) {
		r_scripts.insert(c);
	}

	for (int i = 0; i < p_current->get_child_count(); i++) {
		_find_changed_scripts_for_external_editor(p_base, p_current->get_child(i), r_scripts);
	}
}

void DocumentEditorContainer::_update_modified_scripts_for_external_editor(Ref<Script> p_for_script) {
	bool use_external_editor = bool(EDITOR_GET("text_editor/external/use_external_editor"));

	ERR_FAIL_NULL(get_tree());

	HashSet<Ref<Script>> scripts;

	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		_find_changed_scripts_for_external_editor(base, base, scripts);
	}

	for (const Ref<Script> &E : scripts) {
		Ref<Script> scr = E;

		if (!use_external_editor && !scr->get_language()->overrides_external_editor()) {
			continue; // We're not using an external editor for this script.
		}

		if (p_for_script.is_valid() && p_for_script != scr) {
			continue;
		}

		if (scr->is_built_in()) {
			continue; //internal script, who cares, though weird
		}

		uint64_t last_date = scr->get_last_modified_time();
		uint64_t date = FileAccess::get_modified_time(scr->get_path());

		if (last_date != date) {
			Ref<Script> rel_scr = ResourceLoader::load(scr->get_path(), scr->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
			ERR_CONTINUE(rel_scr.is_null());
			scr->set_source_code(rel_scr->get_source_code());
			scr->set_last_modified_time(rel_scr->get_last_modified_time());
			scr->update_exports();

			trigger_live_script_reload(scr->get_path());
		}
	}
}

void _import_text_editor_theme(const String &p_file) {
	if (p_file.get_extension() != "tet") {
		EditorToaster::get_singleton()->popup_str(TTR("Importing theme failed. File is not a text editor theme file (.tet)."), EditorToaster::SEVERITY_ERROR);
		return;
	}
	const String theme_name = p_file.get_file().get_basename();
	if (EditorSettings::is_default_text_editor_theme(theme_name.to_lower())) {
		EditorToaster::get_singleton()->popup_str(TTR("Importing theme failed. File name cannot be 'Default', 'Custom', or 'Godot 2'."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	const String theme_dir = EditorPaths::get_singleton()->get_text_editor_themes_dir();
	Ref<DirAccess> d = DirAccess::open(theme_dir);
	Error err = FAILED;
	if (d.is_valid()) {
		err = d->copy(p_file, theme_dir.path_join(p_file.get_file()));
	}

	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Importing theme failed. Failed to copy theme file."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	// Reload themes and switch to new theme.
	EditorSettings::get_singleton()->update_text_editor_themes_list();
	EditorSettings::get_singleton()->set_manually("text_editor/theme/color_theme", theme_name, true);
	EditorSettings::get_singleton()->notify_changes();
}

void _save_text_editor_theme_as(const String &p_file) {
	String file = p_file;
	if (p_file.get_extension() != "tet") {
		file += ".tet";
	}

	const String theme_name = file.get_file().get_basename();
	if (EditorSettings::is_default_text_editor_theme(theme_name.to_lower())) {
		EditorToaster::get_singleton()->popup_str(TTR("Saving theme failed. File name cannot be 'Default', 'Custom', or 'Godot 2'."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	const String theme_section = "color_theme";
	const Ref<ConfigFile> cf = memnew(ConfigFile);

	// Use the keys from the Godot 2 theme to know which settings to save.
	HashMap<StringName, Color> text_colors = EditorSettings::get_godot2_text_editor_theme();
	text_colors.sort();
	for (const KeyValue<StringName, Color> &text_color : text_colors) {
		const Color val = EditorSettings::get_singleton()->get_setting(text_color.key);
		const String &key = text_color.key.string().replace("text_editor/theme/highlighting/", "");
		cf->set_value(theme_section, key, val.to_html());
	}

	const Error err = cf->save(file);
	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Saving theme failed."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	// Reload themes and switch to saved theme.
	EditorSettings::get_singleton()->update_text_editor_themes_list();
	if (p_file.get_base_dir() == EditorPaths::get_singleton()->get_text_editor_themes_dir()) {
		// Don't need to emit signal or notify changes as the colors are already set.
		EditorSettings::get_singleton()->set_manually("text_editor/theme/color_theme", theme_name, false);
	}
}

bool DocumentEditorContainer::_script_exists(const String &p_path) const {
	if (p_path.is_empty()) {
		return false;
	} else if (p_path.is_resource_file()) {
		return FileAccess::exists(p_path);
	} else {
		return FileAccess::exists(p_path.get_slice("::", 0));
	}
}

void DocumentEditorContainer::_file_dialog_action(const String &p_file) {
	switch (file_dialog_option) {
		case FILE_MENU_NEW_TEXTFILE: {
			Error err;
			{
				Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::WRITE, &err);
				if (err) {
					EditorNode::get_singleton()->show_warning(TTR("Error writing TextFile:") + "\n" + p_file, TTR("Error!"));
					break;
				}
			}

			if (EditorFileSystem::get_singleton()) {
				if (textfile_extensions.has(p_file.get_extension())) {
					EditorFileSystem::get_singleton()->update_file(p_file);
				}
			}
			[[fallthrough]];
		}
		case FILE_MENU_OPEN: {
			open_file(p_file);
		} break;
		case FILE_MENU_SAVE_AS: {
			if (ScriptEditorBase *current = _get_current_editor()) {
				Ref<Resource> resource = current->get_edited_resource();
				String path = ProjectSettings::get_singleton()->localize_path(p_file);
				Error err = _save_text_file(resource, path);

				if (err != OK) {
					EditorNode::get_singleton()->show_warning(TTR("Error saving file!"));
					return;
				}

				resource->set_path(path);
				_update_filenames();
			}
		} break;
		case THEME_SAVE_AS: {
			_save_text_editor_theme_as(p_file);
		} break;
		case THEME_IMPORT: {
			_import_text_editor_theme(p_file);
		} break;
	}
	file_dialog_option = -1;
}

Dictionary DocumentEditorContainer::get_context_data(Control *p_tab_control) {
	Ref<Resource> current_resource;
	String res_path;
	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(p_tab_control);
	if (seb) {
		current_resource = seb->get_edited_resource();
		if (current_resource.is_valid()) {
			res_path = current_resource->get_path();
		}
	} else {
		EditorHelp *help = Object::cast_to<EditorHelp>(p_tab_control);
		if (help) {
			res_path = "help:" + help->get_class().unquote();
		}
	}
	EditorContextMenuPlugin::OptionsData context_data;
	context_data["selected_resource"] = current_resource;
	context_data["path"] = res_path;
	return context_data;
}

List<String> DocumentEditorContainer::_get_recognized_extensions() {
	List<String> extensions;
	for (const String &type : handled_resource_types) {
		ResourceLoader::get_recognized_extensions_for_type(type, &extensions);
	}
	return extensions;
}

void DocumentEditorContainer::_menu_option(int p_option) {
	Node *current_editor = _active_pane_container()->get_current_tab_control();
	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(current_editor);
	EditorHelp *eh = Object::cast_to<EditorHelp>(current_editor);
	switch (p_option) {
		case FILE_MENU_NEW_SCRIPT: {
			script_create_dialog->config("Node", "new_script", false, false);
			script_create_dialog->popup_centered();
		} break;
		case FILE_MENU_NEW_TEXTFILE: {
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
			file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
			file_dialog_option = FILE_MENU_NEW_TEXTFILE;

			file_dialog->clear_filters();
			for (const String &E : textfile_extensions) {
				file_dialog->add_filter("*." + E, E.to_upper());
			}
			file_dialog->set_title(TTRC("New Text File..."));
			file_dialog->popup_file_dialog();
		} break;
		case FILE_MENU_NEW_SHADER: {
			String base_path = FileSystemDock::get_singleton()->get_current_path().get_base_dir();
			shader_create_dialog->config(base_path.path_join("new_shader"), false, false, "Shader");
			shader_create_dialog->popup_centered();
		} break;
		case FILE_MENU_NEW_INCLUDE: {
			String base_path = FileSystemDock::get_singleton()->get_current_path().get_base_dir();
			shader_create_dialog->config(base_path.path_join("new_shader"), false, false, "ShaderInclude");
			shader_create_dialog->popup_centered();
		} break;
		case FILE_MENU_OPEN: {
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
			file_dialog_option = FILE_MENU_OPEN;

			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("Script", &extensions);
			file_dialog->clear_filters();
			for (const String &extension : extensions) {
				file_dialog->add_filter("*." + extension, extension.to_upper());
			}

			for (const String &E : textfile_extensions) {
				file_dialog->add_filter("*." + E, E.to_upper());
			}

			file_dialog->set_title(TTRC("Open File"));
			file_dialog->popup_file_dialog();
			return;
		} break;
		case FILE_MENU_OPEN_SHADER: {
			InspectorDock::get_singleton()->open_resource("Shader");
		} break;
		case FILE_MENU_OPEN_INCLUDE: {
			InspectorDock::get_singleton()->open_resource("ShaderInclude");
		} break;
		case FILE_MENU_REOPEN_CLOSED: {
			if (previous_scripts.is_empty()) {
				return;
			}

			String path = previous_scripts.back()->get();
			previous_scripts.pop_back();

			bool built_in = !path.is_resource_file();

			if (_get_recognized_extensions().find(path.get_extension()) || built_in) {
				if (built_in) {
					String res_path = path.get_slice("::", 0);
					EditorNode::get_singleton()->load_scene_or_resource(res_path, false, false);
				}

				Ref<Resource> scr = ResourceLoader::load(path);
				if (scr.is_null()) {
					EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + path, TTR("Error!"));
					file_dialog_option = -1;
					return;
				}

				edit(scr);
				file_dialog_option = -1;
			} else {
				Error error;
				Ref<TextFile> text_file = _load_text_file(path, &error);
				if (error != OK) {
					EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + path, TTR("Error!"));
				}

				if (text_file.is_valid()) {
					edit(text_file);
					file_dialog_option = -1;
				}
			}
		} break;
		case FILE_MENU_SAVE_ALL: {
			if (_test_script_times_on_disk()) {
				return;
			}

			save_all_scripts();
		} break;
		case FILE_MENU_INSPECT: {
			if (seb && seb->get_edited_resource().is_valid()) {
				EditorNode::get_singleton()->push_item(seb->get_edited_resource().ptr());
			}
		} break;
		case FILE_MENU_INSPECT_NATIVE_SHADER_CODE: {
			if (seb) {
				Ref<Shader> shader = seb->get_edited_resource();
				if (shader.is_valid()) {
					shader->inspect_native_shader_code();
				}
			}
		} break;
		case SEARCH_IN_FILES:
		case REPLACE_IN_FILES: {
			FindInFiles::get_singleton()->open_dock("", REPLACE_IN_FILES == p_option);
		}; break;
		case SEARCH_HELP: {
			_help_search();
		} break;
		case SEARCH_WEBSITE: {
			bool native_class_doc = false;
			if (eh) {
				const HashMap<String, DocData::ClassDoc>::ConstIterator E = EditorHelp::get_doc_data()->class_list.find(eh->get_class());
				native_class_doc = E && !E->value.is_script_doc;
			}
			if (native_class_doc) {
				String name = eh->get_class().to_lower();
				String doc_url = vformat(GODOT_VERSION_DOCS_URL "/classes/class_%s.html", name);
				OS::get_singleton()->shell_open(doc_url);
			} else if (seb) {
				String doc_url = vformat(GODOT_VERSION_DOCS_URL "%s", seb->get_doc_url_path());
				OS::get_singleton()->shell_open(doc_url);
			} else {
				OS::get_singleton()->shell_open(GODOT_VERSION_DOCS_URL "/");
			}
		} break;
		case FILE_MENU_HISTORY_NEXT: {
			_history_forward();
		} break;
		case FILE_MENU_HISTORY_PREV: {
			_history_back();
		} break;
		case FILE_MENU_SORT: {
			sort_document_editors();
		} break;
		case FILE_MENU_CLOSE: {
			if (seb && seb->is_unsaved()) {
				_ask_close_current_unsaved_tab(seb);
			} else {
				_close_current_tab(false);
			}
		} break;
		case FILE_MENU_CLOSE_DOCS: {
			_close_docs_tab();
		} break;
		case FILE_MENU_CLOSE_OTHER_TABS: {
			_close_other_tabs();
		} break;
		case FILE_MENU_CLOSE_TABS_BELOW: {
			_close_tabs_below();
		} break;
		case FILE_MENU_CLOSE_ALL: {
			_close_all_tabs();
		} break;
		case FILE_MENU_MOVE_UP: {
			// The document being moved is the active pane's current one, so it is
			// that pane which has to move it. Node::move_child() only accepts its
			// own children.
			TabContainer *pane = _active_pane_container();
			const int tab = (pane != nullptr) ? pane->get_current_tab() : -1;
			if (pane != nullptr && current_editor != nullptr && tab > 0) {
				pane->move_child(current_editor, tab - 1);
				document_list->update_list();
			}
		} break;
		case FILE_MENU_MOVE_DOWN: {
			TabContainer *pane = _active_pane_container();
			const int tab = (pane != nullptr) ? pane->get_current_tab() : -1;
			if (pane != nullptr && current_editor != nullptr && tab >= 0 && tab < pane->get_tab_count() - 1) {
				pane->move_child(current_editor, tab + 1);
				document_list->update_list();
			}
		} break;
	}

	if (p_option >= EditorContextMenuPlugin::BASE_ID) {
		EditorContextMenuPluginManager::get_singleton()->activate_custom_option(EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR, p_option);
		return;
	}

	if (seb) {
		switch (p_option) {
			case FILE_MENU_SAVE: {
				save_current_script();
			} break;
			case FILE_MENU_SAVE_AS: {
				_auto_format_text(seb);

				Ref<Resource> resource = seb->get_edited_resource();
				Ref<TextFile> text_file = resource;
				Ref<Script> scr = resource;

				if (text_file.is_valid()) {
					file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
					file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
					file_dialog_option = FILE_MENU_SAVE_AS;

					List<String> extensions;
					ResourceLoader::get_recognized_extensions_for_type("Script", &extensions);
					file_dialog->clear_filters();
					file_dialog->set_current_dir(text_file->get_path().get_base_dir());
					file_dialog->set_current_file(text_file->get_path().get_file());
					file_dialog->set_title(TTRC("Save File As..."));
					file_dialog->popup_file_dialog();
					break;
				}

				if (scr.is_valid()) {
					clear_docs_from_script(scr);
				}

				EditorNode::get_singleton()->push_item(resource.ptr());
				EditorNode::get_singleton()->save_resource_as(resource);

				if (scr.is_valid()) {
					update_docs_from_script(scr);
				}
			} break;

			case FILE_MENU_SOFT_RELOAD_TOOL: {
				Ref<Script> scr = seb->get_edited_resource();
				if (scr.is_null()) {
					EditorNode::get_singleton()->show_warning(TTR("Can't obtain the script for reloading."));
					break;
				}
				if (!scr->is_tool()) {
					EditorNode::get_singleton()->show_warning(TTR("Reload only takes effect on tool scripts."));
					return;
				}
				scr->reload(true);

			} break;

			case FILE_MENU_RUN: {
				Ref<Script> scr = seb->get_edited_resource();
				if (scr.is_null()) {
					EditorToaster::get_singleton()->popup_str(TTR("Cannot run the edited file because it's not a script."), EditorToaster::SEVERITY_WARNING);
					break;
				}

				seb->apply_code();

				EditorNode::get_singleton()->run_editor_script(scr);
			} break;
			case FILE_MENU_SNIPPETS: {
				if (snippets_opener) {
					snippets_opener();
				}
			} break;
			case FILE_MENU_SAVE_SNIPPET: {
				_show_save_snippet_dialog();
			} break;
			case FILE_MENU_LAYOUT_SAVE: {
				_show_layout_save_dialog();
			} break;
			case FILE_MENU_LAYOUT_DELETE: {
				_show_layout_delete_dialog();
			} break;
			case FILE_MENU_SPLIT_EDITOR: {
				_toggle_split_editor_view();
			} break;
			case FILE_MENU_COPY_PATH: {
				_copy_script_path();
			} break;
			case FILE_MENU_COPY_UID: {
				_copy_script_uid();
			} break;
			case FILE_MENU_SHOW_IN_FILE_SYSTEM: {
				const Ref<Resource> res = seb->get_edited_resource();
				String path = res->get_path();
				if (!path.is_empty()) {
					if (res->is_built_in()) {
						path = path.get_slice("::", 0); // Show the scene instead.
					}

					FileSystemDock::get_singleton()->navigate_to_path(path);
				}
			} break;
		}
	} else if (eh) {
		switch (p_option) {
			case HELP_SEARCH_FIND: {
				eh->popup_search();
			} break;
			case HELP_SEARCH_FIND_NEXT: {
				eh->search_again();
			} break;
			case HELP_SEARCH_FIND_PREVIOUS: {
				eh->search_again(true);
			} break;
		}
	}
}

void DocumentEditorContainer::_theme_option(int p_option) {
	switch (p_option) {
		case THEME_IMPORT: {
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
			file_dialog_option = THEME_IMPORT;
			file_dialog->clear_filters();
			file_dialog->add_filter("*.tet");
			file_dialog->set_title(TTRC("Import Theme"));
			file_dialog->popup_file_dialog();
		} break;
		case THEME_RELOAD: {
			EditorSettings::get_singleton()->mark_setting_changed("text_editor/theme/color_theme");
			EditorSettings::get_singleton()->notify_changes();
		} break;
		case THEME_SAVE_AS: {
			DocumentEditorContainer::_show_save_theme_as_dialog();
		} break;
	}
}

void DocumentEditorContainer::_show_save_theme_as_dialog() {
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_dialog_option = THEME_SAVE_AS;
	file_dialog->clear_filters();
	file_dialog->add_filter("*.tet");
	file_dialog->set_current_path(EditorPaths::get_singleton()->get_text_editor_themes_dir().path_join(EDITOR_GET("text_editor/theme/color_theme")) + " New");
	file_dialog->set_title(TTRC("Save Theme As..."));
	file_dialog->popup_file_dialog();
}

bool DocumentEditorContainer::_has_docs_tab() const {
	// A help page can be open in any pane.
	for (Control *editor : _get_all_pane_editors()) {
		if (Object::cast_to<EditorHelp>(editor)) {
			return true;
		}
	}
	return false;
}

bool DocumentEditorContainer::_has_script_tab() const {
	// Saving everything covers the documents of every pane.
	for (Control *editor : _get_all_pane_editors()) {
		if (Object::cast_to<ScriptEditorBase>(editor)) {
			return true;
		}
	}
	return false;
}

void DocumentEditorContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAG_BEGIN: {
			// Processing stays on for the whole drag so the hint and the inline
			// preview follow wherever the drag goes.
			set_process(true);
		} break;

		case NOTIFICATION_DRAG_END: {
			// The drag is over: a tab dragged outside the panes becomes a window
			// now, from the pending ids, rather than on a later process tick
			// where an unrelated action could consume them. The viewport has
			// already cleared its dragging state when this arrives, so
			// _update_drag_feedback() takes its "drag ended" path.
			if (pending_popout_pane.is_valid()) {
				_update_drag_feedback();
			}
			pending_popout_pane = ObjectID();
			pending_popout_editor = ObjectID();
			_hide_drop_overlay();
		} break;

		case NOTIFICATION_PROCESS: {
			// A drag needs its hint and preview refreshed as it moves, and the
			// tab menu needs its hovered entry followed (the menu only reports
			// keyboard navigation, not the mouse).
			Viewport *drag_viewport = get_viewport();
			const bool dragging = drag_viewport != nullptr && drag_viewport->gui_is_dragging();
			const bool menu_open = tab_context_menu != nullptr && tab_context_menu->is_visible();

			if (dragging) {
				_update_drag_feedback();
			} else if (pending_popout_pane.is_valid()) {
				// The drag has just ended: a tab dragged out becomes a window.
				_update_drag_feedback();
			} else if (menu_open) {
				_update_tab_menu_highlight();
			} else {
				_hide_drop_overlay();
				set_process(false);
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			// Keep the document name shown at the top in sync with the pane the
			// user is actually working in.
			Viewport *viewport = get_viewport();
			if (viewport != nullptr && !viewport->is_connected("gui_focus_changed", callable_mp(this, &DocumentEditorContainer::_focus_changed))) {
				viewport->connect("gui_focus_changed", callable_mp(this, &DocumentEditorContainer::_focus_changed));
			}
			_apply_editor_settings();
			[[fallthrough]];
		}

		case NOTIFICATION_TRANSLATION_CHANGED: {
			_update_filenames();
			_update_online_doc();
			[[fallthrough]];
		}
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			tab_container->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("ScriptEditor"), EditorStringName(EditorStyles)));
			_update_drop_overlay_theme();
			_update_tab_bar_settings();
			_update_pane_top_spacing();

			_calculate_script_name_button_size();

			if (is_main_editor) {
				help_search->set_button_icon(get_editor_theme_icon(SNAME("HelpSearch")));
			}
			site_search->set_button_icon(get_editor_theme_icon(SNAME("ExternalLink")));

			if (is_layout_rtl()) {
				script_forward->set_button_icon(get_editor_theme_icon(SNAME("Back")));
				script_back->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
			} else {
				script_forward->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
				script_back->set_button_icon(get_editor_theme_icon(SNAME("Back")));
			}

			recent_scripts->reset_size();

			toggle_files_button->set_button_icon(get_editor_theme_icon(SNAME("SidebarLeft")));
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorThemeManager::is_generated_theme_outdated() ||
					EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/fonts") ||
					EditorSettings::get_singleton()->check_changed_settings_in_group("interface/scene_tabs") ||
					EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor") ||
					EditorSettings::get_singleton()->check_changed_settings_in_group("docks/filesystem")) {
				_apply_editor_settings();
			}
		} break;

		case NOTIFICATION_READY: {
			if (is_main_editor) {
				InspectorDock::get_singleton()->connect("request_help", callable_mp(this, &DocumentEditorContainer::_help_class_open));
				EditorNode::get_singleton()->connect("request_help_search", callable_mp(this, &DocumentEditorContainer::_help_search));
				EditorNode::get_singleton()->connect("script_add_function_request", callable_mp(this, &DocumentEditorContainer::_add_callback));
			}
			EditorNode::get_singleton()->connect("scene_closed", callable_mp(this, &DocumentEditorContainer::_close_builtin_scripts_from_scene));
			EditorNode::get_singleton()->connect("resource_saved", callable_mp(this, &DocumentEditorContainer::_res_saved_callback));
			EditorNode::get_singleton()->connect("scene_saved", callable_mp(this, &DocumentEditorContainer::_scene_saved_callback));
			// Connect to scene_root child entered instead of scene_changed for new scenes.
			EditorNode::get_singleton()->get_scene_root()->connect("child_entered_tree", callable_mp(this, &DocumentEditorContainer::_connect_to_scene).unbind(1));
			FileSystemDock::get_singleton()->connect("files_moved", callable_mp(this, &DocumentEditorContainer::_files_moved));
			FileSystemDock::get_singleton()->connect("file_removed", callable_mp(this, &DocumentEditorContainer::_file_removed));

			script_split->connect("dragged", callable_mp(this, &DocumentEditorContainer::_split_dragged));
			list_split->connect("dragged", callable_mp(this, &DocumentEditorContainer::_split_dragged));

			EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &DocumentEditorContainer::_update_filenames));
#ifdef ANDROID_ENABLED
			set_process(true);
#endif
		} break;

#ifdef ANDROID_ENABLED
		case NOTIFICATION_VISIBILITY_CHANGED: {
			set_process(is_visible_in_tree());
		} break;

		case NOTIFICATION_PROCESS: {
			if (!is_visible()) {
				break;
			}

			const int kb_height = DisplayServer::get_singleton()->virtual_keyboard_get_height();
			if (kb_height == last_kb_height) {
				break;
			}

			last_kb_height = kb_height;
			float spacer_height = 0.0f;

			if (EditorNode::get_singleton()->is_portrait()) {
				// Reset virtual_keyboard_spacer and let EditorNode handle resizing in portrait mode.
				virtual_keyboard_spacer->set_custom_minimum_size(Size2());
			} else {
				const float status_bar_height = 28 * EDSCALE;
				const float control_bottom = get_global_position().y + get_size().y;
				const float extra_bottom = get_viewport_rect().size.y - control_bottom;
				spacer_height = float(kb_height) - extra_bottom - status_bar_height;
				spacer_height = fmax(0, spacer_height);
				virtual_keyboard_spacer->set_custom_minimum_size(Size2(0, spacer_height));
				EditorSceneTabs::get_singleton()->set_visible(kb_height == 0);
			}

			// Make sure caret is visible after resize.
			if (TextEditorBase *editor = Object::cast_to<TextEditorBase>(_get_current_editor())) {
				if (CodeTextEditor *code_editor = editor->get_code_editor()) {
					if (CodeEdit *text_editor = code_editor->get_text_editor()) {
						if (text_editor->has_focus()) {
							text_editor->adjust_viewport_to_caret();
						}
					}
				}
			}
		} break;
#endif
		case NOTIFICATION_APPLICATION_FOCUS_IN: {
			if (is_inside_tree()) {
				_test_script_times_on_disk();
				_update_modified_scripts_for_external_editor();
			}
		} break;
	}
}

void DocumentEditorContainer::_close_builtin_scripts_from_scene(const String &p_scene) {
	for (Control *editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
		if (seb == nullptr) {
			continue;
		}
		Ref<Resource> res = seb->get_edited_resource();
		if (!res.is_valid() || !res->is_built_in() || !res->get_path().begins_with(p_scene)) { // Is an internal script and belongs to scene being closed.
			continue;
		}
		TabContainer *pane = _pane_of(editor);
		if (pane == nullptr || editor->get_parent() != pane) {
			continue;
		}
		_close_tab(pane, pane->get_tab_idx_from_control(editor), false);
	}
}

void DocumentEditorContainer::_toggle_files_pressed(bool p_pressed) {
	ERR_FAIL_NULL(list_split);
	list_split->set_visible(p_pressed);
	EditorSettings::get_singleton()->set_project_metadata("show_files_panel", config_section, list_split->is_visible());
}

void DocumentEditorContainer::edited_scene_changed() {
	_update_modified_scripts_for_external_editor();
}

Vector<String> DocumentEditorContainer::_get_breakpoints() {
	List<String> bpoints_list;
	get_breakpoints(&bpoints_list);

	Vector<String> ret;
	for (const String &E : bpoints_list) {
		ret.push_back(E);
	}

	return ret;
}

void DocumentEditorContainer::get_breakpoints(List<String> *p_breakpoints) {
	HashSet<String> loaded_scripts;
	for (Control *editor : _get_all_pane_editors()) {
		CodeEditorBase *ceb = Object::cast_to<CodeEditorBase>(editor);
		if (!ceb) {
			continue;
		}

		Ref<Script> scr = ceb->get_edited_resource();
		if (scr.is_null()) {
			continue;
		}

		String base = scr->get_path();
		loaded_scripts.insert(base);
		if (base.is_empty() || base.begins_with("local://")) {
			continue;
		}

		PackedInt32Array bpoints = ceb->get_breakpoints();
		for (int32_t bpoint : bpoints) {
			p_breakpoints->push_back(base + ":" + itos((int)bpoint + 1));
		}
	}

	// Load breakpoints that are in closed scripts.
	Vector<String> cached_editors = script_editor_cache->get_sections();
	for (const String &E : cached_editors) {
		if (loaded_scripts.has(E)) {
			continue;
		}

		Array breakpoints = _get_cached_breakpoints_for_script(E);
		for (int breakpoint : breakpoints) {
			p_breakpoints->push_back(E + ":" + itos((int)breakpoint + 1));
		}
	}
}

void DocumentEditorContainer::ensure_select_current() {
	if (restoring_layout) {
		return;
	}
	// The document that matters is the one of the pane being worked in, so the
	// gate is that pane's current tab, not the primary pane's.
	TabContainer *pane = _active_pane_container();
	if (pane != nullptr && pane->get_current_tab() >= 0) {
		TextEditorBase *teb = Object::cast_to<TextEditorBase>(_get_current_editor());
		if (teb) {
			teb->enable_editor();
		}
		if (_get_current_editor() && !grab_focus_block && is_visible_in_tree()) {
			_get_current_editor()->ensure_focus();
		}
	}
	_update_find_replace_bar();

	_update_selected_editor_menu();
	_update_online_doc();

	document_list->update_list();

	document_outline->update_outline();
	document_outline->update_visibility();

	_update_document_name_button();
}

void DocumentEditorContainer::_connect_to_scene() {
	if (!highlight_scene_scripts) {
		return;
	}
	Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
	if (!edited_scene) {
		return;
	}
	_connect_to_scene_recursive(edited_scene);
}

void DocumentEditorContainer::_connect_to_scene_recursive(Node *p_current) {
	_queue_update_list();
	const Callable update_callable = callable_mp(this, &DocumentEditorContainer::_queue_update_list);
	if (p_current->is_connected(CoreStringName(script_changed), update_callable)) {
		return;
	}
	p_current->connect(CoreStringName(script_changed), update_callable);
	p_current->connect(SceneStringName(tree_exited), update_callable);
	// Use `child_entered_tree` to recursively connect to children.
	p_current->connect(SNAME("child_entered_tree"), callable_mp(this, &DocumentEditorContainer::_connect_to_scene_recursive));
}

void DocumentEditorContainer::_update_document_list() {
	document_list->update_list();
	list_update_queued = false;
}

void DocumentEditorContainer::_queue_update_list() {
	if (list_update_queued || !highlight_scene_scripts) {
		return;
	}
	list_update_queued = true;
	callable_mp(this, &DocumentEditorContainer::_update_document_list).call_deferred();
}

void DocumentEditorContainer::_update_online_doc() {
	TabContainer *pane = _active_pane_container();
	Node *current = (pane != nullptr) ? pane->get_current_tab_control() : nullptr;

	EditorHelp *eh = Object::cast_to<EditorHelp>(current);
	bool native_class_doc = false;
	if (eh) {
		const HashMap<String, DocData::ClassDoc>::ConstIterator E = EditorHelp::get_doc_data()->class_list.find(eh->get_class());
		native_class_doc = E && !E->value.is_script_doc;
	}
	if (native_class_doc) {
		String name = eh->get_class();
		String tooltip = vformat(TTR("Open '%s' in Godot online documentation."), name);
		site_search->set_text(TTRC("Open in Online Docs"));
		site_search->set_tooltip_text(tooltip);
	} else {
		site_search->set_text(TTRC("Online Docs"));
		site_search->set_tooltip_text(TTRC("Open Godot online documentation."));
	}
}

Ref<TextFile> DocumentEditorContainer::_load_text_file(const String &p_path, Error *r_error) const {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	String path = ResourceLoader::path_remap(local_path);

	// Reuse an already-open TextFile resource to avoid creating duplicates.
	// This prevents opening the same file multiple times as "unsaved" tabs.
	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		if (seb->edited_file_data.path == local_path) {
			Ref<TextFile> existing_text_file = seb->get_edited_resource();
			if (existing_text_file.is_valid()) {
				if (r_error) {
					*r_error = OK;
				}
				return existing_text_file;
			}
		}
	}

	Ref<TextFile> text_file;
	text_file.instantiate();
	Error err = text_file->load_text(path);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Cannot load text file '" + path + "'.");

	text_file->set_file_path(local_path);
	text_file->set_path(local_path, true);

	if (ResourceLoader::get_timestamp_on_load()) {
		text_file->set_last_modified_time(FileAccess::get_modified_time(path));
	}

	if (r_error) {
		*r_error = OK;
	}

	return text_file;
}

Error DocumentEditorContainer::_save_text_file(Ref<TextFile> p_text_file, const String &p_path) {
	Ref<TextFile> sqscr = p_text_file;
	ERR_FAIL_COND_V(sqscr.is_null(), ERR_INVALID_PARAMETER);

	String source = sqscr->get_text();

	Error err;
	{
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);

		ERR_FAIL_COND_V_MSG(err, err, "Cannot save text file '" + p_path + "'.");

		file->store_string(source);
		if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
			return ERR_CANT_CREATE;
		}
	}

	if (ResourceSaver::get_timestamp_on_save()) {
		p_text_file->set_last_modified_time(FileAccess::get_modified_time(p_path));
	}

	EditorFileSystem::get_singleton()->update_file(p_path);

	_res_saved_callback(sqscr);
	return OK;
}

bool DocumentEditorContainer::edit(const Ref<Resource> &p_resource, int p_line, int p_col, bool p_grab_focus) {
	if (p_resource.is_null()) {
		return false;
	}

	Ref<Script> scr = p_resource;

	// Don't open dominant script if using an external editor.
	bool use_external_editor = ScriptEditor::get_singleton()->should_use_external_editor(p_resource);
	const bool open_dominant = EDITOR_GET("text_editor/behavior/files/open_dominant_script_on_scene_change");

	const bool should_open = (open_dominant && !use_external_editor) || !EditorNode::get_singleton()->is_changing_scene();

	if (scr.is_valid() && scr->get_language()->overrides_external_editor()) {
		if (should_open) {
			Error err = scr->get_language()->open_in_external_editor(scr, p_line >= 0 ? p_line : 0, p_col);
			if (err != OK) {
				ERR_PRINT("Couldn't open script in the overridden external text editor");
			}
		}
		return false;
	}

	if (use_external_editor &&
			(EditorDebuggerNode::get_singleton()->get_dump_stack_script() != p_resource || (EditorDebuggerNode::get_singleton()->get_debug_with_external_editor() && scr.is_valid())) &&
			p_resource->get_path().is_resource_file()) {
		if (ScriptEditorPlugin::open_in_external_editor(ProjectSettings::get_singleton()->globalize_path(p_resource->get_path()), p_line, p_col)) {
			return false;
		} else {
			ERR_PRINT("Couldn't open external text editor, falling back to the internal editor. Review your `text_editor/external/` editor settings.");
		}
	}

	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		if ((scr.is_valid() && seb->get_edited_resource() == p_resource) || seb->get_edited_resource()->get_path() == p_resource->get_path()) {
			if (should_open) {
				TextEditorBase *teb = Object::cast_to<TextEditorBase>(seb);
				if (teb) {
					teb->enable_editor();
				}

				if (_get_current_editor() != seb) {
					grab_focus_block = !p_grab_focus;
					_activate_document(pane_editor, p_grab_focus);
					grab_focus_block = false;
				} else if (is_visible_in_tree() && p_grab_focus) {
					seb->ensure_focus();
				}

				if (teb) {
					ScriptEditorNavigationMarker::get_singleton()->locate_begin();
					if (p_line >= 0) {
						teb->goto_line_and_center_if_necessary(p_line, p_col);
					} else {
						teb->trigger_history_save_on_navigate();
					}
					ScriptEditorNavigationMarker::get_singleton()->locate_end();
				}
				document_list->ensure_current_is_visible();
			}
			return true;
		}
	}

	// doesn't have it, make a new one
	ScriptEditorBase *seb = nullptr;

	for (int i = script_editor_func_count - 1; i >= 0; i--) {
		seb = script_editor_funcs[i](p_resource);
		if (seb) {
			break;
		}
	}
	ERR_FAIL_NULL_V(seb, false);

	seb->set_edited_resource(p_resource);

	grab_focus_block = !p_grab_focus;
	// New documents open in the pane the user was last working in.
	_active_pane_container()->add_child(seb);
	grab_focus_block = false;

	if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(seb)) {
		if (p_grab_focus) {
			teb->enable_editor();
		}

		ScriptEditorBase::EditMenusBase *editor_edit_menu = nullptr;
		for (ScriptEditorBase::EditMenusBase *editor_menu : editor_menus) {
			if (editor_menu->handles(teb)) {
				editor_edit_menu = editor_menu;
				break;
			}
		}
		if (!editor_edit_menu) {
			editor_edit_menu = teb->create_edit_menu(this);
		}
		if (!editor_edit_menu->get_parent()) {
			editor_edit_menu->hide();
			editor_menus.push_back(editor_edit_menu);
			menu_hb->add_child(editor_edit_menu);
			menu_hb->move_child(editor_edit_menu, 3);
			for (int i = 0; i < editor_edit_menu->get_child_count(); ++i) {
				Control *c = Object::cast_to<Control>(editor_edit_menu->get_child(i));
				if (c) {
					c->set_shortcut_context(this);
				}
			}
		}
	}

	_update_filenames();
	grab_focus_block = !p_grab_focus;
	// New documents open in the pane the user was last working in.
	TabContainer *target_pane = _active_pane_container();
	target_pane->set_current_tab(target_pane->get_tab_count() - 1);
	_set_active_pane(target_pane);
	ensure_select_current();
	grab_focus_block = false;
	_add_recent_script(p_resource->get_path());

	// If we delete a script within the filesystem, the original resource path
	// is lost, so keep it as `edited_file_data` to figure out the exact tab to delete.
	seb->edited_file_data.path = p_resource->get_path();
	seb->edited_file_data.last_modified_time = FileAccess::get_modified_time(p_resource->get_path());

	seb->connect("name_changed", callable_mp(this, &DocumentEditorContainer::_update_filenames));
	seb->connect("_validation_updated", callable_mp(this, &DocumentEditorContainer::_validation_updated));

	if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(seb)) {
		// Syntax highlighting.
		bool highlighter_set = false;
		for (int i = 0; i < syntax_highlighters.size(); i++) {
			Ref<EditorSyntaxHighlighter> highlighter = syntax_highlighters[i]->_create();
			if (highlighter.is_null()) {
				continue;
			}
			teb->add_syntax_highlighter(highlighter);

			if (highlighter_set) {
				continue;
			}

			PackedStringArray languages = highlighter->_get_supported_languages();
			// If script try language, else use extension.
			if (scr.is_valid()) {
				if (languages.has(scr->get_language()->get_name())) {
					teb->set_syntax_highlighter(highlighter);
					highlighter_set = true;
				}
				continue;
			}

			if (languages.has(p_resource->get_path().get_extension())) {
				teb->set_syntax_highlighter(highlighter);
				highlighter_set = true;
				continue;
			}

			// For built-in shaders, which do not use file extensions in their path.
			ShaderTextEditor *ste = Object::cast_to<ShaderTextEditor>(teb);
			if (ste && languages.has("gdshader")) {
				teb->set_syntax_highlighter(highlighter);
				highlighter_set = true;
			}
		}

		teb->set_tooltip_request_func(callable_mp(this, &DocumentEditorContainer::_get_debug_tooltip));

		teb->connect("request_help", callable_mp(this, &DocumentEditorContainer::_help_search));
		teb->connect("request_open_script_at_line", callable_mp(this, &DocumentEditorContainer::_goto_script_line));
		teb->connect("go_to_help", callable_mp(this, &DocumentEditorContainer::_help_class_goto));
		teb->connect("_request_save_new_history", callable_mp(this, &DocumentEditorContainer::_save_new_history).bind(teb));
		teb->connect("_request_save_previous_state", callable_mp(this, &DocumentEditorContainer::_save_previous_state).bind(teb));
		teb->connect("go_to_method", callable_mp(this, &DocumentEditorContainer::script_goto_method));

#ifndef DISABLE_DEPRECATED
		teb->connect("search_in_files_requested", callable_mp(FindInFiles::get_singleton(), &FindInFiles::open_dock).bind("", false));
		teb->connect("replace_in_files_requested", callable_mp(FindInFiles::get_singleton(), &FindInFiles::open_dock).bind("", true));
#endif

		if (script_editor_cache->has_section(p_resource->get_path())) {
			teb->set_edit_state(script_editor_cache->get_value(p_resource->get_path(), "state"), p_grab_focus);
			if (ScriptTextEditor *ste = Object::cast_to<ScriptTextEditor>(teb)) {
				ste->store_previous_state();
			}
		}

		if (CodeTextEditor *cte = teb->get_code_editor()) {
			cte->set_zoom_factor(zoom_factor);
			cte->connect("zoomed", callable_mp(this, &DocumentEditorContainer::_set_script_zoom_factor));
			cte->connect(SceneStringName(visibility_changed), callable_mp(this, &DocumentEditorContainer::_update_code_editor_zoom_factor).bind(cte));
		}
	}

	sort_document_editors();
	_save_layout();

	//test for modification, maybe the script was not edited but was loaded

	_test_script_times_on_disk(p_resource);
	_update_modified_scripts_for_external_editor(p_resource);

	if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(seb)) {
		ScriptEditorNavigationMarker::get_singleton()->locate_begin();
		if (p_line >= 0) {
			teb->goto_line_and_center_if_necessary(p_line, p_col);
		} else {
			teb->trigger_history_save_on_navigate();
		}
		ScriptEditorNavigationMarker::get_singleton()->locate_end();
	}

	ScriptEditor::get_singleton()->notify_script_changed(p_resource);
	return true;
}

void DocumentEditorContainer::reload_open_files() {
	_test_script_times_on_disk();
	_update_modified_scripts_for_external_editor();
}

Control *DocumentEditorContainer::get_active_editor() const {
	TabContainer *pane = _active_pane_container();
	return (pane != nullptr) ? pane->get_current_tab_control() : nullptr;
}

Vector<Control *> DocumentEditorContainer::get_all_editors() const {
	// Documents live in every pane, not only the primary one.
	Vector<Control *> editors;
	for (TabContainer *pane : panes) {
		for (int i = 0; i < pane->get_tab_count(); i++) {
			editors.push_back(pane->get_tab_control(i));
		}
	}
	return editors;
}

void DocumentEditorContainer::focus_active_editor() {
	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(get_active_editor());
	EditorHelp *eh = Object::cast_to<EditorHelp>(get_active_editor());
	if (seb && seb->get_base_editor()) {
		seb->get_base_editor()->grab_focus();
	} else if (eh) {
		eh->set_focused();
	}
}

PackedStringArray DocumentEditorContainer::get_unsaved_scripts() const {
	PackedStringArray unsaved_list;

	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (seb && seb->is_unsaved()) {
			unsaved_list.append(seb->get_document_name());
		}
	}
	return unsaved_list;
}

PackedStringArray DocumentEditorContainer::get_unsaved_files() const {
	PackedStringArray unsaved_list;

	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (seb && seb->is_unsaved()) {
			unsaved_list.append(seb->get_edited_resource()->get_path());
		}
	}
	return unsaved_list;
}

void DocumentEditorContainer::save_current_script() {
	ScriptEditorBase *current = _get_current_editor();
	if (!current || _test_script_times_on_disk()) {
		return;
	}

	_auto_format_text(current);

	Ref<Resource> resource = current->get_edited_resource();
	Ref<TextFile> text_file = resource;
	Ref<Script> scr = resource;

	if (text_file.is_valid()) {
		current->apply_code();
		_save_text_file(text_file, text_file->get_path());
		return;
	}

	if (scr.is_valid()) {
		clear_docs_from_script(scr);
	}

	EditorNode::get_singleton()->save_resource(resource);

	if (scr.is_valid()) {
		update_docs_from_script(scr);
	}
}

void DocumentEditorContainer::save_all_scripts() {
	HashSet<String> scenes_to_save;

	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		_auto_format_text(seb);

		if (!seb->is_unsaved()) {
			continue;
		}

		Ref<Resource> edited_res = seb->get_edited_resource();
		if (edited_res.is_valid()) {
			seb->apply_code();
		}

		Ref<Script> scr = edited_res;

		if (scr.is_valid()) {
			clear_docs_from_script(scr);
		}

		if (!edited_res->is_built_in()) {
			Ref<TextFile> text_file = edited_res;
			if (text_file.is_valid()) {
				_save_text_file(text_file, text_file->get_path());
				continue;
			}

			// External script, save it.
			EditorNode::get_singleton()->save_resource(edited_res);
		} else {
			// For built-in scripts, save their scenes instead.
			const String scene_path = edited_res->get_path().get_slice("::", 0);
			if (!scene_path.is_empty() && !scenes_to_save.has(scene_path)) {
				scenes_to_save.insert(scene_path);
			}
		}

		if (scr.is_valid()) {
			update_docs_from_script(scr);
		}
	}

	if (!scenes_to_save.is_empty()) {
		EditorNode::get_singleton()->save_scene_list(scenes_to_save);
	}

	_update_filenames();
}

void DocumentEditorContainer::update_script_times() {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		if (ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i))) {
			seb->edited_file_data.last_modified_time = FileAccess::get_modified_time(seb->edited_file_data.path);
		}
	}
}

void DocumentEditorContainer::apply_scripts() const {
	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(seb)) {
			teb->insert_final_newline();
		}
		seb->apply_code();
	}
}

void DocumentEditorContainer::reload_scripts(bool p_refresh_only) {
	// Call deferred to make sure it runs on the main thread.
	if (!Thread::is_main_thread()) {
		callable_mp(this, &DocumentEditorContainer::_reload_scripts).call_deferred(p_refresh_only);
		return;
	}
	_reload_scripts(p_refresh_only);
}

void DocumentEditorContainer::_reload_scripts(bool p_refresh_only) {
	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		Ref<Resource> edited_res = seb->get_edited_resource();

		if (edited_res->is_built_in()) {
			continue; // Internal script, who cares.
		}

		if (p_refresh_only) {
			// Make sure the modified time is correct.
			seb->edited_file_data.last_modified_time = FileAccess::get_modified_time(edited_res->get_path());
		} else {
			uint64_t last_date = seb->edited_file_data.last_modified_time;
			uint64_t date = FileAccess::get_modified_time(edited_res->get_path());

			if (last_date == date) {
				continue;
			}
			seb->edited_file_data.last_modified_time = date;

			Ref<Script> scr = edited_res;
			if (scr.is_valid()) {
				Ref<Script> rel_scr = ResourceLoader::load(scr->get_path(), scr->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
				ERR_CONTINUE(rel_scr.is_null());
				scr->set_source_code(rel_scr->get_source_code());
				scr->reload(true);

				update_docs_from_script(scr);
			}

			Ref<JSON> json = edited_res;
			if (json.is_valid()) {
				Ref<JSON> rel_json = ResourceLoader::load(json->get_path(), json->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
				ERR_CONTINUE(rel_json.is_null());
				json->parse(rel_json->get_parsed_text(), true);
			}

			Ref<TextFile> text_file = edited_res;
			if (text_file.is_valid()) {
				text_file->reload_from_file();
			}

			ShaderTextEditor *ste = Object::cast_to<ShaderTextEditor>(seb);
			if (ste) {
				Ref<Shader> shader = edited_res;
				Ref<ShaderInclude> shader_inc = edited_res;
				String new_code;
				if (shader.is_valid()) {
					Ref<Shader> rel_sh = ResourceLoader::load(shader->get_path(), shader->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
					ERR_CONTINUE(rel_sh.is_null());
					new_code = rel_sh->get_code();
				} else if (shader_inc.is_valid()) {
					Ref<ShaderInclude> rel_sh_inc = ResourceLoader::load(shader_inc->get_path(), shader_inc->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
					ERR_CONTINUE(rel_sh_inc.is_null());
					new_code = rel_sh_inc->get_code();
				}
				ste->set_code_block_changed(new_code);
			}
		}

		if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(seb)) {
			Dictionary state = teb->get_edit_state();
			teb->reload_text();
			teb->set_edit_state(state);
		}
	}

	disk_changed->hide();
}

void DocumentEditorContainer::_auto_format_text(ScriptEditorBase *p_seb) {
	if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(p_seb)) {
		if (format_on_save) {
			teb->format_code();
			return;
		}
		if (trim_trailing_whitespace_on_save) {
			teb->trim_trailing_whitespace();
		}
		if (trim_final_newlines_on_save) {
			teb->trim_final_newlines();
		}
		if (convert_indent_on_save) {
			teb->convert_indent();
		}
	}
}

void DocumentEditorContainer::open_script_create_dialog(const String &p_base_name, const String &p_base_path) {
	_menu_option(FILE_MENU_NEW_SCRIPT);
	script_create_dialog->config(p_base_name, p_base_path);
}

void DocumentEditorContainer::open_text_file_create_dialog(const String &p_base_path, const String &p_base_name) {
	_menu_option(FILE_MENU_NEW_TEXTFILE);
	file_dialog->set_current_dir(p_base_path);
	file_dialog->set_current_file(p_base_name);
}

Ref<Resource> DocumentEditorContainer::open_file(const String &p_file) {
	if (_get_recognized_extensions().find(p_file.get_extension())) {
		Ref<Resource> scr = ResourceLoader::load(p_file);
		if (scr.is_null()) {
			EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + p_file, TTR("Error!"));
			return Ref<Resource>();
		}

		edit(scr);
		return scr;
	}

	Error error;
	Ref<TextFile> text_file = _load_text_file(p_file, &error);
	if (error != OK) {
		EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + p_file, TTR("Error!"));
		return Ref<Resource>();
	}

	if (text_file.is_valid()) {
		edit(text_file);
		return text_file;
	}
	return Ref<Resource>();
}

bool DocumentEditorContainer::can_open_file(const String &p_file) const {
	if (p_file.is_empty() || !FileAccess::exists(p_file)) {
		return false;
	}

	const String &file_type = ResourceLoader::get_resource_type(p_file);
	if (handled_resource_types.has(file_type)) {
		return true;
	}

	if (is_main_editor && textfile_extensions.has(p_file.get_extension())) {
		Error err;
		Ref<TextFile> text_file = _load_text_file(p_file, &err);
		if (text_file.is_valid() && err == OK) {
			return true;
		}
	}
	return false;
}

Error DocumentEditorContainer::close_file(const String &p_file) {
	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (seb && seb->get_edited_resource()->get_path() == p_file) {
			if (seb->is_unsaved()) {
				seb->get_edited_resource()->reload_from_file();
			}
			TabContainer *pane = _pane_of(pane_editor);
			if (pane != nullptr && pane_editor->get_parent() == pane) {
				_close_tab(pane, pane->get_tab_idx_from_control(pane_editor), false);
			}
			return OK;
		}
	}
	return ERR_FILE_NOT_FOUND;
}

void DocumentEditorContainer::_add_callback(Object *p_obj, const String &p_function, const PackedStringArray &p_args) {
	ERR_FAIL_NULL(p_obj);
	Ref<Script> scr = p_obj->get_script();
	ERR_FAIL_COND(scr.is_null());

	if (!scr->get_language()->can_make_function()) {
		return;
	}

	EditorNode::get_singleton()->push_item(scr.ptr());

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptTextEditor *ste = Object::cast_to<ScriptTextEditor>(tab_container->get_tab_control(i));
		if (!ste || ste->get_edited_resource() != scr) {
			continue;
		}

		ste->add_callback(p_function, p_args);

		_go_to_tab(i, true);

		// Save the current script so the changes can be picked up by an external editor.
		if (!scr.ptr()->is_built_in()) { // But only if it's not built-in script.
			save_current_script();
		} else {
			ste->apply_code();
			scr->reload(true);
		}

		break;
	}

	// Move back to the previously edited node to reselect it in the Inspector and the SignalsDock.
	// We assume that the previous item is the node on which the callbacks were added.
	EditorNode::get_singleton()->edit_previous_item();
}

void DocumentEditorContainer::_save_editor_state(ScriptEditorBase *p_editor) {
	if (restoring_layout) {
		return;
	}

	const String &path = p_editor->get_edited_resource()->get_path();
	if (path.is_empty()) {
		return;
	}

	if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(p_editor)) {
		script_editor_cache->set_value(path, "state", teb->get_edit_state());
	}
	// This is saved later when we save the editor layout.
}

void DocumentEditorContainer::_save_layout() {
	if (restoring_layout) {
		return;
	}

	EditorNode::get_singleton()->save_editor_layout_delayed();
}

void DocumentEditorContainer::_apply_editor_settings() {
	textfile_extensions.clear();
	const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
	for (const String &E : textfile_ext) {
		textfile_extensions.insert(E);
	}

	trim_trailing_whitespace_on_save = EDITOR_GET("text_editor/behavior/files/trim_trailing_whitespace_on_save");
	trim_final_newlines_on_save = EDITOR_GET("text_editor/behavior/files/trim_final_newlines_on_save");
	convert_indent_on_save = EDITOR_GET("text_editor/behavior/files/convert_indent_on_save");
	format_on_save = EDITOR_GET("text_editor/behavior/formatter/format_on_save");

	document_outline->update_editor_settings();

	_update_autosave_timer();

	DocumentNameDisplayMode previous_document_display_name_mode = document_display_name_mode;
	document_display_name_mode = (DocumentNameDisplayMode)(int)EDITOR_GET("text_editor/script_list/list_script_names_as");
	if (previous_document_display_name_mode != document_display_name_mode) {
		_update_filenames();
	}

	DocumentSortMode previous_document_sort_by_mode = document_sort_by_mode;
	bool previous_document_sort_group_help_pages = document_sort_group_help_pages;
	document_sort_by_mode = (DocumentSortMode)(int)EDITOR_GET("text_editor/script_list/sort_scripts_by");
	document_sort_group_help_pages = EDITOR_GET("text_editor/script_list/group_help_pages");
	if (document_sort_by_mode != previous_document_sort_by_mode || document_sort_group_help_pages != previous_document_sort_group_help_pages) {
		sort_document_editors();
	}

	bool previous_highlight_scene_scripts = highlight_scene_scripts;
	highlight_scene_scripts = EDITOR_GET("text_editor/script_list/highlight_scene_scripts");
	if (highlight_scene_scripts && !previous_highlight_scene_scripts) {
		_connect_to_scene();
	}

	document_list->update_editor_settings();

	_update_tab_bar_settings();
	_update_pane_top_spacing();

	ScriptServer::set_reload_scripts_on_save(EDITOR_GET("text_editor/behavior/files/auto_reload_and_parse_scripts_on_save"));

	// Settings apply to the editors of every pane, not only the primary one.
	for (Control *editor : _get_all_pane_editors()) {
		if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(editor)) {
			teb->update_settings();
		}
	}
}

void DocumentEditorContainer::_update_filenames() {
	if (restoring_layout) {
		return;
	}

	// Names are disambiguated across every open document, in every pane.
	LocalVector<Control *> editors;
	LocalVector<TabContainer *> owners;
	LocalVector<int> owner_indices;
	for (TabContainer *pane : panes) {
		for (int i = 0; i < pane->get_tab_count(); i++) {
			editors.push_back(pane->get_tab_control(i));
			owners.push_back(pane);
			owner_indices.push_back(i);
		}
	}

	const int num_open_editors = (int)editors.size();
	PackedStringArray disambiguated_names;
	disambiguated_names.reserve(num_open_editors);
	PackedStringArray full_paths;
	full_paths.reserve(num_open_editors);

	LocalVector<bool> unsaved;
	unsaved.reserve(num_open_editors);

	for (int i = 0; i < num_open_editors; i++) {
		Control *editor = editors[i];
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
		EditorHelp *eh = Object::cast_to<EditorHelp>(editor);
		if (seb) {
			unsaved.push_back(seb->is_unsaved());

			const String &path = seb->get_edited_resource()->get_path();
			const String &name = seb->get_document_name();
			String display_name;

			switch (document_display_name_mode) {
				case DISPLAY_NAME: {
					display_name = name;
				} break;
				case DISPLAY_DIR_AND_NAME: {
					if (!path.get_base_dir().get_file().is_empty()) {
						display_name = path.get_base_dir().get_file().path_join(name);
					} else {
						display_name = name;
					}
				} break;
				case DISPLAY_FULL_PATH: {
					display_name = path;
				} break;
			}
			full_paths.push_back(path);
			disambiguated_names.push_back(display_name);
		}
		if (eh) {
			const String name = eh->get_class().is_empty() ? TTR("Unknown Help Class") : eh->get_class().unquote();
			unsaved.push_back(false);

			full_paths.push_back(name);
			disambiguated_names.push_back(name);
		}
	}

	EditorNode::disambiguate_filenames(full_paths, disambiguated_names);

	for (int i = 0; i < disambiguated_names.size(); i++) {
		String name = disambiguated_names[i];
		if (unsaved[i]) {
			name += "(*)";
		}
		owners[i]->set_tab_title(owner_indices[i], name);
		owners[i]->get_tab_control(owner_indices[i])->set_name(name);
	}

	document_list->update_list();
	_update_document_name_button();
	_update_popout_windows();
}

void DocumentEditorContainer::_files_moved(const String &p_old_file, const String &p_new_file) {
	if (!script_editor_cache->has_section(p_old_file)) {
		return;
	}

	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (seb && seb->edited_file_data.path == p_old_file) {
			seb->edited_file_data.path = p_new_file;
			break;
		}
	}

	Variant state = script_editor_cache->get_value(p_old_file, "state");
	script_editor_cache->erase_section(p_old_file);
	script_editor_cache->set_value(p_new_file, "state", state);

	// If Script, update breakpoints with debugger.
	Array breakpoints = _get_cached_breakpoints_for_script(p_new_file);
	for (int breakpoint : breakpoints) {
		int line = (int)breakpoint + 1;
		EditorDebuggerNode::get_singleton()->set_breakpoint(p_old_file, line, false);
		if (!p_new_file.begins_with("local://") && ResourceLoader::exists(p_new_file, "Script")) {
			EditorDebuggerNode::get_singleton()->set_breakpoint(p_new_file, line, true);
		}
	}
	// This is saved later when we save the editor layout.
}

void DocumentEditorContainer::_file_removed(const String &p_removed_file) {
	LocalVector<Control *> to_close;
	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (seb && seb->edited_file_data.path == p_removed_file) {
			// The script is deleted with no undo, so just close the tab.
			to_close.push_back(pane_editor);
		}
	}

	for (Control *editor : to_close) {
		TabContainer *pane = _pane_of(editor);
		if (pane != nullptr && editor->get_parent() == pane) {
			_close_tab(pane, pane->get_tab_idx_from_control(editor), false);
		}
	}

	// Check closed.
	if (script_editor_cache->has_section(p_removed_file)) {
		Array breakpoints = _get_cached_breakpoints_for_script(p_removed_file);
		for (int breakpoint : breakpoints) {
			EditorDebuggerNode::get_singleton()->set_breakpoint(p_removed_file, (int)breakpoint + 1, false);
		}
		script_editor_cache->erase_section(p_removed_file);
	}
}

void DocumentEditorContainer::_update_find_replace_bar() {
	if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(_get_current_editor())) {
		teb->set_find_replace_bar(find_replace_bar);
	} else {
		find_replace_bar->set_text_edit(nullptr);
		find_replace_bar->hide();
	}
}

void DocumentEditorContainer::_autosave_scripts() {
	save_all_scripts();
}

void DocumentEditorContainer::_update_autosave_timer() {
	if (!autosave_timer->is_inside_tree()) {
		return;
	}

	float autosave_time = EDITOR_GET("text_editor/behavior/files/autosave_interval_secs");
	if (autosave_time > 0) {
		autosave_timer->set_wait_time(autosave_time);
		autosave_timer->start();
	} else {
		autosave_timer->stop();
	}
}

void DocumentEditorContainer::_split_dragged(float) {
	_save_layout();
}

void DocumentEditorContainer::input(const Ref<InputEvent> &p_event) {
	// This feature can be disabled to avoid interfering with other uses of the additional
	// mouse buttons, such as push-to-talk in a VoIP program.
	if (EDITOR_GET("interface/editor/input/mouse_extra_buttons_navigate_history")) {
		const Ref<InputEventMouseButton> mb = p_event;

		// Navigate the script history using additional mouse buttons present on some mice.
		// This must be hardcoded as the editor shortcuts dialog doesn't allow assigning
		// more than one shortcut per action.
		Control *focus_owner = get_viewport()->gui_get_focus_owner();
		if (focus_owner && (this == focus_owner || is_ancestor_of(focus_owner))) {
			if (mb.is_valid() && mb->is_pressed() && is_visible_in_tree()) {
				if (mb->get_button_index() == MouseButton::MB_XBUTTON1) {
					_history_back();
				}

				if (mb->get_button_index() == MouseButton::MB_XBUTTON2) {
					_history_forward();
				}
			}
		}
	}
}

void DocumentEditorContainer::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!is_visible_in_tree() || !p_event->is_pressed()) {
		return;
	}

	Control *focus_owner = get_viewport()->gui_get_focus_owner();
	if (!(focus_owner && (this == focus_owner || is_ancestor_of(focus_owner)))) {
		return;
	}

	if (ED_IS_SHORTCUT("script_editor/next_script", p_event)) {
		document_list->goto_next_document();
		accept_event();
	}
	if (ED_IS_SHORTCUT("script_editor/prev_script", p_event)) {
		document_list->goto_next_document(true);
		accept_event();
	}

	static const HashMap<String, int> shortcut_mappings = {
		{ "script_editor/close_other_tabs", FILE_MENU_CLOSE_OTHER_TABS },
		{ "script_editor/close_tabs_below", FILE_MENU_CLOSE_TABS_BELOW },
		{ "script_editor/close_docs", FILE_MENU_CLOSE_DOCS },
		{ "script_editor/copy_path", FILE_MENU_COPY_PATH },
		{ "script_editor/copy_uid", FILE_MENU_COPY_UID },
		{ "script_editor/show_in_file_system", FILE_MENU_SHOW_IN_FILE_SYSTEM },
		{ "script_editor/move_document_up", FILE_MENU_MOVE_UP },
		{ "script_editor/move_document_down", FILE_MENU_MOVE_DOWN },
		{ "script_editor/sort_documents", FILE_MENU_SORT },
	};

	for (const KeyValue<String, int> &E : shortcut_mappings) {
		if (ED_IS_SHORTCUT(E.key, p_event)) {
			_menu_option(E.value);
			accept_event();
			break;
		}
	}

	if (p_event->is_echo()) {
		return;
	}

	const Callable custom_callback = EditorContextMenuPluginManager::get_singleton()->match_custom_shortcut(EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR, p_event);
	if (custom_callback.is_valid()) {
		EditorContextMenuPlugin::OptionsData context_data = DocumentEditorContainer::get_context_data(get_active_editor());
#ifndef DISABLE_DEPRECATED
		if (p_event->get_meta("_legacy_shortcut", false)) {
			EditorContextMenuPluginManager::get_singleton()->invoke_callback(custom_callback, context_data["selected_resource"]);
			accept_event();
			return;
		}
#endif
		EditorContextMenuPluginManager::get_singleton()->invoke_callback(custom_callback, context_data);
		accept_event();
	}
}

void DocumentEditorContainer::_prepare_file_menu() {
	PopupMenu *p_menu = file_menu->get_popup();
	p_menu->set_item_disabled(p_menu->get_item_index(FILE_MENU_REOPEN_CLOSED), previous_scripts.is_empty());

	ScriptEditorBase *seb = _get_current_editor();
	const Ref<Resource> res = seb ? seb->get_edited_resource() : Ref<Resource>();
	if (res.is_valid()) {
		p_menu->set_item_disabled(p_menu->get_item_index(FILE_MENU_SAVE), res.is_null());
		p_menu->set_item_disabled(p_menu->get_item_index(FILE_MENU_SAVE_AS), res.is_null());
	}
	p_menu->set_item_disabled(p_menu->get_item_index(FILE_MENU_SAVE_ALL), !_has_script_tab());
	// Closing acts on the documents of the active pane, so that is the pane
	// whose documents decide whether there is anything to close.
	TabContainer *active_pane = _active_pane_container();
	const int active_tab_count = (active_pane != nullptr) ? active_pane->get_tab_count() : 0;
	p_menu->set_item_disabled(p_menu->get_item_index(FILE_MENU_CLOSE), active_tab_count < 1);
	p_menu->set_item_disabled(p_menu->get_item_index(FILE_MENU_CLOSE_ALL), active_tab_count < 1);

	_update_layout_presets_menu();

	// Turning the selection into a snippet needs a selection.
	const int snippet_item = p_menu->get_item_index(FILE_MENU_SAVE_SNIPPET);
	if (snippet_item != -1) {
		p_menu->set_item_disabled(snippet_item, !_has_text_selection());
	}

	// The split action turns into a merge action while a second pane exists.
	const int split_item = p_menu->get_item_index(FILE_MENU_SPLIT_EDITOR);
	if (split_item != -1) {
		p_menu->set_item_text(split_item, panes.size() > 1 ? TTRC("Merge Editor Views") : TTRC("Split Editor View"));
		p_menu->set_item_disabled(split_item, _get_current_editor() == nullptr && panes.size() <= 1);
	}

	const Ref<Script> script = res;
	p_menu->set_item_disabled(p_menu->get_item_index(FILE_MENU_SOFT_RELOAD_TOOL), script.is_null() || !script->is_tool());
	p_menu->set_item_disabled(p_menu->get_item_index(FILE_MENU_RUN), script.is_null());
}

void DocumentEditorContainer::_file_menu_closed() {
	PopupMenu *menu = file_menu->get_popup();
	for (int i = 0; i < menu->get_item_count(); i++) {
		menu->set_item_disabled(i, false);
	}
}

struct SortItemData {
	String sort_key;
	int category = 0;
	int index = 0;
	Node *editor = nullptr;

	bool operator<(const SortItemData &id) const {
		if (category == id.category) {
			if (sort_key == id.sort_key) {
				return index < id.index;
			} else {
				return sort_key.filenocasecmp_to(id.sort_key) < 0;
			}
		} else {
			return category < id.category;
		}
	}
};

void DocumentEditorContainer::sort_document_editors() {
	if (restoring_layout) {
		return;
	}
	if (document_sort_by_mode == SORT_BY_NONE && !document_sort_group_help_pages) {
		return;
	}
	// The panes are independent tab bars, so each one is sorted on its own: a
	// document cannot be moved into another pane by sorting.
	for (TabContainer *pane : panes) {
		const int num_open_editors = pane->get_tab_count();

		LocalVector<SortItemData> sort_items;
		sort_items.reserve(num_open_editors);

		for (int i = 0; i < num_open_editors; i++) {
			Control *editor = pane->get_tab_control(i);
			SortItemData item;
			item.index = i;
			item.editor = editor;

			ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
			EditorHelp *eh = Object::cast_to<EditorHelp>(editor);
			if (seb) {
				item.category = 0;

				switch (document_sort_by_mode) {
					case SORT_BY_NAME: {
						const String &name = seb->get_document_name();
						item.sort_key = name.to_lower();
					} break;
					case SORT_BY_PATH: {
						const String &path = seb->get_edited_resource()->get_path();
						item.sort_key = path;
					} break;
					default:
						break;
				}
			}
			if (eh) {
				const String name = eh->get_class().is_empty() ? TTR("Unknown Help Class") : eh->get_class().unquote();

				item.category = document_sort_group_help_pages ? 1 : 0;
				item.sort_key = name.to_lower();
			}
			sort_items.push_back(item);
		}

		sort_items.sort();

		for (int i = 0; i < (int)sort_items.size(); i++) {
			pane->move_child(sort_items[i].editor, i);
		}
	}

	document_list->update_list();
}

// Rebuilds the saved panes into the given pane, and returns the control that
// stands for the part of the layout that was rebuilt: a pane for a leaf, or the
// split holding the whole subtree.
Control *DocumentEditorContainer::_restore_pane_layout(const Variant &p_node, TabContainer *p_container) {
	if (p_node.get_type() != Variant::DICTIONARY || p_container == nullptr) {
		return p_container;
	}

	Dictionary node = p_node;

	if (node.has("first") && node.has("second")) {
		// The first half goes where the container is, and the second half into a
		// pane split off the whole of it.
		Control *first = _restore_pane_layout(node["first"], p_container);
		if (first == nullptr) {
			return p_container;
		}

		const bool vertical = String(node.get("direction", "horizontal")) == "vertical";
		TabContainer *second = Object::cast_to<TabContainer>(_split_control(first, vertical ? DROP_ZONE_BOTTOM : DROP_ZONE_RIGHT));
		if (second == nullptr) {
			return first;
		}

		_restore_pane_layout(node["second"], second);

		if (SplitContainer *split = Object::cast_to<SplitContainer>(second->get_parent())) {
			split->set_split_offset(node.get("offset", 0));
			return split;
		}
		return first;
	}

	// A pane: open its documents into it, which is where the active pane points.
	active_pane = MAX(0, _pane_index(p_container));
	const List<String> extensions = _get_recognized_extensions();
	const Array scripts = node.get("scripts", Array());
	for (const Variant &script : scripts) {
		const String path = script;
		if (path.is_empty() || !_script_exists(path)) {
			continue;
		}

		// A document that is already open is moved instead of opened twice.
		bool moved = false;
		for (TabContainer *other : panes) {
			if (other == p_container) {
				continue;
			}
			for (int i = 0; i < other->get_tab_count(); i++) {
				ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(other->get_tab_control(i));
				if (seb != nullptr && seb->get_edited_resource()->get_path() == path) {
					_move_tab_to_pane(other, i, p_container);
					moved = true;
					break;
				}
			}
			if (moved) {
				break;
			}
		}
		if (moved) {
			continue;
		}

		// Scripts load as scripts, everything else (shaders, text files) as text.
		bool is_script = ResourceCache::get_ref(path).is_valid();
		if (!is_script && path.is_resource_file()) {
			is_script = extensions.find(path.get_extension());
		}
		if (is_script) {
			Ref<Resource> resource = ResourceLoader::load(path);
			if (resource.is_valid()) {
				edit(resource, false);
			}
		} else {
			Error error = OK;
			Ref<TextFile> text_file = _load_text_file(path, &error);
			if (text_file.is_valid()) {
				edit(text_file, false);
			}
		}
	}
	const Array pane_helps = node.get("help", Array());
	for (const Variant &help : pane_helps) {
		const String help_class = help;
		if (!help_class.is_empty()) {
			_help_class_open(help_class);
		}
	}

	// Go back to the document that was being edited in this pane.
	const String selected = node.get("selected", String());
	if (!selected.is_empty()) {
		for (int i = 0; i < p_container->get_tab_count(); i++) {
			Control *editor = p_container->get_tab_control(i);
			ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
			if (seb != nullptr && seb->get_edited_resource()->get_path() == selected) {
				p_container->set_current_tab(i);
				break;
			}
			EditorHelp *eh = Object::cast_to<EditorHelp>(editor);
			if (eh != nullptr && eh->get_class() == selected) {
				p_container->set_current_tab(i);
				break;
			}
		}
	}

	return p_container;
}

// The sidebar splits are outside the pane tree, and every layout format saves
// their offsets: both restoring paths have to read them back, or the split
// positions do not survive a restart.
void DocumentEditorContainer::_restore_split_offsets(const Ref<ConfigFile> &p_layout) {
	if (p_layout->has_section_key(config_section, "script_split_offset")) {
		script_split->set_split_offset(p_layout->get_value(config_section, "script_split_offset"));
	}

	if (p_layout->has_section_key(config_section, "list_split_offset")) {
		list_split->set_split_offset(p_layout->get_value(config_section, "list_split_offset"));
	}
}

void DocumentEditorContainer::set_window_layout(Ref<ConfigFile> p_layout) {
	if (!p_layout->has_section_key(config_section, "open_scripts") && !p_layout->has_section_key(config_section, "open_help")) {
		return;
	}

	// A layout with panes is restored pane by pane.
	if (p_layout->has_section_key(config_section, "script_panes")) {
		const Variant pane_layout = p_layout->get_value(config_section, "script_panes");
		if (pane_layout.get_type() == Variant::DICTIONARY) {
			restoring_layout = true;
			_set_script_zoom_factor(p_layout->get_value(config_section, "zoom_factor", 1.0f));
			ScriptEditorNavigationMarker::get_singleton()->init_begin();
			_restore_pane_layout(pane_layout, tab_container);
			ScriptEditorNavigationMarker::get_singleton()->init_end();
			restoring_layout = false;
			_restore_split_offsets(p_layout);
			_update_filenames();
			document_list->update_list();
			return;
		}
	}

	Array scripts = p_layout->get_value(config_section, "open_scripts");
	Array helps;
	if (p_layout->has_section_key(config_section, "open_help")) {
		helps = p_layout->get_value(config_section, "open_help");
	}

	restoring_layout = true;

	HashSet<String> loaded_scripts;
	List<String> extensions = _get_recognized_extensions();

	ScriptEditorNavigationMarker::get_singleton()->init_begin();

	_set_script_zoom_factor(p_layout->get_value(config_section, "zoom_factor", 1.0f));

	for (const Variant &v : scripts) {
		String path = v;

		Dictionary script_info = v;
		if (!script_info.is_empty()) {
			path = script_info["path"];
		}

		if (!_script_exists(path)) {
			if (script_editor_cache->has_section(path)) {
				script_editor_cache->erase_section(path);
			}
			continue;
		} else if (!path.is_resource_file() && !EditorNode::get_singleton()->is_scene_open(path.get_slice("::", 0))) {
			continue;
		}
		loaded_scripts.insert(path);

		bool is_script = false;
		if (path.is_resource_file()) {
			is_script = extensions.find(path.get_extension());
		} else {
			Ref<Script> scr = ResourceCache::get_ref(path);
			if (scr.is_valid()) {
				is_script = true;
			} else {
				continue;
			}
		}

		if (is_script) {
			Ref<Resource> scr = ResourceLoader::load(path);
			if (scr.is_null()) {
				continue;
			}
			if (!edit(scr, false)) {
				continue;
			}
		} else {
			Error error;
			Ref<TextFile> text_file = _load_text_file(path, &error);
			if (error != OK || text_file.is_null()) {
				continue;
			}
			if (!edit(text_file, false)) {
				continue;
			}
		}

		if (!script_info.is_empty()) {
			if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(tab_container->get_tab_control(tab_container->get_tab_count() - 1))) {
				teb->set_edit_state(script_info["state"]);
			}
		}
	}

	for (int i = 0; i < helps.size(); i++) {
		String path = helps[i];
		if (path.is_empty()) { // invalid, skip
			continue;
		}
		_help_class_open(path);
	}

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		tab_container->get_tab_control(i)->set_meta("__editor_pass", Variant());
	}

	_restore_split_offsets(p_layout);

	// Remove any deleted editors that have been removed between launches.
	// and if a Script, register breakpoints with the debugger.
	Vector<String> cached_editors = script_editor_cache->get_sections();
	for (const String &E : cached_editors) {
		if (loaded_scripts.has(E)) {
			continue;
		}

		if (!_script_exists(E)) {
			script_editor_cache->erase_section(E);
			continue;
		}

		Array breakpoints = _get_cached_breakpoints_for_script(E);
		for (int breakpoint : breakpoints) {
			EditorDebuggerNode::get_singleton()->set_breakpoint(E, (int)breakpoint + 1, true);
		}
	}

	restoring_layout = false;

	_update_filenames();
	sort_document_editors();
	ScriptEditorNavigationMarker::get_singleton()->init_end();

	bool selected_saved_history = false;
	if (p_layout->has_section_key(config_section, "selected_script")) {
		String selected_script = p_layout->get_value(config_section, "selected_script");
		// If the selected script is not in the list of open scripts, select nothing.
		for (int i = 0; i < tab_container->get_tab_count() && !selected_script.is_empty(); i++) {
			ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
			if (seb && seb->get_edited_resource()->get_path() == selected_script) {
				_go_to_tab(i, true);
				selected_saved_history = true;
				break;
			}
		}
	}
	ensure_select_current();

	if (!selected_saved_history && tab_container->get_child_count() > 0) {
		ScriptEditorNavigationMarker::get_singleton()->locate_begin();
		Control *tselected = tab_container->get_current_tab_control();
		TextEditorBase *teb = Object::cast_to<TextEditorBase>(tselected);
		if (teb && teb->get_code_editor()) {
			teb->get_code_editor()->trigger_history_save_on_navigate();
		}

		EditorHelp *eh = Object::cast_to<EditorHelp>(tselected);
		if (eh) {
			eh->trigger_history_save_on_navigate();
		}
		ScriptEditorNavigationMarker::get_singleton()->locate_end();
	}
}

// Describes how the panes are laid out and which documents each of them holds,
// as a tree: leaves are panes and inner nodes are splits.
Variant DocumentEditorContainer::_serialize_pane_layout(const Node *p_node) const {
	if (const TabContainer *pane = Object::cast_to<TabContainer>(p_node)) {
		Dictionary leaf;
		Array scripts;
		Array pane_helps;
		String selected;
		for (int i = 0; i < pane->get_tab_count(); i++) {
			Control *editor = pane->get_tab_control(i);
			if (ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor)) {
				const String path = seb->get_edited_resource()->get_path();
				if (path.is_empty()) {
					continue;
				}
				if (pane->get_current_tab_control() == editor) {
					selected = path;
				}
				scripts.push_back(path);
			} else if (EditorHelp *eh = Object::cast_to<EditorHelp>(editor)) {
				if (pane->get_current_tab_control() == editor) {
					selected = eh->get_class();
				}
				pane_helps.push_back(eh->get_class());
			}
		}
		leaf["scripts"] = scripts;
		leaf["help"] = pane_helps;
		leaf["selected"] = selected;
		return leaf;
	}

	if (const SplitContainer *split = Object::cast_to<SplitContainer>(p_node)) {
		if (split->get_child_count() < 2) {
			// A split without two children is not a split yet.
			return split->get_child_count() == 1 ? _serialize_pane_layout(split->get_child(0)) : Variant();
		}
		Dictionary node;
		node["direction"] = Object::cast_to<VSplitContainer>(split) != nullptr ? "vertical" : "horizontal";
		node["offset"] = split->get_split_offset();
		node["first"] = _serialize_pane_layout(split->get_child(0));
		node["second"] = _serialize_pane_layout(split->get_child(1));
		return node;
	}

	return Variant();
}

void DocumentEditorContainer::get_window_layout(Ref<ConfigFile> p_layout) {
	Array scripts;
	Array helps;
	String selected_script;
	for (TabContainer *pane : panes) {
		for (int i = 0; i < pane->get_tab_count(); i++) {
			Control *editor = pane->get_tab_control(i);
			if (ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor)) {
				const String path = seb->get_edited_resource()->get_path();
				if (path.is_empty()) {
					continue;
				}

				if (pane == tab_container && pane->get_current_tab_control() == editor) {
					selected_script = path;
				}

				_save_editor_state(seb);
				scripts.push_back(path);
			}

			if (EditorHelp *eh = Object::cast_to<EditorHelp>(editor)) {
				helps.push_back(eh->get_class());
			}
		}
	}

	p_layout->set_value(config_section, "script_panes", _serialize_pane_layout(editor_split));
	p_layout->set_value(config_section, "open_scripts", scripts);
	p_layout->set_value(config_section, "selected_script", selected_script);
	p_layout->set_value(config_section, "open_help", helps);
	p_layout->set_value(config_section, "script_split_offset", script_split->get_split_offset());
	p_layout->set_value(config_section, "list_split_offset", list_split->get_split_offset());
	p_layout->set_value(config_section, "zoom_factor", zoom_factor);

	// Save the cache.
	script_editor_cache->save(EditorPaths::get_singleton()->get_project_settings_dir().path_join(cache_path));
}

void DocumentEditorContainer::_help_class_open(const String &p_class) {
	if (p_class.is_empty()) {
		return;
	}

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));

		if (eh && eh->get_class() == p_class) {
			_go_to_tab(i, true);
			return;
		}
	}

	EditorHelp *eh = memnew(EditorHelp);

	eh->connect("go_to_help", callable_mp(this, &DocumentEditorContainer::_help_class_goto));
	eh->connect("_request_save_new_history", callable_mp(this, &DocumentEditorContainer::_save_new_history).bind(eh));

	// Go to the class first, so it has a class name when switching to it.
	ScriptEditorNavigationMarker::get_singleton()->locate_begin();
	eh->go_to_class(p_class);
	ScriptEditorNavigationMarker::get_singleton()->locate_end();

	tab_container->add_child(eh);
	_update_filenames();
	_go_to_tab(tab_container->get_tab_count() - 1);
	sort_document_editors();
	_add_recent_script(p_class);
	_save_layout();
}

void DocumentEditorContainer::_help_class_goto(const String &p_desc) {
	const String cname = p_desc.get_slicec(':', 1);

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));

		if (eh && eh->get_class() == cname) {
			ScriptEditorNavigationMarker::get_singleton()->locate_begin();
			eh->go_to_help(p_desc);
			ScriptEditorNavigationMarker::get_singleton()->locate_end();
			_go_to_tab(i);
			return;
		}
	}

	EditorHelp *eh = memnew(EditorHelp);

	eh->connect("go_to_help", callable_mp(this, &DocumentEditorContainer::_help_class_goto));
	eh->connect("_request_save_new_history", callable_mp(this, &DocumentEditorContainer::_save_new_history).bind(eh));

	ScriptEditorNavigationMarker::get_singleton()->locate_begin();
	eh->go_to_help(p_desc);
	ScriptEditorNavigationMarker::get_singleton()->locate_end();

	tab_container->add_child(eh);
	_update_filenames();
	_go_to_tab(tab_container->get_tab_count() - 1);
	sort_document_editors();
	_add_recent_script(eh->get_class());
	_save_layout();
}

void DocumentEditorContainer::update_doc(const String &p_name) {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));
		if (eh && eh->get_class() == p_name) {
			eh->update_doc();
			return;
		}
	}
}

void DocumentEditorContainer::clear_docs_from_script(const Ref<Script> &p_script) {
	ERR_FAIL_COND(p_script.is_null());

	for (const DocData::ClassDoc &cd : p_script->get_documentation()) {
		if (EditorHelp::get_doc_data()->has_doc(cd.name)) {
			EditorHelp::get_doc_data()->remove_doc(cd.name);
		}
	}
}

void DocumentEditorContainer::update_docs_from_script(const Ref<Script> &p_script) {
	ERR_FAIL_COND(p_script.is_null());

	for (const DocData::ClassDoc &cd : p_script->get_documentation()) {
		EditorHelp::get_doc_data()->add_doc(cd);
		update_doc(cd.name);
	}
}

void ScriptEditor::rename_symbol(const String &p_symbol, const EditorLanguage::LookupResult &p_lookup) {
	FindInFiles::get_singleton()->get_container()->create_rename_control(p_symbol, p_lookup);
	EditorDockManager::get_singleton()->focus_dock(FindInFiles::get_singleton()->get_container());

	LineEdit *name_edit = FindInFiles::get_singleton()->get_container()->get_search_control()->get_rename_line_edit();
	callable_mp((Control *)name_edit, &Control::grab_focus).call_deferred(false);
	callable_mp(name_edit, &LineEdit::select_all).call_deferred();
}

void DocumentEditorContainer::_update_selected_editor_menu() {
	for (ScriptEditorBase::EditMenusBase *editor_menu : editor_menus) {
		editor_menu->set_visible(editor_menu->handles(_get_current_editor()));
	}

	PopupMenu *search_popup = script_search_menu->get_popup();
	search_popup->clear();
	// What the search menu offers depends on the current document, which lives
	// in the active pane.
	TabContainer *pane = _active_pane_container();
	Control *current = (pane != nullptr) ? pane->get_current_tab_control() : nullptr;
	const int tab_count = (pane != nullptr) ? pane->get_tab_count() : 0;
	if (Object::cast_to<EditorHelp>(current)) {
		search_popup->add_shortcut(ED_SHORTCUT("script_editor/find", TTRC("Find..."), KeyModifierMask::CMD_OR_CTRL | Key::F), HELP_SEARCH_FIND);
		search_popup->add_shortcut(ED_SHORTCUT("script_editor/find_next", TTRC("Find Next"), Key::F3), HELP_SEARCH_FIND_NEXT);
		search_popup->add_shortcut(ED_SHORTCUT("script_editor/find_previous", TTRC("Find Previous"), KeyModifierMask::SHIFT | Key::F3), HELP_SEARCH_FIND_PREVIOUS);
		search_popup->add_separator();
		search_popup->add_shortcut(ED_GET_SHORTCUT("editor/find_in_files"), SEARCH_IN_FILES);
		search_popup->add_shortcut(ED_GET_SHORTCUT("editor/replace_in_files"), REPLACE_IN_FILES);
		script_search_menu->show();
	} else {
		if (tab_count == 0) {
			search_popup->add_shortcut(ED_GET_SHORTCUT("editor/find_in_files"), SEARCH_IN_FILES);
			search_popup->add_shortcut(ED_GET_SHORTCUT("editor/replace_in_files"), REPLACE_IN_FILES);
			script_search_menu->show();
		} else {
			script_search_menu->hide();
		}
	}
}

void DocumentEditorContainer::_update_history_pos(int p_new_pos) {
	history_pos = p_new_pos;

	// The document the history points at can be in any pane by now, so the pane
	// holding it is the one to switch.
	Control *control = Object::cast_to<Control>(history[history_pos].control);
	TabContainer *pane = (control != nullptr) ? _pane_of(control) : nullptr;
	if (pane != nullptr && control->get_parent() == pane) {
		pane->set_current_tab(pane->get_tab_idx_from_control(control));
		_set_active_pane(pane);
	}

	Node *n = history[history_pos].control;

	if (ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(n)) {
		if (TextEditorBase *teb = Object::cast_to<TextEditorBase>(n)) {
			teb->set_edit_state(history[history_pos].state);
		}
		seb->ensure_focus();

		Ref<Script> scr = seb->get_edited_resource();
		if (scr.is_valid()) {
			ScriptEditor::get_singleton()->notify_script_changed(scr);
		}

		seb->validate_script();
	}

	if (EditorHelp *eh = Object::cast_to<EditorHelp>(n)) {
		eh->set_scroll(history[history_pos].state["row"]);
		eh->set_focused();
	}

	n->set_meta("__editor_pass", ++edit_pass);
	_update_history_arrows();
}

void DocumentEditorContainer::_history_forward() {
	if (history_pos < history.size() - 1) {
		ScriptEditorNavigationMarker::get_singleton()->traverse_begin();
		_update_history_pos(history_pos + 1);
		ScriptEditorNavigationMarker::get_singleton()->traverse_end();
	}
}

void DocumentEditorContainer::_history_back() {
	if (history_pos > 0) {
		ScriptEditorNavigationMarker::get_singleton()->traverse_begin();
		_update_history_pos(history_pos - 1);
		ScriptEditorNavigationMarker::get_singleton()->traverse_end();
	}
}

void DocumentEditorContainer::_roll_back_to_pre_tab() {
	Control *tselected = tab_container->get_current_tab_control();
	if (history_pos == -1 || history[history_pos].control != tselected) {
		return;
	}

	int pos = -1;
	for (int i = history_pos - 1; i >= 0; i--) {
		if (history[i].control != tselected) {
			pos = i;
			break;
		}
	}

	if (pos == -1) {
		history_pos = -1;
		history.clear();
	} else {
		_update_history_pos(pos);
		history.resize(history_pos + 1);
	}
}

Vector<Ref<Script>> DocumentEditorContainer::get_open_scripts() const {
	Vector<Ref<Script>> out_scripts = Vector<Ref<Script>>();

	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (!seb) {
			continue;
		}

		Ref<Script> scr = seb->get_edited_resource();
		if (scr.is_valid()) {
			out_scripts.push_back(scr);
		}
	}

	return out_scripts;
}

ScriptEditorBase *DocumentEditorContainer::get_resource_editor(const Ref<Resource> &p_res) const {
	for (Control *pane_editor : _get_all_pane_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(pane_editor);
		if (seb && seb->get_edited_resource() == p_res) {
			return seb;
		}
	}
	return nullptr;
}

bool DocumentEditorContainer::script_goto_method(Ref<Script> p_script, const String &p_method) {
	int line = p_script->get_member_line(p_method);

	if (line == -1) {
		return false;
	}

	return edit(p_script, line, 0);
}

void DocumentEditorContainer::set_live_auto_reload_running_scripts(bool p_enabled) {
	auto_reload_running_scripts = p_enabled;
}

void DocumentEditorContainer::_calculate_script_name_button_size() {
	Ref<Font> font = script_name_button->get_theme_font(SceneStringName(font), SNAME("Button"));
	HorizontalAlignment alignment = script_name_button->get_text_alignment();
	int font_size = script_name_button->get_theme_font_size(SceneStringName(font_size), SNAME("Button"));
	String text = script_name_button->get_text();
	int jst_flags = TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_SKIP_LAST_LINE | TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE;
	TextServer::Direction direction = TextServer::Direction(script_name_button->get_text_direction());
	Vector2 text_size = font->get_string_size(text, alignment, -1, font_size, jst_flags, direction, TextServer::ORIENTATION_HORIZONTAL);

	script_name_width = text_size.x + script_name_button->get_theme_stylebox(CoreStringName(normal))->get_content_margin(SIDE_LEFT) + script_name_button->get_theme_stylebox(CoreStringName(normal))->get_content_margin(SIDE_RIGHT);
	_calculate_script_name_button_ratio();
}

void DocumentEditorContainer::_calculate_script_name_button_ratio() {
	const float total_width = script_name_button_hbox->get_size().width;
	if (total_width <= 0) {
		return;
	}

	// Make the ratios a fraction bigger, to avoid unnecessary trimming.
	const float extra_ratio = 4 / total_width;

	const float script_name_ratio = MIN(1, script_name_width / total_width + extra_ratio);
	script_name_button->set_stretch_ratio(script_name_ratio);

	float ratio_left = 1 - script_name_ratio;
	script_name_button_left_spacer->set_stretch_ratio(ratio_left / 2);
	script_name_button_right_spacer->set_stretch_ratio(ratio_left / 2);
}

void DocumentEditorContainer::_focus_changed(Node *p_node) {
	if (panes.size() <= 1) {
		return;
	}

	Control *focused = Object::cast_to<Control>(p_node);
	if (focused == nullptr) {
		return;
	}

	TabContainer *pane = nullptr;
	for (TabContainer *candidate : panes) {
		if (candidate->is_ancestor_of(focused)) {
			pane = candidate;
			break;
		}
	}
	if (pane == nullptr) {
		// Focus moved outside the editing panes (file list, menus...).
		return;
	}

	const int new_active = _pane_index(pane);
	if (new_active < 0 || active_pane == new_active) {
		return;
	}

	// The user started working in the other pane: follow it everywhere the
	// current document matters, including the name shown at the top, the
	// outline (which lists the members of the active document) and the menus
	// that depend on whether that document is a script or a help page.
	active_pane = new_active;
	_update_document_name_button();
	document_list->update_list();
	_update_selected_editor_menu();
	_update_online_doc();
	document_outline->update_outline();
	document_outline->update_visibility();
}

void DocumentEditorContainer::_update_document_name_button() {
	TabContainer *pane = _active_pane_container();
	if (pane->get_current_tab() >= 0) {
		const String &name = pane->get_tab_title(pane->get_current_tab());
		script_name_button->set_text(name);
		script_name_button->show();
		_calculate_script_name_button_size();
	} else {
		script_name_button->hide();
		script_name_button->set_text(String());
	}
}

void DocumentEditorContainer::_help_search(const String &p_text) {
	ScriptEditor::get_singleton()->open_help_search_dialog(p_text);
}

void DocumentEditorContainer::register_syntax_highlighter(const Ref<EditorSyntaxHighlighter> &p_syntax_highlighter) {
	ERR_FAIL_COND(p_syntax_highlighter.is_null());

	if (!syntax_highlighters.has(p_syntax_highlighter)) {
		syntax_highlighters.push_back(p_syntax_highlighter);
	}
}

void DocumentEditorContainer::unregister_syntax_highlighter(const Ref<EditorSyntaxHighlighter> &p_syntax_highlighter) {
	ERR_FAIL_COND(p_syntax_highlighter.is_null());

	syntax_highlighters.erase(p_syntax_highlighter);
}

int DocumentEditorContainer::script_editor_func_count = 0;
CreateScriptEditorFunc DocumentEditorContainer::script_editor_funcs[DocumentEditorContainer::SCRIPT_EDITOR_FUNC_MAX];

void DocumentEditorContainer::register_create_script_editor_function(CreateScriptEditorFunc p_func) {
	ERR_FAIL_COND(script_editor_func_count == SCRIPT_EDITOR_FUNC_MAX);
	script_editor_funcs[script_editor_func_count++] = p_func;
}

void DocumentEditorContainer::_validation_updated() {
	document_list->update_list();

	document_outline->update_outline();
	document_outline->update_visibility();
}

void DocumentEditorContainer::_set_script_zoom_factor(float p_zoom_factor) {
	if (zoom_factor == p_zoom_factor) {
		return;
	}

	zoom_factor = p_zoom_factor;
}

void DocumentEditorContainer::_update_code_editor_zoom_factor(CodeTextEditor *p_code_text_editor) {
	if (p_code_text_editor && p_code_text_editor->is_visible_in_tree() && zoom_factor != p_code_text_editor->get_zoom_factor()) {
		p_code_text_editor->set_zoom_factor(zoom_factor);
	}
}

DocumentEditorContainer::DocumentEditorContainer(bool p_is_main_editor, const String &p_config_section, const String &p_cache_path) {
	is_main_editor = p_is_main_editor;
	config_section = p_config_section;
	cache_path = p_cache_path;

	script_editor_cache.instantiate();
	script_editor_cache->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join(cache_path));

	VBoxContainer *main_container = memnew(VBoxContainer);
	add_child(main_container);

	menu_hb = memnew(HBoxContainer);
	main_container->add_child(menu_hb);

	script_split = memnew(HSplitContainer);
	main_container->add_child(script_split);
	script_split->set_v_size_flags(SIZE_EXPAND_FILL);
	script_split->set_drag_nested_intersections(true);

#ifdef ANDROID_ENABLED
	virtual_keyboard_spacer = memnew(Control);
	virtual_keyboard_spacer->set_h_size_flags(SIZE_EXPAND_FILL);
	main_container->add_child(virtual_keyboard_spacer);
#endif

	list_split = memnew(VSplitContainer);
	script_split->add_child(list_split);
	list_split->set_v_size_flags(SIZE_EXPAND_FILL);
	list_split->set_drag_nested_intersections(true);

	document_list = memnew(DocumentList(this));

	// The documents are already listed as tabs, so the sidebar keeps only the
	// outline. The list stays alive to keep being updated, but out of the way.
	main_container->add_child(document_list);
	document_list->hide();
	script_split->set_split_offset(200 * EDSCALE);

	document_outline = memnew(DocumentOutline(this));
	document_outline->set_custom_minimum_size(Size2(0, 90));
	document_outline->set_v_size_flags(SIZE_EXPAND_FILL);
	list_split->add_child(document_outline);

	VBoxContainer *code_editor_container = memnew(VBoxContainer);
	script_split->add_child(code_editor_container);

	// Room above the panes for the Expressive tab lift. Resized in _update_pane_top_spacing().
	pane_top_spacer = memnew(Control);
	pane_top_spacer->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	code_editor_container->add_child(pane_top_spacer);

	editor_split = memnew(HSplitContainer);
	code_editor_container->add_child(editor_split);
	editor_split->set_h_size_flags(SIZE_EXPAND_FILL);
	editor_split->set_v_size_flags(SIZE_EXPAND_FILL);
	editor_split->connect(SceneStringName(resized), callable_mp(this, &DocumentEditorContainer::_update_drop_overlay_rect));

	// Highlight shown while a document tab is dragged over the area where a
	// second pane can be created. It is also the drop target for that gesture,
	// since the code editors below it stop drag propagation.
	split_drop_overlay = memnew(Panel);
	split_drop_overlay->set_as_top_level(true);
	// Hidden until a drag starts; _apply_drop_overlay() sets the real filter (STOP for a drop target,
	// IGNORE for a hint-only highlight) when it appears.
	split_drop_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	split_drop_overlay->set_drag_forwarding(Callable(), callable_mp(this, &DocumentEditorContainer::_can_drop_on_editor), callable_mp(this, &DocumentEditorContainer::_drop_on_editor));
	split_drop_overlay->hide();
	code_editor_container->add_child(split_drop_overlay);

	// Hint and code preview shown inside the highlight, telling what releasing
	// there does. They must not take part in drag-and-drop, or they would block
	// the drop target. Their theme-dependent styling is applied once the editor
	// theme is available.
	split_drop_box = memnew(VBoxContainer);
	split_drop_box->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	split_drop_box->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	split_drop_box->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	split_drop_box->add_theme_constant_override("separation", 8);
	split_drop_overlay->add_child(split_drop_box);

	split_drop_hint = memnew(Label);
	split_drop_hint->set_text(TTRC("Drop here to split the view"));
	split_drop_hint->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	split_drop_hint->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	split_drop_hint->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	split_drop_box->add_child(split_drop_hint);

	// Menu opened by right-clicking a document tab.
	// Asking for the prefix when turning the selection into a snippet.
	snippet_save_dialog = memnew(ConfirmationDialog);
	snippet_save_dialog->set_title(TTRC("Save Selection as Snippet"));
	snippet_save_dialog->set_ok_button_text(TTRC("Save"));
	snippet_save_dialog->set_flag(Window::FLAG_RESIZE_DISABLED, true);
	VBoxContainer *snippet_dialog_box = memnew(VBoxContainer);
	snippet_save_dialog->add_child(snippet_dialog_box);
	snippet_prefix_edit = memnew(LineEdit);
	snippet_prefix_edit->set_placeholder(TTRC("Snippet prefix"));
	snippet_prefix_edit->set_custom_minimum_size(Size2(280, 0) * EDSCALE);
	snippet_dialog_box->add_child(snippet_prefix_edit);
	snippet_save_dialog->register_text_enter(snippet_prefix_edit);
	snippet_prefix_edit->connect(SceneStringName(text_changed), callable_mp(this, &DocumentEditorContainer::_update_snippet_dialog_ok));
	snippet_save_dialog->connect(SceneStringName(confirmed), callable_mp(this, &DocumentEditorContainer::_on_save_snippet_confirmed));
	add_child(snippet_save_dialog);

	// Naming a layout when it is saved, and picking one to delete.
	layout_save_dialog = memnew(ConfirmationDialog);
	layout_save_dialog->set_title(TTRC("Save Layout"));
	layout_save_dialog->set_ok_button_text(TTRC("Save"));
	layout_save_dialog->set_flag(Window::FLAG_RESIZE_DISABLED, true);
	VBoxContainer *layout_dialog_box = memnew(VBoxContainer);
	layout_save_dialog->add_child(layout_dialog_box);
	layout_name_edit = memnew(LineEdit);
	layout_name_edit->set_placeholder(TTRC("Layout name"));
	layout_name_edit->set_custom_minimum_size(Size2(280, 0) * EDSCALE);
	layout_dialog_box->add_child(layout_name_edit);
	layout_name_edit->connect(SceneStringName(text_changed), callable_mp(this, &DocumentEditorContainer::_update_layout_save_ok_unused));
	layout_save_dialog->register_text_enter(layout_name_edit);
	layout_save_dialog->connect(SceneStringName(confirmed), callable_mp(this, &DocumentEditorContainer::_on_save_layout_confirmed));
	add_child(layout_save_dialog);

	layout_delete_dialog = memnew(ConfirmationDialog);
	layout_delete_dialog->set_title(TTRC("Delete Layout"));
	layout_delete_dialog->set_ok_button_text(TTRC("Delete"));
	layout_delete_list = memnew(ItemList);
	layout_delete_list->set_custom_minimum_size(Size2(280, 160) * EDSCALE);
	layout_delete_dialog->add_child(layout_delete_list);
	layout_delete_list->connect(SceneStringName(item_selected), callable_mp(this, &DocumentEditorContainer::_on_layout_delete_selected));
	layout_delete_dialog->connect(SceneStringName(confirmed), callable_mp(this, &DocumentEditorContainer::_on_delete_layout_confirmed));
	add_child(layout_delete_dialog);

	tab_context_menu = memnew(PopupMenu);
	add_child(tab_context_menu);
	tab_context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &DocumentEditorContainer::_tab_menu_option));
	// Hovering an entry that moves the document highlights where it will go.
	tab_context_menu->connect("id_focused", callable_mp(this, &DocumentEditorContainer::_tab_menu_option_focused));
	tab_context_menu->connect("popup_hide", callable_mp(this, &DocumentEditorContainer::_tab_context_menu_hidden));

	tab_container = memnew(TabContainer);
	tab_container->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	editor_split->add_child(tab_container);
	tab_container->set_h_size_flags(SIZE_EXPAND_FILL);
	tab_container->set_v_size_flags(SIZE_EXPAND_FILL);
	_setup_pane(tab_container);
	panes.push_back(tab_container);

	find_replace_bar = memnew(FindReplaceBar);
	code_editor_container->add_child(find_replace_bar);
	find_replace_bar->hide();

	ED_SHORTCUT("script_editor/sort_documents", TTRC("Sort Documents"));
	ED_SHORTCUT("script_editor/move_document_up", TTRC("Move Document Up"));
	ED_SHORTCUT("script_editor/move_document_down", TTRC("Move Document Down"));
	// FIXME: These should be `Key::GREATER` and `Key::LESS` but those don't work.
	ED_SHORTCUT("script_editor/next_script", TTRC("Next Script"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::PERIOD);
	ED_SHORTCUT("script_editor/prev_script", TTRC("Previous Script"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::COMMA);

	ED_SHORTCUT("script_editor/save", TTRC("Save"), KeyModifierMask::ALT | KeyModifierMask::CMD_OR_CTRL | Key::S);
	ED_SHORTCUT("script_editor/save_as", TTRC("Save As..."));
	ED_SHORTCUT("script_editor/save_all", TTRC("Save All"), KeyModifierMask::SHIFT | KeyModifierMask::ALT | Key::S);
	ED_SHORTCUT_OVERRIDE("script_editor/save_all", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::S);

	ED_SHORTCUT("script_editor/reload_script_soft", TTRC("Soft Reload Tool Script"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::R);
	ED_SHORTCUT("script_editor/run_file", TTRC("Run"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::X);

	ED_SHORTCUT("script_editor/copy_path", TTRC("Copy File Path"));
	ED_SHORTCUT("script_editor/copy_uid", TTRC("Copy File UID"));
	ED_SHORTCUT("script_editor/show_in_file_system", TTRC("Show in FileSystem"));

	ED_SHORTCUT_ARRAY("script_editor/history_previous", TTRC("History Previous"), { int32_t(KeyModifierMask::ALT | Key::LEFT), int32_t(Key::BACK) });
	ED_SHORTCUT_ARRAY("script_editor/history_next", TTRC("History Next"), { int32_t(KeyModifierMask::ALT | Key::RIGHT), int32_t(Key::FORWARD) });
	ED_SHORTCUT_OVERRIDE("script_editor/history_previous", "macos", KeyModifierMask::ALT | KeyModifierMask::META | Key::LEFT);
	ED_SHORTCUT_OVERRIDE("script_editor/history_next", "macos", KeyModifierMask::ALT | KeyModifierMask::META | Key::RIGHT);

	ED_SHORTCUT("script_editor/close_file", TTRC("Close"), KeyModifierMask::CMD_OR_CTRL | Key::W);
	ED_SHORTCUT("script_editor/close_all", TTRC("Close All"));
	ED_SHORTCUT("script_editor/close_other_tabs", TTRC("Close Other Tabs"));
	ED_SHORTCUT("script_editor/close_tabs_below", TTRC("Close Tabs Below"));
	ED_SHORTCUT("script_editor/close_docs", TTRC("Close Docs"));

	ED_SHORTCUT("script_editor/toggle_files_panel", TTRC("Toggle Files Panel"), KeyModifierMask::CMD_OR_CTRL | Key::BACKSLASH);

	set_process_input(true);
	set_process_shortcut_input(true);

	toggle_files_button = memnew(Button);
	toggle_files_button->set_theme_type_variation(SceneStringName(FlatButton));
	toggle_files_button->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	toggle_files_button->set_tooltip_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	toggle_files_button->connect(SceneStringName(toggled), callable_mp(this, &DocumentEditorContainer::_toggle_files_pressed));
	toggle_files_button->set_accessibility_name(TTRC("Toggle Document List"));
	toggle_files_button->set_toggle_mode(true);
	toggle_files_button->set_flat(true);
	toggle_files_button->set_shortcut_context(this);
	toggle_files_button->set_shortcut(ED_GET_SHORTCUT("script_editor/toggle_files_panel"));
	menu_hb->add_child(toggle_files_button);
#ifndef DISABLE_DEPRECATED
	EditorSettings *es = EditorSettings::get_singleton();
	if (es && es->get_project_metadata("files_panel", "show_files_panel", "") != "") {
		bool show_files_panel = es->get_project_metadata("files_panel", "show_files_panel", true);
		es->set_project_metadata("show_files_panel", "ScriptEditor", show_files_panel);
		es->set_project_metadata("show_files_panel", "ShaderEditor", show_files_panel);
		es->set_project_metadata("files_panel", "show_files_panel", Variant());
		es->save_project_metadata();
	}
#endif
	bool initial_state = EditorSettings::get_singleton()->get_project_metadata("show_files_panel", config_section, true);
	toggle_files_button->set_pressed_no_signal(initial_state);
	_toggle_files_pressed(initial_state);

	VSeparator *sep = memnew(VSeparator);
	menu_hb->add_child(sep);

	file_menu = memnew(MenuButton);
	file_menu->set_flat(false);
	file_menu->set_theme_type_variation("FlatMenuButton");
	file_menu->set_text(TTRC("File"));
	file_menu->set_switch_on_hover(true);
	file_menu->set_shortcut_context(this);
	file_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &DocumentEditorContainer::_menu_option));
	file_menu->get_popup()->connect("about_to_popup", callable_mp(this, &DocumentEditorContainer::_prepare_file_menu));
	file_menu->get_popup()->connect("popup_hide", callable_mp(this, &DocumentEditorContainer::_file_menu_closed));
	menu_hb->add_child(file_menu);
	recent_scripts = memnew(PopupMenu);
	recent_scripts->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	recent_scripts->connect(SceneStringName(id_pressed), callable_mp(this, &DocumentEditorContainer::_open_recent_script));
	_update_recent_scripts();

	theme_submenu = memnew(PopupMenu);
	theme_submenu->connect(SceneStringName(id_pressed), callable_mp(this, &DocumentEditorContainer::_theme_option));
	theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/import_theme", TTRC("Import Theme...")), THEME_IMPORT);
	theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/reload_theme", TTRC("Reload Theme")), THEME_RELOAD);
	theme_submenu->add_separator();
	theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/save_theme_as", TTRC("Save Theme As...")), THEME_SAVE_AS);

	if (is_main_editor) {
		file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new_script", TTRC("New Script..."), KeyModifierMask::CMD_OR_CTRL | Key::N), FILE_MENU_NEW_SCRIPT);
		file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new_textfile", TTRC("New Text File..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::N), FILE_MENU_NEW_TEXTFILE);
		file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/open", TTRC("Open...")), FILE_MENU_OPEN);
	} else {
		file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new_shader", TTRC("New Shader..."), KeyModifierMask::CMD_OR_CTRL | Key::N), FILE_MENU_NEW_SHADER);
		file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new_include", TTRC("New Shader Include..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::N), FILE_MENU_NEW_INCLUDE);
		file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/open_shader", TTRC("Load Shader File..."), KeyModifierMask::CMD_OR_CTRL | Key::O), FILE_MENU_OPEN_SHADER);
		file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/open_include", TTRC("Load Shader Include File..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::O), FILE_MENU_OPEN_INCLUDE);
	}
	file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/reopen_closed_script"), FILE_MENU_REOPEN_CLOSED);
	file_menu->get_popup()->add_submenu_node_item(TTRC("Open Recent"), recent_scripts, FILE_MENU_OPEN_RECENT);
	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/save"), FILE_MENU_SAVE);
	file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/save_as"), FILE_MENU_SAVE_AS);
	file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/save_all"), FILE_MENU_SAVE_ALL);
	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/close_file"), FILE_MENU_CLOSE);
	file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/close_all"), FILE_MENU_CLOSE_ALL);
	file_menu->get_popup()->add_separator();
	if (is_main_editor) {
		file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/reload_script_soft"), FILE_MENU_SOFT_RELOAD_TOOL);
		file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/run_file"), FILE_MENU_RUN);
		if (snippets_opener) {
			file_menu->get_popup()->add_separator();
			file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/code_snippets", TTRC("Code Snippets...")), FILE_MENU_SNIPPETS);
			file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save_snippet", TTRC("Save Selection as Snippet...")), FILE_MENU_SAVE_SNIPPET);
			file_menu->get_popup()->add_separator();
			file_menu->get_popup()->add_item(TTRC("Save Layout..."), FILE_MENU_LAYOUT_SAVE);
			file_menu->get_popup()->add_item(TTRC("Delete Layout..."), FILE_MENU_LAYOUT_DELETE);

			// The saved layouts themselves, filled in when the menu opens.
			layout_presets_menu = memnew(PopupMenu);
			layout_presets_menu->set_name("__layout_presets");
			file_menu->get_popup()->add_child(layout_presets_menu);
			layout_presets_menu->connect(SceneStringName(id_pressed), callable_mp(this, &DocumentEditorContainer::_layout_preset_selected));
			file_menu->get_popup()->add_submenu_item(TTRC("Layouts"), "__layout_presets");
			// Ctrl+\ is the files panel toggle, so the split view needs its own
			// shortcut; with the old one only the toggle ever fired.
			file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/split_editor_view", TTRC("Split Editor View"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::BACKSLASH), FILE_MENU_SPLIT_EDITOR);
		}
		file_menu->get_popup()->add_separator();
	}
	file_menu->get_popup()->add_submenu_node_item(TTRC("Theme"), theme_submenu, FILE_MENU_THEME_SUBMENU);

	script_search_menu = memnew(MenuButton);
	script_search_menu->set_flat(false);
	script_search_menu->set_theme_type_variation("FlatMenuButton");
	script_search_menu->set_text(TTRC("Search"));
	script_search_menu->set_switch_on_hover(true);
	script_search_menu->set_shortcut_context(this);
	script_search_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &DocumentEditorContainer::_menu_option));
	menu_hb->add_child(script_search_menu);

	if (is_main_editor) {
		MenuButton *debug_menu_btn = memnew(MenuButton);
		debug_menu_btn->set_flat(false);
		debug_menu_btn->set_theme_type_variation("FlatMenuButton");
		menu_hb->add_child(debug_menu_btn);
		debug_menu_btn->hide(); // Handled by EditorDebuggerNode below.

		EditorDebuggerNode *debugger = EditorDebuggerNode::get_singleton();
		debugger->set_script_debug_button(debug_menu_btn);
		debugger->connect("goto_script_line", callable_mp(this, &DocumentEditorContainer::_goto_script_line));
		debugger->connect("set_execution", callable_mp(this, &DocumentEditorContainer::_set_execution));
		debugger->connect("clear_execution", callable_mp(this, &DocumentEditorContainer::_clear_execution));
		debugger->connect("breakpoint_set_in_tree", callable_mp(this, &DocumentEditorContainer::_set_breakpoint));
		debugger->connect("breakpoints_cleared_in_tree", callable_mp(this, &DocumentEditorContainer::_clear_breakpoints));
	}

	script_name_button_hbox = memnew(HBoxContainer);
	script_name_button_hbox->set_h_size_flags(SIZE_EXPAND_FILL);
	script_name_button_hbox->add_theme_constant_override("separation", 0);
	script_name_button_hbox->connect(SceneStringName(item_rect_changed), callable_mp(this, &DocumentEditorContainer::_calculate_script_name_button_ratio));
	menu_hb->add_child(script_name_button_hbox);

	script_name_button_left_spacer = memnew(Control);
	script_name_button_left_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	script_name_button_hbox->add_child(script_name_button_left_spacer);

	script_name_button = memnew(Button);
	script_name_button->set_flat(true);
	script_name_button->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	script_name_button->set_h_size_flags(SIZE_EXPAND_FILL);
	script_name_button->set_tooltip_text(TTRC("Navigate to document list."));
	script_name_button->connect(SceneStringName(pressed), callable_mp(document_list, &DocumentList::ensure_current_is_visible));
	script_name_button_hbox->add_child(script_name_button);

	script_name_button_right_spacer = memnew(Control);
	script_name_button_right_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	script_name_button_hbox->add_child(script_name_button_right_spacer);

	site_search = memnew(Button);
	site_search->set_theme_type_variation(SceneStringName(FlatButton));
	site_search->set_accessibility_name(TTRC("Site Search"));
	site_search->connect(SceneStringName(pressed), callable_mp(this, &DocumentEditorContainer::_menu_option).bind(SEARCH_WEBSITE));
	menu_hb->add_child(site_search);

	if (is_main_editor) {
		help_search = memnew(Button);
		help_search->set_theme_type_variation(SceneStringName(FlatButton));
		help_search->set_text(TTRC("Search Help"));
		help_search->connect(SceneStringName(pressed), callable_mp(this, &DocumentEditorContainer::_menu_option).bind(SEARCH_HELP));
		menu_hb->add_child(help_search);
		help_search->set_tooltip_text(TTRC("Search the reference documentation."));
	}

	menu_hb->add_child(memnew(VSeparator));

	script_back = memnew(Button);
	script_back->set_theme_type_variation(SceneStringName(FlatButton));
	script_back->set_tooltip_text(TTRC("Go to previous edited document."));
	script_back->set_shortcut(ED_GET_SHORTCUT("script_editor/history_previous"));
	script_back->set_shortcut_context(this);
	script_back->set_disabled(true);
	menu_hb->add_child(script_back);
	script_back->connect(SceneStringName(pressed), callable_mp(this, &DocumentEditorContainer::_history_back));

	script_forward = memnew(Button);
	script_forward->set_theme_type_variation(SceneStringName(FlatButton));
	script_forward->set_tooltip_text(TTRC("Go to next edited document."));
	script_forward->set_shortcut(ED_GET_SHORTCUT("script_editor/history_next"));
	script_forward->set_shortcut_context(this);
	script_forward->set_disabled(true);
	menu_hb->add_child(script_forward);
	script_forward->connect(SceneStringName(pressed), callable_mp(this, &DocumentEditorContainer::_history_forward));

	tab_container->connect("tab_changed", callable_mp(this, &DocumentEditorContainer::ensure_select_current).unbind(1));

	erase_tab_confirm = memnew(ConfirmationDialog);
	erase_tab_confirm->set_flag(Window::FLAG_RESIZE_DISABLED, true);
	erase_tab_confirm->set_ok_button_text(TTRC("Save"));
	erase_tab_confirm->add_button(TTRC("Discard"), DisplayServer::get_singleton()->get_swap_cancel_ok(), "discard");
	erase_tab_confirm->connect(SceneStringName(confirmed), callable_mp(this, &DocumentEditorContainer::_close_current_tab).bind(true));
	erase_tab_confirm->connect("custom_action", callable_mp(this, &DocumentEditorContainer::_close_discard_current_tab));
	add_child(erase_tab_confirm);

	script_create_dialog = memnew(ScriptCreateDialog);
	script_create_dialog->set_title(TTRC("Create Script"));
	add_child(script_create_dialog);
	script_create_dialog->connect("script_created", callable_mp(this, &DocumentEditorContainer::_resource_created));

	file_dialog_option = -1;
	file_dialog = memnew(EditorFileDialog);
	add_child(file_dialog);
	file_dialog->connect("file_selected", callable_mp(this, &DocumentEditorContainer::_file_dialog_action));

	shader_create_dialog = memnew(ShaderCreateDialog);
	shader_create_dialog->connect("shader_created", callable_mp(this, &DocumentEditorContainer::_resource_created));
	shader_create_dialog->connect("shader_include_created", callable_mp(this, &DocumentEditorContainer::_resource_created));
	add_child(shader_create_dialog);

	error_dialog = memnew(AcceptDialog);
	add_child(error_dialog);

	disk_changed = memnew(ConfirmationDialog);
	{
		disk_changed->set_title(TTRC("Files have been modified outside Godot"));

		VBoxContainer *vbc = memnew(VBoxContainer);
		disk_changed->add_child(vbc);

		Label *files_are_newer_label = memnew(Label);
		files_are_newer_label->set_text(TTRC("The following files are newer on disk:"));
		vbc->add_child(files_are_newer_label);

		disk_changed_list = memnew(Tree);
		disk_changed_list->set_hide_root(true);
		disk_changed_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		disk_changed_list->set_accessibility_name(TTRC("The following files are newer on disk:"));
		disk_changed_list->set_v_size_flags(SIZE_EXPAND_FILL);
		vbc->add_child(disk_changed_list);

		Label *what_action_label = memnew(Label);
		what_action_label->set_text(TTRC("What action should be taken?"));
		vbc->add_child(what_action_label);

		disk_changed->connect(SceneStringName(confirmed), callable_mp(this, &DocumentEditorContainer::reload_scripts).bind(false));
		disk_changed->set_ok_button_text(TTRC("Reload from disk"));

		disk_changed->add_button(TTRC("Ignore external changes"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "resave");
		disk_changed->connect("custom_action", callable_mp(this, &DocumentEditorContainer::_resave_scripts));
	}

	add_child(disk_changed);

	autosave_timer = memnew(Timer);
	autosave_timer->set_one_shot(false);
	autosave_timer->connect(SceneStringName(tree_entered), callable_mp(this, &DocumentEditorContainer::_update_autosave_timer));
	autosave_timer->connect("timeout", callable_mp(this, &DocumentEditorContainer::_autosave_scripts));
	add_child(autosave_timer);

	history_pos = -1;

	trim_trailing_whitespace_on_save = EDITOR_GET("text_editor/behavior/files/trim_trailing_whitespace_on_save");
	trim_final_newlines_on_save = EDITOR_GET("text_editor/behavior/files/trim_final_newlines_on_save");
	convert_indent_on_save = EDITOR_GET("text_editor/behavior/files/convert_indent_on_save");
	format_on_save = EDITOR_GET("text_editor/behavior/formatter/format_on_save");

	//// Highlighters ////

	Ref<EditorJSONSyntaxHighlighter> json_syntax_highlighter;
	json_syntax_highlighter.instantiate();
	register_syntax_highlighter(json_syntax_highlighter);

	Ref<EditorMarkdownSyntaxHighlighter> markdown_syntax_highlighter;
	markdown_syntax_highlighter.instantiate();
	register_syntax_highlighter(markdown_syntax_highlighter);

	Ref<EditorConfigFileSyntaxHighlighter> config_file_syntax_highlighter;
	config_file_syntax_highlighter.instantiate();
	register_syntax_highlighter(config_file_syntax_highlighter);

	Ref<GDShaderSyntaxHighlighter> gdshader_syntax_highlighter;
	gdshader_syntax_highlighter.instantiate();
	register_syntax_highlighter(gdshader_syntax_highlighter);
}

void ScriptEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_apply_editor_settings();
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor")) {
				_apply_editor_settings();
			}
		} break;
	}
}

bool ScriptEditor::should_use_external_editor(const Ref<Script> &p_for_script) {
	bool use_external_editor = external_editor_active ||
			(p_for_script.is_valid() && p_for_script->get_language()->overrides_external_editor());
	return use_external_editor && !(p_for_script.is_valid() && p_for_script->is_built_in()); // Ignore external editor for built-in scripts.
}

void ScriptEditor::focus_editor() {
	EditorDockManager::get_singleton()->force_focus_dock(this);
}

void ScriptEditor::focus_script_editor(const Ref<Resource> &p_for_resource) {
	if (!should_use_external_editor(p_for_resource)) {
		focus_editor();
	}
}

void ScriptEditor::update_layout(EditorDock::DockLayout p_layout, int p_slot) {
	begin_bulk_theme_override();
	if (p_layout == DOCK_LAYOUT_FLOATING) {
		remove_theme_constant_override("margin_left");
		remove_theme_constant_override("margin_right");
		remove_theme_constant_override("margin_bottom");
	} else {
		int margin = EDSCALE_RND(EditorNode::get_singleton()->get_editor_theme()->get_constant("base_margin", EditorStringName(Editor)));
		add_theme_constant_override("margin_left", margin);
		add_theme_constant_override("margin_right", margin);
		add_theme_constant_override("margin_bottom", margin);
	}
	end_bulk_theme_override();
}

void ScriptEditor::_update_margins() {
	update_layout(get_current_layout(), get_current_slot());
}

void ScriptEditor::_apply_editor_settings() {
	external_editor_active = EDITOR_GET("text_editor/external/use_external_editor");
	set_allow_switch_screen(!bool(EDITOR_GET("text_editor/behavior/navigation/stay_in_script_editor_on_node_selected")));
}

void ScriptEditor::_on_find_in_files_result_selected(const String &fpath, int line_number, int begin, int end) {
	ScriptEditorNavigationMarker::get_singleton()->locate_begin();
	if (ResourceLoader::exists(fpath)) {
		Ref<Resource> res = ResourceLoader::load(fpath);

		if (fpath.has_extension("gdshader") || fpath.has_extension("gdshaderinc")) {
			ShaderEditorPlugin *shader_editor = Object::cast_to<ShaderEditorPlugin>(EditorNode::get_editor_data().get_editor_by_name("Shader"));
			shader_editor->edit(res.ptr());
			shader_editor->set_current();
			if (ShaderTextEditor *shader_te = Object::cast_to<ShaderTextEditor>(shader_editor->get_shader_container()->get_resource_editor(res))) {
				shader_te->goto_line_selection(line_number - 1, begin, end);
			}
			ScriptEditorNavigationMarker::get_singleton()->locate_end();
			return;
		} else if (fpath.has_extension("tscn")) {
			const PackedStringArray lines = FileAccess::get_file_as_string(fpath).split("\n");
			if (line_number > lines.size()) {
				ScriptEditorNavigationMarker::get_singleton()->locate_end();
				return;
			}

			const char *scr_header = "[sub_resource type=\"GDScript\" id=\"";
			const char *source_header = "script/source = \"";
			String script_id;

			// Search the scene backwards from the found line.
			int scan_line = line_number - 1;
			while (scan_line >= 0) {
				const String &line = lines[scan_line];
				if (line.begins_with(source_header)) {
					// Adjust line relative to the script beginning.
					line_number -= scan_line + 1;
				} else if (line.begins_with(scr_header)) {
					script_id = line.trim_prefix(scr_header).get_slicec('"', 0);
					break;
				}
				scan_line--;
			}

			EditorNode::get_singleton()->open_scene(fpath);
			if (!script_id.is_empty()) {
				Ref<Script> scr = ResourceLoader::load(fpath + "::" + script_id, "Script");
				if (scr.is_valid()) {
					edit(scr);
					ScriptTextEditor *ste = Object::cast_to<ScriptTextEditor>(get_current_editor());

					if (ste) {
						callable_mp(this, &ScriptEditor::focus_editor).call_deferred();
						if (line_number == 0) {
							const int source_len = strlen(source_header);
							ste->goto_line_selection(line_number, begin - source_len, end - source_len);
						} else {
							ste->goto_line_selection(line_number, begin, end);
						}
					}
				}
			}

			ScriptEditorNavigationMarker::get_singleton()->locate_end();
			return;
		} else {
			Ref<Script> scr = res;
			Ref<JSON> json = res;
			if (scr.is_valid() || json.is_valid()) {
				edit(scr);

				ScriptTextEditor *ste = Object::cast_to<ScriptTextEditor>(get_current_editor());
				if (ste) {
					focus_script_editor(res);
					ste->goto_line_selection(line_number - 1, begin, end);
				}
				ScriptEditorNavigationMarker::get_singleton()->locate_end();
				return;
			}
		}
	}

	// If the file is not a valid resource/script, load it as a text file.
	Ref<Resource> text_file = script_container->open_file(fpath);
	if (text_file.is_valid()) {
		TextEditor *te = Object::cast_to<TextEditor>(get_current_editor());
		if (te) {
			te->goto_line_selection(line_number - 1, begin, end);
		}
	}
	ScriptEditorNavigationMarker::get_singleton()->locate_end();
}

void ScriptEditor::_goto_line(int p_line) {
	TextEditorBase *teb = Object::cast_to<TextEditorBase>(get_current_editor());
	if (teb) {
		ScriptEditorNavigationMarker::get_singleton()->locate_begin();
		teb->goto_line(p_line);
		ScriptEditorNavigationMarker::get_singleton()->locate_end();
	}
}

TypedArray<ScriptEditorBase> ScriptEditor::_get_open_script_editors() const {
	TypedArray<ScriptEditorBase> script_editors;
	for (Control *editor : script_container->get_all_editors()) {
		ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(editor);
		if (seb) {
			script_editors.push_back(seb);
		}
	}
	return script_editors;
}

Ref<Script> ScriptEditor::_get_current_script() {
	ScriptEditorBase *seb = get_current_editor();
	if (seb) {
		Ref<Script> scr = seb->get_edited_resource();
		return scr.is_valid() ? scr : nullptr;
	} else {
		return nullptr;
	}
}

TypedArray<Script> ScriptEditor::_get_open_scripts() const {
	TypedArray<Script> ret;
	Vector<Ref<Script>> scripts = get_open_scripts();
	for (const Ref<Script> &script : scripts) {
		ret.push_back(script);
	}
	return ret;
}

void ScriptEditor::set_scene_root_script(const Ref<Script> &p_script) {
	if (p_script.is_null() || should_use_external_editor(p_script)) {
		return;
	}

	bool open_dominant = EDITOR_GET("text_editor/behavior/files/open_dominant_script_on_scene_change");
	if (open_dominant) {
		edit(p_script);
	}
}

void ScriptEditor::notify_script_close(const Ref<Script> &p_script) {
	emit_signal(SNAME("script_close"), p_script);
}

void ScriptEditor::notify_script_changed(const Ref<Script> &p_script) {
	emit_signal(SNAME("editor_script_changed"), p_script);
}

void ScriptEditor::open_help_search_dialog(const String &p_search_text) {
	help_search_dialog->popup_dialog(p_search_text);
}

void ScriptEditor::_bind_methods() {
	ClassDB::bind_method("get_current_editor", &ScriptEditor::get_current_editor);
	ClassDB::bind_method("get_open_script_editors", &ScriptEditor::_get_open_script_editors);
	ClassDB::bind_method("get_breakpoints", &ScriptEditor::_get_breakpoints);

	ClassDB::bind_method(D_METHOD("register_syntax_highlighter", "syntax_highlighter"), &ScriptEditor::register_syntax_highlighter);
	ClassDB::bind_method(D_METHOD("unregister_syntax_highlighter", "syntax_highlighter"), &ScriptEditor::unregister_syntax_highlighter);

	ClassDB::bind_method(D_METHOD("goto_line", "line_number"), &ScriptEditor::_goto_line);
	ClassDB::bind_method(D_METHOD("get_current_script"), &ScriptEditor::_get_current_script);
	ClassDB::bind_method(D_METHOD("get_open_scripts"), &ScriptEditor::_get_open_scripts);
	ClassDB::bind_method(D_METHOD("open_script_create_dialog", "base_name", "base_path"), &ScriptEditor::open_script_create_dialog);
	ClassDB::bind_method(D_METHOD("reload_open_files"), &ScriptEditor::reload_open_files);

	ClassDB::bind_method(D_METHOD("goto_help", "topic"), &ScriptEditor::goto_help);
	ClassDB::bind_method(D_METHOD("update_docs_from_script", "script"), &ScriptEditor::update_docs_from_script);
	ClassDB::bind_method(D_METHOD("clear_docs_from_script", "script"), &ScriptEditor::clear_docs_from_script);

	ClassDB::bind_method(D_METHOD("get_unsaved_files"), &ScriptEditor::get_unsaved_files);

	ClassDB::bind_method(D_METHOD("save_all_scripts"), &ScriptEditor::save_all_scripts);
	ClassDB::bind_method(D_METHOD("close_file", "path"), &ScriptEditor::close_file);

	ADD_SIGNAL(MethodInfo("editor_script_changed", PropertyInfo(Variant::OBJECT, "script", PROPERTY_HINT_RESOURCE_TYPE, Script::get_class_static())));
	ADD_SIGNAL(MethodInfo("script_close", PropertyInfo(Variant::OBJECT, "script", PROPERTY_HINT_RESOURCE_TYPE, Script::get_class_static())));
}

ScriptEditor::ScriptEditor() {
	script_editor = this;

	script_container = memnew(DocumentEditorContainer(true, "ScriptEditor", "script_editor_cache.cfg"));
	script_container->set_handled_resource_types({ "GDScript", "Script", "JSON" });
	add_child(script_container);
	connect("_focused", callable_mp(script_container, &DocumentEditorContainer::focus_active_editor));

	help_search_dialog = memnew(EditorHelpSearch);
	add_child(help_search_dialog);
	help_search_dialog->connect("go_to_help", callable_mp(script_container, &DocumentEditorContainer::goto_help));

	FindInFilesContainer *find_in_files = FindInFiles::get_singleton()->get_container();
	find_in_files->connect("result_selected", callable_mp(this, &ScriptEditor::_on_find_in_files_result_selected));
	find_in_files->connect("files_modified", callable_mp(script_container, &DocumentEditorContainer::reload_open_files));

	EditorNode::get_singleton()->get_gui_base()->connect(SceneStringName(theme_changed), callable_mp(this, &ScriptEditor::_update_margins));
}

ScriptEditor::~ScriptEditor() {
	ScriptEditorNavigationMarker::release_singleton();
}

void ScriptEditorPlugin::shortcut_input(const Ref<InputEvent> &p_event) {
	if (p_event.is_null() || !p_event->is_pressed() || p_event->is_echo()) {
		return;
	}

	if (make_floating_shortcut.is_valid() && make_floating_shortcut->matches_event(p_event)) {
		script_editor->make_floating();
	}
}

bool ScriptEditorPlugin::open_in_external_editor(const String &p_path, int p_line, int p_col, bool p_ignore_project) {
	const String path = EDITOR_GET("text_editor/external/exec_path");
	if (path.is_empty()) {
		return false;
	}

	String flags = EDITOR_GET("text_editor/external/exec_flags");

	List<String> args;
	bool has_file_flag = false;

	if (!flags.is_empty()) {
		flags = flags.replacen("{line}", itos(MAX(p_line + 1, 1)));
		flags = flags.replacen("{col}", itos(p_col + 1));
		flags = flags.strip_edges().replace("\\\\", "\\");

		int from = 0;
		int num_chars = 0;
		bool inside_quotes = false;

		for (int i = 0; i < flags.size(); i++) {
			if (flags[i] == '"' && (!i || flags[i - 1] != '\\')) {
				if (!inside_quotes) {
					from++;
				}
				inside_quotes = !inside_quotes;

			} else if (flags[i] == '\0' || (!inside_quotes && flags[i] == ' ')) {
				String arg = flags.substr(from, num_chars);
				if (arg.contains("{file}")) {
					has_file_flag = true;
				}

				// Do path replacement here, else there will be issues with spaces and quotes
				if (p_ignore_project) {
					arg = arg.replacen("{project}", String());
				} else {
					arg = arg.replacen("{project}", ProjectSettings::get_singleton()->get_resource_path());
				}
				arg = arg.replacen("{file}", p_path);
				args.push_back(arg);

				from = i + 1;
				num_chars = 0;
			} else {
				num_chars++;
			}
		}
	}

	// Default to passing script path if no {file} flag is specified.
	if (!has_file_flag) {
		args.push_back(p_path);
	}
	return OS::get_singleton()->create_process(path, args) == OK;
}

void ScriptEditorPlugin::edit(Object *p_object) {
	if (!p_object) {
		return;
	}

	if (Object::cast_to<Script>(p_object)) {
		Script *p_script = Object::cast_to<Script>(p_object);
		String res_path = p_script->get_path().get_slice("::", 0);

		// Only update main editor screen if using in-engine editor.
		if (!p_script->is_built_in() && (bool(EDITOR_GET("text_editor/external/use_external_editor")) || p_script->get_language()->overrides_external_editor())) {
			skip_visible = true;
		} else if (p_script->is_built_in() && !res_path.is_empty()) {
			EditorNode::get_singleton()->load_scene_or_resource(res_path, false, false);
		}
		script_editor->edit(p_script);
	} else if (Object::cast_to<JSON>(p_object) || Object::cast_to<TextFile>(p_object)) {
		script_editor->edit(Object::cast_to<Resource>(p_object));
	}
}

bool ScriptEditorPlugin::handles(Object *p_object) const {
	if (Object::cast_to<Script>(p_object)) {
		return true;
	} else if (Object::cast_to<JSON>(p_object)) {
		// This is here to stop resource files of class JSON from getting confused
		// with json files and being opened in the text editor.
		if (Object::cast_to<JSON>(p_object)->get_path().get_extension().to_lower() == "json") {
			return true;
		}
	} else if (Object::cast_to<TextFile>(p_object)) {
		return true;
	}

	return p_object->is_class("Script");
}

void ScriptEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		if (skip_visible) {
			skip_visible = false;
			return;
		}
		script_editor->focus_editor();
		script_editor->get_script_container()->ensure_select_current();
	}
}

String ScriptEditorPlugin::get_unsaved_status(const String &p_for_scene) const {
	const PackedStringArray unsaved_scripts = script_editor->get_script_container()->get_unsaved_scripts();
	if (unsaved_scripts.is_empty()) {
		return String();
	}

	PackedStringArray message;
	if (!p_for_scene.is_empty()) {
		PackedStringArray unsaved_built_in_scripts;

		const String scene_file = p_for_scene.get_file();
		for (const String &E : unsaved_scripts) {
			if (!E.is_resource_file() && E.contains(scene_file)) {
				unsaved_built_in_scripts.append(E);
			}
		}

		if (unsaved_built_in_scripts.is_empty()) {
			return String();
		} else {
			message.push_back(TTR("There are unsaved changes in the following built-in resource(s)"));
			message.append_array(unsaved_built_in_scripts);
			return String("\n").join(message);
		}
	}

	message.push_back(TTR("Save changes to the following file(s) before quitting?"));
	message.append_array(unsaved_scripts);
	return String("\n").join(message);
}

void ScriptEditorPlugin::save_external_data() {
	if (!EditorNode::get_singleton()->is_exiting()) {
		script_editor->save_all_scripts();
	}
}

void ScriptEditorPlugin::set_window_layout(Ref<ConfigFile> p_layout) {
	if (bool(EDITOR_GET("text_editor/behavior/files/restore_scripts_on_load"))) {
		script_editor->get_script_container()->set_window_layout(p_layout);
	}
}

void ScriptEditorPlugin::get_window_layout(Ref<ConfigFile> p_layout) {
#ifndef DISABLE_DEPRECATED
	EditorSettings *es = EditorSettings::get_singleton();
	if (es && es->get_project_metadata("recent_files", "scripts", "") != "") {
		es->set_project_metadata("recent_files", "ScriptEditor", es->get_project_metadata("recent_files", "scripts", Array()));
		es->set_project_metadata("recent_files", "scripts", Variant());
		es->save_project_metadata();
	}

	if (p_layout->has_section_key("ScriptEditor", "window_rect")) {
		p_layout->erase_section_key("ScriptEditor", "window_rect");
	}
	if (p_layout->has_section_key("ScriptEditor", "window_screen")) {
		p_layout->erase_section_key("ScriptEditor", "window_screen");
	}
	if (p_layout->has_section_key("ScriptEditor", "window_screen_rect")) {
		p_layout->erase_section_key("ScriptEditor", "window_screen_rect");
	}
#endif

	script_editor->get_script_container()->get_window_layout(p_layout);
}

ScriptEditorPlugin::ScriptEditorPlugin() {
	script_editor_plugin = this;

	ED_SHORTCUT("script_editor/reopen_closed_script", TTRC("Reopen Closed Script"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::T);
	ED_SHORTCUT("script_editor/clear_recent", TTRC("Clear Recent Scripts"));

	ED_SHORTCUT("script_text_editor/convert_to_uppercase", TTRC("Uppercase"), KeyModifierMask::SHIFT | Key::F4);
	ED_SHORTCUT("script_text_editor/convert_to_lowercase", TTRC("Lowercase"), KeyModifierMask::SHIFT | Key::F5);
	ED_SHORTCUT("script_text_editor/capitalize", TTRC("Capitalize"), KeyModifierMask::SHIFT | Key::F6);

	make_floating_shortcut = ED_SHORTCUT_AND_COMMAND("script_editor/make_floating", TTRC("Make Floating"));
	set_process_shortcut_input(true);

	memnew(FindInFiles);

	script_editor = memnew(ScriptEditor);
	script_editor->set_name(TTRC("Script"));
	script_editor->set_icon_name("Script");
	script_editor->set_available_layouts(EditorDock::DOCK_LAYOUT_MAIN_SCREEN | EditorDock::DOCK_LAYOUT_FLOATING);
	script_editor->set_default_slot(EditorDock::DOCK_SLOT_MAIN_SCREEN);
	script_editor->set_dock_shortcut(ED_GET_SHORTCUT("editor/editor_script"));

	EditorDockManager::get_singleton()->add_dock(script_editor);

	ScriptServer::set_reload_scripts_on_save(EDITOR_GET("text_editor/behavior/files/auto_reload_and_parse_scripts_on_save"));
}

ScriptEditorPlugin::~ScriptEditorPlugin() {
	memdelete(FindInFiles::get_singleton());
}
