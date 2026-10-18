/**************************************************************************/
/*  test_editor_global_plugins.cpp                                        */
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

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/plugins/editor_plugin.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_editor_global_plugins)

namespace TestEditorGlobalPlugins {

static PackedStringArray make_names(std::initializer_list<const char *> p_names) {
	PackedStringArray result;
	for (const char *name : p_names) {
		result.push_back(String(name));
	}
	return result;
}

static String make_temp_dir(const String &p_name) {
	const String path = OS::get_singleton()->get_temp_path().path_join(vformat("godot_global_plugins_%s_%d", p_name, OS::get_singleton()->get_ticks_usec())).simplify_path();
	DirAccess::make_dir_recursive_absolute(path);
	return path;
}

static void write_plugin_cfg(const String &p_addon_dir) {
	DirAccess::make_dir_recursive_absolute(p_addon_dir);
	Ref<FileAccess> file = FileAccess::open(p_addon_dir.path_join("plugin.cfg"), FileAccess::WRITE);
	ERR_FAIL_COND(file.is_null());
	file->store_string("[plugin]\nname=\"Test\"\ndescription=\"\"\nauthor=\"\"\nversion=\"1\"\nscript=\"plugin.gd\"\n");
	file->close();
}

static void remove_dir_recursive(const String &p_dir) {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return;
	}
	dir->list_dir_begin();
	for (String name = dir->get_next(); !name.is_empty(); name = dir->get_next()) {
		if (name == "." || name == "..") {
			continue;
		}
		const String child = p_dir.path_join(name);
		if (dir->current_is_dir()) {
			remove_dir_recursive(child);
		} else {
			DirAccess::remove_absolute(child);
		}
	}
	dir->list_dir_end();
	DirAccess::remove_absolute(p_dir);
}

TEST_CASE("[Editor] Global plugin conflict matrix C1-C8") {
	const String root = make_temp_dir("matrix");
	const String global_dir = root.path_join("global_addons");
	const String project_a = root.path_join("project_a");
	const String project_b = root.path_join("project_b");
	DirAccess::make_dir_recursive_absolute(project_b.path_join("addons").path_join("a"));

	write_plugin_cfg(global_dir.path_join("a"));
	write_plugin_cfg(global_dir.path_join("b"));

	PackedStringArray stale;
	// C1/C5/C6: dedupe, sort, keep stale entries separate.
	PackedStringArray resolved = EditorNode::resolve_global_addons(make_names({ "b", "a", "b", "missing" }), global_dir, project_a.path_join("addons"), &stale);
	CHECK(resolved.size() == 2);
	CHECK(resolved[0].ends_with("a/plugin.cfg"));
	CHECK(resolved[1].ends_with("b/plugin.cfg"));
	CHECK(stale.size() == 1);
	if (stale.size() == 1) {
		CHECK(stale[0] == "missing");
	}

	// C3/C4: a project directory with the same name shadows the global version even if not enabled.
	resolved = EditorNode::resolve_global_addons(make_names({ "a", "b" }), global_dir, project_b.path_join("addons"));
	CHECK(resolved.size() == 1);
	CHECK(resolved[0].ends_with("b/plugin.cfg"));

	// C2/C6: no global entries means no global work at all.
	CHECK(EditorNode::resolve_global_addons(make_names({}), global_dir, project_a.path_join("addons")).is_empty());

	// Double-project smoke: project A sees the global marker, project B shadows it.
	CHECK(EditorNode::resolve_global_addons(make_names({ "a" }), global_dir, project_a.path_join("addons")).size() == 1);
	CHECK(EditorNode::resolve_global_addons(make_names({ "a" }), global_dir, project_b.path_join("addons")).is_empty());

	// C8: a missing global directory yields stale entries instead of loading.
	PackedStringArray stale_missing;
	CHECK(EditorNode::resolve_global_addons(make_names({ "a" }), root.path_join("does_not_exist"), project_a.path_join("addons"), &stale_missing).is_empty());
	CHECK(stale_missing.size() == 1);

	CHECK(EditorNode::is_global_addon_path(global_dir.path_join("a").path_join("plugin.cfg")));
	CHECK(!EditorNode::is_global_addon_path("res://addons/a/plugin.cfg"));
}

TEST_CASE("[Editor] EditorPlugin global flag") {
	EditorPlugin *plugin = memnew(EditorPlugin);
	CHECK(!plugin->is_global_plugin());
	plugin->set_global_plugin(true);
	CHECK(plugin->is_global_plugin());
	memdelete(plugin);
}

} // namespace TestEditorGlobalPlugins
