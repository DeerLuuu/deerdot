/**************************************************************************/
/*  snippet_parser.h                                                      */
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

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"

// A tab stop parsed out of a VSCode-style snippet body. Positions refer to
// the expanded text returned by SnippetParser::parse().
struct SnippetTabStop {
	int number = 0; // 0 = the final cursor position ($0), 1..n = order of definition.
	int start_line = 0;
	int start_col = 0;
	int end_line = 0;
	int end_col = 0; // Same as start for zero-width stops (no placeholder).
};

// Parses VSCode-style snippet syntax into plain expanded text plus the tab
// stop positions within it. Supported syntax:
//
//   $0, $1 ... $n        zero-width tab stops
//   ${1:placeholder}     tab stop with default text
//   ${1|a,b,c|}          choice tab stop (the first choice is the default text)
//   $name, ${name:dflt}  variables, resolved through the passed map
//   $$, \$, \{, \}       escaped literals
//
// Limitations of this first version: placeholders are taken literally
// (nested tab stops inside a placeholder are not parsed) and repeated stop
// numbers are emitted as separate stops visited one after another.
//
// This is plain text logic with no editor dependencies: any consumer
// (snippets, script templates, quick fixes, other language modules) can
// use it.
namespace SnippetParser {

// Returns the expanded body. r_tabstops receives the stops in order of
// appearance; variables missing from p_variables resolve to their default
// text (after ':') or an empty string.
String parse(const String &p_body, const HashMap<String, String> &p_variables, List<SnippetTabStop> &r_tabstops);

} // namespace SnippetParser
