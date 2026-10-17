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
#include "../gdscript_tokenizer.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

static uint64_t trait_index_scan_usec = 0;

// Reads one script and registers the traits it declares at its top level.
static void _register_traits_in_file(const String &p_path, HashSet<StringName> *r_found) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return;
	}

	String source = file->get_as_text();
	if (!source.contains("trait")) {
		// Most scripts declare no trait, and tokenizing is not free.
		return;
	}
	source = source.remove_char(0xFEFF);

	// Read the declarations with the tokenizer rather than matching lines: a
	// `trait Name` written inside a multi-line string is not a declaration, and
	// a line-based scan would register it as a project trait.
	GDScriptTokenizerText tokenizer;
	tokenizer.set_source_code(source);

	GDScriptTokenizer::Token token = tokenizer.scan();
	while (token.type != GDScriptTokenizer::Token::TK_EOF) {
		// Only declarations at the first column describe the whole file, which is
		// what makes a trait global. A trait nested in an inner class is indented.
		if (token.type == GDScriptTokenizer::Token::TRAIT && token.start_column == 1) {
			const GDScriptTokenizer::Token name_token = tokenizer.scan();
			if (name_token.type == GDScriptTokenizer::Token::IDENTIFIER) {
				GDScriptLanguage::register_global_trait(name_token.get_identifier(), p_path);
				if (r_found != nullptr) {
					r_found->insert(name_token.get_identifier());
				}
			}
			token = name_token;
			continue;
		}
		token = tokenizer.scan();
	}
}

// Walks the project itself rather than the editor filesystem: scripts can be
// analyzed (restored ones, for instance) before that filesystem is ready.
static void _scan_traits_in_dir(const String &p_path, HashSet<String> &r_visited, int p_depth, HashSet<StringName> &r_found) {
	// A directory can link back to one of its ancestors (a symlink or, on
	// Windows, a junction), which would make this walk unbounded. Bound it on
	// depth as well as on the directories already seen.
	constexpr int MAX_DEPTH = 64;
	if (p_depth > MAX_DEPTH) {
		WARN_PRINT(vformat(R"(GDScript: Not scanning "%s" for traits: the directory tree is too deep, probably because of a link to an ancestor.)", p_path));
		return;
	}

	if (r_visited.has(p_path.simplify_path())) {
		return;
	}
	r_visited.insert(p_path.simplify_path());

	if (FileAccess::exists(p_path.path_join(".gdignore"))) {
		// The editor filesystem skips this directory, so its scripts are not part
		// of the project and their traits must not be registered.
		return;
	}

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
			if (dir->is_link(full_path)) {
				// A link may point to an ancestor of this directory (or to itself),
				// which no visited set can detect through the changing paths.
				continue;
			}
			_scan_traits_in_dir(full_path, r_visited, p_depth + 1, r_found);
		} else if (name.get_extension().to_lower() == "gd") {
			_register_traits_in_file(full_path, &r_found);
		}
	}
	dir->list_dir_end();
}

void gdscript_scan_project_traits() {
	if (GDScriptLanguage::get_singleton() == nullptr) {
		return;
	}

	HashSet<String> visited;
	HashSet<StringName> found;
	_scan_traits_in_dir("res://", visited, 0, found);

	// The registry only ever grows while scripts are parsed, so a trait whose
	// file was deleted or renamed would keep answering `is` and keep showing up
	// in `uses` completion. This scan just read the whole project, so any trait
	// of a project script that it did not find is gone.
	List<StringName> registered;
	GDScriptLanguage::get_singleton()->get_global_trait_names(&registered);
	for (const StringName &name : registered) {
		if (found.has(name)) {
			continue;
		}
		if (GDScriptLanguage::get_global_trait_path(name).begins_with("res://")) {
			GDScriptLanguage::unregister_global_trait(name);
		}
	}

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
