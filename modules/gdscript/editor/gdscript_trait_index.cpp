/**************************************************************************/
/*  gdscript_trait_index.cpp                                              */
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

#include "gdscript_trait_index.h"

#include "../gdscript.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

static uint64_t trait_index_scan_usec = 0;

// Reads one script and registers the traits it declares at its top level.
static void _register_traits_in_file(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return;
	}

	for (const String &line : file->get_as_text().split("\n")) {
		// Only top level declarations are registered globally, and those start
		// at the first column, after a possible byte order mark.
		const String code = line.remove_char(0xFEFF);
		if (!code.begins_with("trait ")) {
			continue;
		}
		const String name = code.substr(6).strip_edges().split(":")[0].split(" ")[0].strip_edges();
		if (name.is_valid_identifier()) {
			GDScriptLanguage::register_global_trait(name, p_path);
		}
	}
}

// Walks the project itself rather than the editor filesystem: scripts can be
// analyzed (restored ones, for instance) before that filesystem is ready.
static void _scan_traits_in_dir(const String &p_path) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		return;
	}

	dir->list_dir_begin();
	for (String name = dir->get_next(); !name.is_empty(); name = dir->get_next()) {
		if (name.begins_with(".")) {
			// Project data and version control directories.
			continue;
		}

		const String full_path = p_path.path_join(name);
		if (dir->current_is_dir()) {
			_scan_traits_in_dir(full_path);
		} else if (name.ends_with(".gd")) {
			_register_traits_in_file(full_path);
		}
	}
	dir->list_dir_end();
}

void gdscript_scan_project_traits() {
	if (GDScriptLanguage::get_singleton() == nullptr) {
		return;
	}

	_scan_traits_in_dir("res://");

	trait_index_scan_usec = OS::get_singleton()->get_ticks_usec();

	List<StringName> names;
	GDScriptLanguage::get_singleton()->get_global_trait_names(&names);
	String list;
	for (const StringName &name : names) {
		list = list.is_empty() ? String(name) : list + ", " + String(name);
	}
	print_verbose(vformat("GDScript: the project declares %d trait(s): %s", names.size(), list));
}

bool gdscript_project_has_trait(const StringName &p_name) {
	if (GDScriptLanguage::has_global_trait(p_name)) {
		return true;
	}

	// Not known: the project may not have been read yet, which happens when a
	// script is analyzed before the filesystem scan finishes. Read it, but not
	// more than once a second.
	const uint64_t now = OS::get_singleton()->get_ticks_usec();
	if (trait_index_scan_usec == 0 || now - trait_index_scan_usec > 1000000) {
		gdscript_scan_project_traits();
	}

	return GDScriptLanguage::has_global_trait(p_name);
}
