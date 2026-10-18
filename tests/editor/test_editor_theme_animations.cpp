/**************************************************************************/
/*  test_editor_theme_animations.cpp                                      */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_editor_theme_animations)

#include "editor/themes/editor_theme_manager.h"

namespace TestEditorThemeAnimations {

using AnimationKind = EditorThemeManager::AnimationKind;
using AnimationParameters = EditorThemeManager::AnimationParameters;
using AnimationStyle = EditorThemeManager::AnimationStyle;

// The Subtle table is the defaults that shipped before the Expressive style existed. It must stay
// byte-for-byte identical, so the full table is pinned here. These values match the v1 probe baseline
// captured before the Expressive amplification (see .workbuddy/anim/implementation-notes-amplify.md,
// values unchanged since the expressive style was introduced).
struct SubtleBaselineEntry {
	AnimationKind kind;
	double duration;
	Tween::TransitionType transition;
	bool animate_rect;
	double exit_duration;
	Tween::TransitionType exit_transition;
};

const SubtleBaselineEntry subtle_baseline[] = {
	{ EditorThemeManager::ANIM_BUTTON_NORMAL, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_BUTTON_HOVER, 0.12, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_BUTTON_PRESSED, 0.08, Tween::TRANS_QUAD, false, 0.0, Tween::TRANS_QUAD },
	{ EditorThemeManager::ANIM_BUTTON_DISABLED, 0.12, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_FOCUS_RING, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_FLAT_HOVER, 0.12, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_FLAT_PRESSED, 0.08, Tween::TRANS_QUAD, false, 0.10, Tween::TRANS_QUAD },
	{ EditorThemeManager::ANIM_FLAT_HOVER_PRESSED, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_HOVER, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_SELECTED, 0.12, Tween::TRANS_SINE, false, 0.14, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_HOVERED_SELECTED, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_CURSOR, 0.10, Tween::TRANS_SINE, false, 0.08, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_FOCUS, 0.12, Tween::TRANS_SINE, false, 0.14, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_ITEMLIST_HOVER, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_ITEMLIST_HOVER_SELECTED, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_ITEMLIST_CURSOR, 0.10, Tween::TRANS_SINE, false, 0.08, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TAB_STATE, 0.12, Tween::TRANS_SINE, true, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TAB_SMALL_BUTTON, 0.08, Tween::TRANS_SINE, false, 0.08, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_MENU_HOVER, 0.10, Tween::TRANS_SINE, false, 0.08, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_GRABBER, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_SLIDER_FILL, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_SPINBOX_HOVER, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_SPINBOX_PRESSED, 0.08, Tween::TRANS_QUAD, false, 0.0, Tween::TRANS_QUAD },
};

TEST_CASE("[Editor][Theme] Subtle animation table matches the v1 baseline") {
	// Every kind must be pinned exactly once; a new kind cannot silently fall back to zero animations.
	for (int kind = 0; kind < EditorThemeManager::ANIM_KIND_MAX; kind++) {
		int matches = 0;
		for (const SubtleBaselineEntry &entry : subtle_baseline) {
			if (int(entry.kind) == kind) {
				matches++;
			}
		}
		CHECK_MESSAGE(matches == 1, "Subtle baseline must cover animation kind ", kind, " exactly once.");
	}

	for (const SubtleBaselineEntry &entry : subtle_baseline) {
		INFO("kind: ", int(entry.kind));
		const AnimationParameters parameters = EditorThemeManager::get_animation_parameters(entry.kind, EditorThemeManager::ANIMATION_STYLE_SUBTLE);
		CHECK(parameters.duration == doctest::Approx(entry.duration));
		CHECK(parameters.transition == entry.transition);
		CHECK(parameters.animate_rect == entry.animate_rect);
		CHECK(parameters.exit_duration == doctest::Approx(entry.exit_duration));
		CHECK(parameters.exit_transition == entry.exit_transition);
		CHECK(parameters.fade == false);
		CHECK(parameters.offset.is_zero_approx());
		CHECK(parameters.scale.is_equal_approx(Size2(1, 1)));
		CHECK(parameters.rotation == 0.0);
	}
}

TEST_CASE("[Editor][Theme] Animation tables keep the perceptibility budget") {
	// Budget from animation-spec-amplify.md, applied to every kind in both styles. These are regression
	// guards: growing any animation past these limits (or adding rotation/bounce) has to be a deliberate
	// change that updates this test together with the spec.
	for (int style_index = 0; style_index < 2; style_index++) {
		const AnimationStyle style = style_index == 0 ? EditorThemeManager::ANIMATION_STYLE_SUBTLE : EditorThemeManager::ANIMATION_STYLE_EXPRESSIVE;
		for (int kind = 0; kind < EditorThemeManager::ANIM_KIND_MAX; kind++) {
			INFO("style: ", style_index, ", kind: ", kind);
			const AnimationParameters parameters = EditorThemeManager::get_animation_parameters(AnimationKind(kind), style);

			CHECK(parameters.duration > 0.0);
			CHECK(parameters.duration <= 0.24);
			CHECK(parameters.exit_duration <= 0.16);

			CHECK(parameters.offset.x >= -10.0f);
			CHECK(parameters.offset.x <= 10.0f);
			CHECK(parameters.offset.y >= -10.0f);
			CHECK(parameters.offset.y <= 10.0f);

			CHECK(parameters.scale.x >= 0.82f);
			CHECK(parameters.scale.x <= 1.06f);
			CHECK(parameters.scale.y >= 0.82f);
			CHECK(parameters.scale.y <= 1.06f);

			CHECK(parameters.transition != Tween::TRANS_BOUNCE);
			CHECK(parameters.exit_transition != Tween::TRANS_BOUNCE);
			if (parameters.transition == Tween::TRANS_ELASTIC) {
				CHECK(kind == EditorThemeManager::ANIM_FLAT_HOVER);
			}
			if (parameters.exit_transition == Tween::TRANS_ELASTIC) {
				CHECK(kind == EditorThemeManager::ANIM_FLAT_HOVER);
			}

			CHECK(parameters.rotation == 0.0);

			// Animating the whole rect makes text and icons slide with the box. Only the tab backgrounds use
			// it; every other kind must keep the box anchored to its layout position.
			const bool expects_rect_animation = kind == EditorThemeManager::ANIM_TAB_STATE;
			CHECK(parameters.animate_rect == expects_rect_animation);
		}
	}
}

} // namespace TestEditorThemeAnimations
