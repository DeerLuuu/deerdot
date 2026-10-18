/**************************************************************************/
/*  gdscript_snippet_editor.h                                             */
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

#include "scene/gui/dialogs.h"

class Button;
class CodeEdit;
class ItemList;
class Label;
class PopupMenu;
class ScriptEditorBase;
class SnippetJSONHighlighter;
class TabContainer;

// A window to quickly preview and edit the user-defined code snippet files
// (VSCode-style JSON) of both snippet directories: the editor-wide global one
// and the project one. Saving a file writes it back to disk and hot-reloads
// the snippets immediately.

// AcceptDialog has no cancel signal, so detecting a cancel on a child dialog
// requires deriving and overriding cancel_pressed().
class SnippetSwitchConfirm : public ConfirmationDialog {
	GDCLASS(SnippetSwitchConfirm, ConfirmationDialog);

public:
	Callable cancel_callable;

protected:
	virtual void cancel_pressed() override {
		if (cancel_callable.is_valid()) {
			cancel_callable.call();
		}
	}
};

class GDScriptSnippetEditor : public AcceptDialog {
	GDCLASS(GDScriptSnippetEditor, AcceptDialog);

	static GDScriptSnippetEditor *singleton;

	// Snippets offered by the context menu of the editor it was opened on.
	static ScriptEditorBase *context_menu_editor;
	static HashMap<int, String> context_menu_prefixes;
	static void _expand_context_menu_snippet(int p_id);

	struct SnippetDirTab {
		String dir_path;
		Ref<SnippetJSONHighlighter> highlighter;
		ItemList *file_list = nullptr;
		CodeEdit *code_edit = nullptr;
		Label *status = nullptr;
		Button *save_button = nullptr;
		String current_path;
		bool dirty = false;
	};

	enum {
		TAB_GLOBAL,
		TAB_PROJECT,
		TAB_MAX,
	};

	TabContainer *tab_container = nullptr;
	SnippetDirTab tabs[TAB_MAX];

	SnippetSwitchConfirm *switch_confirm = nullptr;
	AcceptDialog *error_dialog = nullptr;
	int pending_tab = -1;
	String pending_path;

	SnippetDirTab &get_tab(int p_index) { return tabs[p_index]; }

	// The window is attached to the window the script editor currently lives in (a floating script editor
	// lives in its own Window), and shown deferred so an exclusive child window cannot block it silently.
	static Window *_get_host_window();
	void _open_deferred();
	void _retry_open();
	void _reset_singleton();

	void _build_tab(int p_index, const String &p_title, const String &p_dir);
	void _refresh_tab(int p_index);
	void _load_file(int p_index, const String &p_path);
	bool _save_current(int p_index);
	void _revert_selection(int p_index);

	void _on_file_selected(int p_item, int p_tab);
	void _on_text_changed(int p_tab);
	void _on_save_pressed(int p_tab);
	void _on_refresh_pressed(int p_tab);
	void _on_open_dir_pressed(int p_tab);
	void _on_code_completion_requested(int p_tab);
	void _on_switch_confirm_confirmed();
	void _on_switch_confirm_discard(const String &p_action);
	void _on_switch_confirm_cancelled();

public:
	static GDScriptSnippetEditor *get_singleton() { return singleton; }
	// Registered as the script editor's "Code Snippets..." opener. Lazily creates the window (as a child
	// of the window the script editor currently lives in) and shows it on the next idle frame.
	static void open_snippets_editor();
	static void reload_snippets();

	// Adds the loaded snippets to a code editor's context menu, so one can be
	// expanded at the caret without typing its prefix.
	static void fill_context_menu(PopupMenu *p_menu, ScriptEditorBase *p_editor);

	GDScriptSnippetEditor();
	~GDScriptSnippetEditor() override;
};
