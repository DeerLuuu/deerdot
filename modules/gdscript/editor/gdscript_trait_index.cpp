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

#include "core/io/file_access.h"
#include "editor/file_system/editor_file_system.h"

static void _scan_traits_in_dir(EditorFileSystemDirectory *p_dir) {
	if (p_dir == nullptr) {
		return;
	}

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		const String path = p_dir->get_file_path(i);
		if (!path.ends_with(".gd")) {
			continue;
		}

		Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
		if (file.is_null()) {
			continue;
		}

		for (const String &line : file->get_as_text().split("\n")) {
			// Only top level declarations are registered globally, and those
			// start at the first column.
			if (!line.begins_with("trait ")) {
				continue;
			}
			const String name = line.substr(6).strip_edges().split(":")[0].split(" ")[0].strip_edges();
			if (name.is_valid_identifier()) {
				GDScriptLanguage::register_global_trait(name, path);
			}
		}
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_scan_traits_in_dir(p_dir->get_subdir(i));
	}
}

void gdscript_scan_project_traits() {
	if (EditorFileSystem::get_singleton() == nullptr) {
		return;
	}

	EditorFileSystemDirectory *root = EditorFileSystem::get_singleton()->get_filesystem();
	_scan_traits_in_dir(root);

	print_verbose("GDScript: registered traits declared by the project.");
}
